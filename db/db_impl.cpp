// 该文件实现了DBimpl类，负责数据库核心功能，包括读写操作、内存表压缩、清单文件(manifest)管理等，维护数据一致性与持久化
#include "db_impl.h"

#include <fcntl.h>
#include <unistd.h>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "option.h"
#include "builder.h"
#include "random_access_file.h"

namespace koishidb
{

    // DBimpl的内部结构体，用于管理写批量操作的上下文
    struct DBimpl::Writer
    {
        // 构造函数：初始化写批量对象，标记未完成状态
        Writer(WriteBatch *batch) : batch(batch)
        {
            done = false;
        }

        // 拷贝构造函数：默认实现
        Writer(const Writer &that) = default;

        WriteBatch *batch;          // 指向写批量对象的指针
        bool done;                  // 标记当前写任务是否完成
        std::condition_variable cv; // 条件变量，用于等待任务完成通知
    };

    // manifest文件格式说明：
    // 首先存储文件元信息数量、最后序列号， followed by 各文件元信息的编码数据

    // 构造函数：初始化数据库状态，读取manifest文件
    DBimpl::DBimpl()
        : memtable_(new Memtable()),
          immutable_memtable_(nullptr),
          cmp_(new InternalKeyComparator())
    {
        LOG_INFO("DBimpl call");
        // 检查manifest文件是否存在
        if (access("./manifest", F_OK) == -1) // access检查文件是否存在或测试文件的访问权限
        {
            // 不存在manifest文件，初始化SSTable编号和最后序列号
            this->sstable_number_ = 1;
            this->last_sequence_ = 1;
        }
        else
        {
            // 存在manifest文件，读取其中的元数据
            ReadManifest();
        }
    }

    // 析构函数：释放资源，确保后台压缩完成，持久化manifest
    DBimpl::~DBimpl()
    {
        LOG_INFO("[DBimpl] destructor called");
        // 获取独占锁，确保线程安全
        this->rwlock_.lock();
        // 标记数据库正在关闭
        this->shutting_down_.store(true, std::memory_order_release);
        // 等待后台压缩任务完成
        while (this->background_compaction_schedule_ == true)
        {
            std::unique_lock<std::mutex> lock(this->cv_lock_, std::adopt_lock);
            this->background_work_finish_signal_.wait(lock);
        }
        this->rwlock_.unlock();

        // 若内存表非空，先将其转为不可变内存表并压缩到磁盘
        if (memtable_ != nullptr)
        {
            immutable_memtable_ = memtable_;
            CompactMemtable();
            immutable_memtable_ = nullptr;
            delete memtable_;
        }

        // 最后持久化manifest文件
        DumpManifest();

        // 断言不可变内存表已释放
        assert(immutable_memtable_ == nullptr);

        // 释放所有文件元信息
        for (int i = 0; i < this->file_metas_.size(); i++)
        {
            delete this->file_metas_[i];
        }

        // 释放比较器
        delete cmp_;
    }

    // 读取manifest文件，恢复数据库元数据（SSTable信息、序列号等）
    // manifest格式：
    // 文件元信息数量(varint32) -> 最后序列号(varint64) -> 各文件元信息编码数据...
    Status DBimpl::ReadManifest()
    {
        // 打开manifest文件（只读模式）
        int fd = ::open("manifest", O_RDONLY);
        if (fd == -1)
        {
            LOG_ERROR("manifest corruption");
            return Status::Corruption("manifest corruption");
        }

        // 读取文件内容到缓冲区
        char buf[kManifestReserved];
        int ptr = 0;
        int n;
        while ((n = ::read(fd, buf + ptr, sizeof(buf))) > 0)
        {
            ptr += n;
        }

        // 解析缓冲区数据
        Slice slice(buf, ptr);
        uint32_t file_meta_count;
        GetVarint32(&slice, &file_meta_count); // 读取文件元信息数量

        uint64_t last_sequence;
        GetVarint64(&slice, &last_sequence); // 读取最后序列号
        this->last_sequence_ = last_sequence;

        // 读取每个文件元信息并存储
        for (int i = 0; i < file_meta_count; i++)
        {
            FileMeta *file_meta;
            DecodeFileMeta(&file_meta, &slice);
            this->file_metas_.push_back(file_meta);
        }

        // 设置SSTable编号（与文件元信息数量一致）
        this->sstable_number_ = file_meta_count;
        ::close(fd); // 关闭文件描述符

        return Status::OK();
    }

    // 持久化manifest文件，保存当前数据库元数据
    void DBimpl::DumpManifest()
    {
        LOG_INFO("[DumpManifest] called");
        // 打开manifest文件（只写、创建模式，权限0777）
        int fd = ::open("manifest", O_WRONLY | O_CREAT, 0777);
        if (fd < 0) {
            LOG_ERROR("[DumpManifest] open failed: errno=%d, %s", errno, strerror(errno));
            return;
        }
        LOG_INFO("[DumpManifest] open manifest fd=%d", fd);
        WritableFile file(fd, "manifest"); // 创建可写文件对象

        // 写入文件元信息数量和最后序列号
        std::string rep;
        PutVarint32(this->file_metas_.size(), &rep);
        PutVarint64(&rep, this->last_sequence_);
        file.Append(rep.data());
        file.Flush(); // 刷新缓冲区到磁盘

        // 写入每个文件元信息
        for (int i = 0; i < this->file_metas_.size(); i++)
        {
            PrintFileMeta(*this->file_metas_[i]);       // 打印元信息（调试用）
            EncodeFileMeta(this->file_metas_[i], file); // 编码并写入
        }
    }

    // 插入键值对（通过WriteBatch实现）
    void DBimpl::Put(const Slice &key, const Slice &value)
    {
        WriteBatch write_batch;
        write_batch.Put(key, value); // 将Put操作加入批处理
        return Write(&write_batch);  // 执行批处理
    }

    // 删除键（通过WriteBatch实现）
    void DBimpl::Delete(const Slice &key)
    {
        WriteBatch write_batch;
        write_batch.Delete(key);    // 将Delete操作加入批处理
        return Write(&write_batch); // 执行批处理
    }

    // 处理写批量操作，线程安全
    void DBimpl::Write(WriteBatch *update)
    {
        Writer w(update); // 创建写任务对象

        // 获取独占锁（写锁）
        std::unique_lock<std::shared_mutex> lock(rwlock_);
        writers_.push_back(&w); // 将任务加入写队列

        // 当前任务不是队列头部时需等待（保证FIFO顺序）
        if (!w.done && &w != writers_.front())
        {
            // 避免死锁，释放条件变量锁后等待
            std::unique_lock<std::mutex> cv_lock(cv_lock_, std::adopt_lock);
            w.cv.wait(cv_lock);
            cv_lock.release();
        }
        cv_lock_.unlock(); // 释放条件变量锁

        // 任务已完成，直接返回
        if (w.done)
        {
            return;
        }

        MakeRoomForWrite(); // 确保有空间写入（必要时触发压缩）
        Writer *last_writer = &w;

        // 写批量合并优化:将队列中连续的多个WriteBatch合并为一个（减少锁冲突）
        WriteBatch *writer_batch = BuildWriteBatchGroup(&last_writer);

        // 设置批处理的序列号，更新最后序列号
        // 每个操作分配唯一递增的sequence_number
        WriteBatchInternal::SetSequence(writer_batch, last_sequence_);
        last_sequence_ += WriteBatchInternal::Count(writer_batch);

        // 将批处理中的所有put和delete操作插入内存表
        writer_batch->InsertAll(memtable_);

        // 唤醒所有被合并的写任务（从队列头到last_writer）,因为这些写批量任务都已经被合并并完成了
        while (true)
        {
            Writer *ready = writers_.front();
            writers_.pop_front();

            // 若不是当前任务，标记完成并通知
            if (ready != &w)
            {
                ready->done = true;
                ready->cv.notify_one();
            }

            // 到达最后一个任务，退出循环
            if (ready == last_writer)
                break;
        }

        // 唤醒下一个头部任务对应的线程(main.cpp中创建)
        if (!writers_.empty())
        {
            writers_.front()->cv.notify_one();
        }
    }

    // 读取键对应的值（先查内存表，再查磁盘SSTable）
    // 当前不支持事务和快照读
    //在查磁盘SSTable中,为了优化查询,有两层保障,第1层是记录每个存储文件的最小键和最大键,如果要查的件不在这个范围内,就直接跳过,第2层则是使用布隆过滤器
    bool DBimpl::Get(const Slice &key, std::string *value)
    {
        // 获取共享锁（读锁）
        std::shared_lock<std::shared_mutex> lock(rwlock_);

        // 创建用于内存表查询的键（包含最大序列号和查找类型）,使用最大的序列号是为了查找最新的值,因为序列号越大说明内存表现越小,查找的本质是查找第1个大于等于这个内存表键的内存表键,如果序列号给小了,查到的第1个大于等于该内存表键的内存表键可能就不是最新的了
        Slice memtable_key = CreateMemtableKey(key, UINT64_MAX, "", kTypeSeek);
        Memtable *mem = memtable_;
        Memtable *imm = nullptr;
        if (immutable_memtable_ != nullptr)
        {
            imm = immutable_memtable_;
        }

        // 先查当前内存表
        if (mem->Get(memtable_key, value))
        {
            delete memtable_key.data(); // 释放动态分配的键内存(CreateMemtableKey中new出来的)
            return true;
        }
        // 再查不可变内存表
        else if (immutable_memtable_ != nullptr && imm->Get(memtable_key, value))
        {
            delete memtable_key.data();
            return true;
        }

        delete memtable_key.data(); // 释放内存表查询键
        // 在内存表中的查询结束,接下来开始查硬盘

        // 构建用于SSTable查询的内部键
        Slice internal_key = CreateInternalKey(key, UINT64_MAX, kTypeSeek);

        // 查磁盘上的SSTable
        const Option *opt = new Option(new InternalKeyComparator());
        for (int i = 0; i < this->file_metas_.size(); i++)
        {
            auto file_meta = this->file_metas_[i];

            // 检查键是否在当前SSTable的键范围内
            if (this->cmp_->Compare(file_meta->smallest_key.ToSlice(), internal_key) == -1 &&
                this->cmp_->Compare(file_meta->largest_key.ToSlice(), internal_key) == 1)
            {
                // 打开SSTable文件
                std::string file_name = "sstable_" + std::to_string(file_meta->number);
                RandomAccessFile random_access_file(file_name);

                // 打开SSTable并查询
                auto sstable_opt = SSTable::Open(opt, &random_access_file, file_meta->file_size);
                assert(sstable_opt.has_value()); // 断言打开成功
                auto result = (*sstable_opt)->InternalGet(internal_key);

                if (!result.has_value())
                {
                    continue; // 未找到，继续查下一个SSTable
                }
                else
                {
                    *value = *result; // 找到值，赋值并返回
                    delete internal_key.data();
                    return true;
                }
            }
        }

        delete internal_key.data(); // 释放内部键内存
        return false;               // 未找到
    }

    // 构建写批量组，合并多个连续的写任务（前置条件：持有独占锁）
    WriteBatch *DBimpl::BuildWriteBatchGroup(Writer **last_writer)
    {
        Writer *first = writers_.front();
        WriteBatch *result = first->batch; // 以队列第一个任务为基础
        assert(result != nullptr);

        // 计算当前批处理大小
        size_t size = WriteBatchInternal::Bytes(result);

        // 限制批处理最大大小（小批量时允许适度增长）
        size_t max_size = 1 << 20; // 1MB
        if (size <= (128 << 10))   // 128KB
        {
            max_size = size + (128 << 10); // 允许增长128KB
        }

        *last_writer = first;
        std::deque<Writer *>::iterator iter = writers_.begin();
        ++iter; // 跳过第一个任务

        // 合并后续任务
        for (; iter != writers_.end(); ++iter)
        {
            Writer *w = *iter;
            if (w != nullptr)
            {
                size += WriteBatchInternal::Bytes(w->batch);
                if (size > max_size)
                {
                    // 超过最大大小，停止合并
                    break;
                }
                // 将任务追加到结果批处理中
                WriteBatchInternal::Append(result, w->batch);
            }
            *last_writer = w; // 更新最后一个任务
        }

        return result;
    }

    // 将不可变内存表压缩为SSTable写入磁盘（前置条件：持有独占锁）
    // TODO：处理错误情况
    Status DBimpl::CompactMemtable()
    {
        LOG_INFO("[CompactMemtable] called");
        // 生成新SSTable文件名
        std::string new_sstable_name = "sstable_" + std::to_string(sstable_number_);
        FileMeta *new_file_meta = new FileMeta(); // 创建新文件元信息
        this->file_metas_.push_back(new_file_meta);

        // 构建SSTable
        int fd = ::open(new_sstable_name.c_str(), O_WRONLY | O_CREAT, 0777);
        if (fd < 0) {
            LOG_ERROR("[CompactMemtable] open sstable failed: %s, errno=%d, %s", new_sstable_name.c_str(), errno, strerror(errno));
        } else {
            LOG_INFO("[CompactMemtable] open sstable %s fd=%d", new_sstable_name.c_str(), fd);
            ::close(fd); // 这里只是测试能否创建文件，实际BuildTable会重新创建
        }
        Status s = BuildTable(new_sstable_name, new_file_meta, this->immutable_memtable_->NewIterator());

        // 更新SSTable编号
        new_file_meta->number = sstable_number_;
        sstable_number_++;

        // 若失败，回滚编号
        if (!s.ok())
        {
            sstable_number_--;
            return s;
        }

        return s;
    }

    // 后台压缩逻辑（前置条件：持有独占锁）
    void DBimpl::BackGroundCompaction()
    {
        // 若存在不可变内存表，压缩它
        if (immutable_memtable_ != nullptr)
        {
            Status s = CompactMemtable();
            if (s.ok())
            {
                delete immutable_memtable_; // 压缩成功，释放不可变内存表
                immutable_memtable_ = nullptr;
            }
            return;
        }

        // 其他压缩条件（预留）
    }

    // 后台压缩回调（需先获取锁）
    void DBimpl::BackGroundCall()
    {
        // 获取独占锁
        std::unique_lock<std::shared_mutex> wlock(rwlock_);
        BackGroundCompaction();                      // 执行压缩
        background_compaction_schedule_ = false;     // 标记压缩任务已完成
        background_work_finish_signal_.notify_all(); // 通知等待的线程
    }

    // 可能调度压缩任务（前置条件：持有锁）
    void DBimpl::MaybeScheduleCompaction()
    {
        // 已调度压缩任务，直接返回
        if (background_compaction_schedule_ == true)
        {
            return;
        }
        // 存在不可变内存表，调度压缩任务
        else if (immutable_memtable_ != nullptr)
        {
            // 创建后台压缩线程并分离（不阻塞主线程）
            std::thread compaction_task([this]()
                                        { this->BackGroundCall(); });
            compaction_task.detach();
        }
        else
        {
            // 其他压缩场景（预留，后续实现磁盘层压缩）
        }
    }

    // 确保有空间写入数据（必要时触发压缩，前置条件：持有独占锁）
    void DBimpl::MakeRoomForWrite()
    {
        while (true)
        {
            // 内存表未达最大容量，可直接写入
            if (memtable_->EstimatedSize() < kMaxMemtableSize)
            {
                return;
            }
            // 存在不可变内存表，等待压缩完成
            else if (immutable_memtable_ != nullptr)
            {
                std::unique_lock<std::mutex> lock(cv_lock_, std::adopt_lock);
                background_work_finish_signal_.wait(lock); // 等待压缩完成通知
                lock.release();
            }
            // 内存表已满，转为不可变内存表，创建新内存表并调度压缩
            else
            {
                immutable_memtable_ = memtable_; // 当前内存表转为不可变
                memtable_ = new Memtable();      // 创建新内存表
                MaybeScheduleCompaction();       // 调度压缩任务
            }
        }
    }
}; // namespace koishidb

// 项目中最重要的部分,实现了数据库的核心功能,实现了不可变内存表到磁盘的压缩,但还未实现磁盘层的压缩(TODO)