// 该文件实现了DBimpl类，负责数据库核心功能，包括读写操作、内存表压缩、清单文件(manifest)管理等，维护数据一致性与持久化
#include "db_impl.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>



#include "option.h"
#include "builder.h"
#include "random_access_file.h"
#include "table_cache.h"
#include "wal.h"

namespace koishidb
{
    namespace
    {
        const char *kWalFileName = "wal.log";
    }

    // DBimpl的内部结构体，用于管理写批量操作的上下文
    struct DBimpl::Writer
    {
        // 构造函数：初始化写批量对象，标记未完成状态
        Writer(WriteBatch *batch) : batch(batch)
        {
            done = false;
        }

        // 禁用拷贝构造函数（condition_variable 不可拷贝）
        Writer(const Writer &that) = delete;

        WriteBatch *batch;          // 指向写批量对象的指针

        bool done;                  // 标记当前写任务是否完成
        std::condition_variable cv; // 条件变量，用于等待任务完成通知
    };

    // manifest文件格式说明：
    // 首先存储文件元信息数量、最后序列号， followed by 各文件元信息的编码数据

    // 构造函数：初始化数据库状态，读取manifest文件
    DBimpl::DBimpl(bool enable_wal)
        : memtable_(new Memtable()),
          immutable_memtable_(nullptr),
          cmp_(new InternalKeyComparator()),
          table_cache_(new TableCache(128)),//布隆过滤器的大小为128
          background_compaction_schedule_(false),
          shutting_down_(false),
          wal_writer_(nullptr),
          wal_enabled_(enable_wal)
    {
        LOG_INFO("DBimpl构造函数启动");
        // 检查manifest文件是否存在
        if (access("./manifest", F_OK) == -1) // access检查文件是否存在或测试文件的访问权限
        {
            LOG_INFO("manifest不存在");
            // 不存在manifest文件，初始化SSTable编号和最后序列号
            this->sstable_number_ = 1;
            this->last_sequence_ = 1;
        }
        else
        {
            LOG_INFO("manifest存在");
            // 存在manifest文件，读取其中的元数据
            Status s = ReadManifest();
            if (!s.ok())
            {
                LOG_ERROR("[DBimpl] ReadManifest failed: %s", s.ToString().c_str());
            }
        }

        if (wal_enabled_)
        {
            Status recover_status = RecoverWal();
            if (!recover_status.ok())
            {
                LOG_ERROR("[DBimpl] RecoverWal failed: %s", recover_status.ToString().c_str());
            }

            Status open_wal_status = OpenWalForAppend();
            if (!open_wal_status.ok())
            {
                LOG_ERROR("[DBimpl] OpenWalForAppend failed: %s", open_wal_status.ToString().c_str());
            }
        }
        else
        {
            LOG_WARN("[DBimpl] WAL disabled, writes will not survive process crash before memtable flush");
        }
    }

    // 析构函数：释放资源，确保后台压缩完成，持久化manifest
    DBimpl::~DBimpl()
    {
        LOG_INFO("[DBimpl] destructor called");
        
        // 标记数据库正在关闭
        this->shutting_down_.store(true, std::memory_order_release);
        
        // 唤醒所有可能在等待的线程
        {
            std::lock_guard<std::mutex> lock(this->cv_lock_);
            this->background_work_finish_signal_.notify_all();
        }
        
        // 获取独占锁，确保线程安全
        this->rwlock_.lock();
        
        // 唤醒所有等待的写任务，让它们检查 shutting_down_ 标志并退出
        for (auto writer : writers_)
        {
            writer->cv.notify_one();
        }
        
        // 等待后台压缩任务完成
        while (this->background_compaction_schedule_ == true)
        {
            std::unique_lock<std::mutex> lock(this->cv_lock_);
            this->rwlock_.unlock();
            this->background_work_finish_signal_.wait(lock);
            this->rwlock_.lock();
        }
        this->rwlock_.unlock();

        // 若内存表非空，先将其转为不可变内存表并压缩到磁盘
        if (memtable_ != nullptr)
        {
            immutable_memtable_ = memtable_;
            Status compact_status = CompactMemtable();
            immutable_memtable_ = nullptr;
            delete memtable_;
            memtable_ = nullptr;
            if (!compact_status.ok())
            {
                LOG_ERROR("[DBimpl] CompactMemtable on close failed: %s", compact_status.ToString().c_str());
            }
        }

        // 最后持久化manifest文件；成功后清理 WAL，避免下次重复重放
        Status manifest_status = DumpManifest();
        if (manifest_status.ok())
        {
            if (wal_enabled_)
            {
                ResetWalAfterCleanShutdown();
            }
            else
            {
                delete wal_writer_;
                wal_writer_ = nullptr;
            }
        }
        else
        {
            LOG_ERROR("[DBimpl] DumpManifest failed: %s", manifest_status.ToString().c_str());
            delete wal_writer_;
            wal_writer_ = nullptr;
        }

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

        // manifest 会随 SSTable 数量增长，不能使用固定 4KB 栈缓冲区读取
        std::string manifest_content;
        char buf[4096];
        while (true)
        {
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0)
            {
                manifest_content.append(buf, static_cast<size_t>(n));
                continue;
            }
            if (n == 0)
            {
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }
            ::close(fd);
            return Status::IOError("read manifest error");
        }
        ::close(fd); // 关闭文件描述符

        // 解析缓冲区数据
        Slice slice(manifest_content);
        uint32_t file_meta_count;
        if (slice.empty())
        {
            return Status::Corruption("bad manifest file meta count");
        }
        GetVarint32(&slice, &file_meta_count);

        uint64_t last_sequence;

        if (!GetVarint64(&slice, &last_sequence))
        {
            return Status::Corruption("bad manifest last sequence");
        }
        this->last_sequence_ = last_sequence;

        uint64_t max_file_number = 0;
        // 读取每个文件元信息并存储
        for (uint32_t i = 0; i < file_meta_count; i++)
        {
            FileMeta *file_meta;
            DecodeFileMeta(&file_meta, &slice);
            max_file_number = std::max(max_file_number, file_meta->number);
            this->file_metas_.push_back(file_meta);
        }

        // 设置下一个 SSTable 编号
        this->sstable_number_ = max_file_number + 1;

        return Status::OK();
    }


    // 持久化manifest文件，保存当前数据库元数据
    Status DBimpl::DumpManifest()
    {
        // 打开manifest文件（只写、创建模式,覆盖元文件,而非追加，权限0777）
        int fd = ::open("manifest", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd < 0)
        {
            LOG_ERROR("[DumpManifest] open failed: errno=%d, %s", errno, strerror(errno));
            return Status::IOError("open manifest failed", std::strerror(errno));
        }
        LOG_INFO("[DumpManifest] open manifest fd=%d", fd);
        WritableFile file(fd, "manifest"); // 创建可写文件对象

        // 写入文件元信息数量和最后序列号
        std::string rep;
        PutVarint32(this->file_metas_.size(), &rep);
        PutVarint64(&rep, this->last_sequence_);
        Status s = file.Append(Slice(rep.data(), rep.size()));
        if (!s.ok())
        {
            return s;
        }

        // 写入每个文件元信息
        for (int i = 0; i < this->file_metas_.size(); i++)
        {
            PrintFileMeta(*this->file_metas_[i]);       // 打印元信息（调试用）
            EncodeFileMeta(this->file_metas_[i], file); // 编码并写入
        }
        return file.Sync();
    }

    // 启动时重放 WAL，恢复尚未写入 SSTable/manifest 的批量写入
    Status DBimpl::RecoverWal()
    {
        WalReader reader(kWalFileName);
        if (!reader.status().ok())
        {
            return reader.status();
        }

        std::string record;
        Status read_status;
        while (reader.ReadRecord(&record, &read_status))
        {
            WriteBatch batch;
            Status batch_status = WriteBatchInternal::SetContents(&batch, Slice(record.data(), record.size()));
            if (!batch_status.ok())
            {
                LOG_ERROR("[RecoverWal] bad write batch: %s", batch_status.ToString().c_str());
                break;
            }

            SequenceNumber batch_begin = WriteBatchInternal::Sequence(&batch);
            SequenceNumber batch_end = batch_begin + WriteBatchInternal::Count(&batch);
            if (batch_end <= this->last_sequence_)
            {
                continue; // manifest 已覆盖该 batch，无需重放
            }

            batch.InsertAll(memtable_);
            this->last_sequence_ = std::max(this->last_sequence_, batch_end);

            if (memtable_->EstimatedSize() >= kMaxMemtableSize)
            {
                immutable_memtable_ = memtable_;
                memtable_ = new Memtable();
                Status compact_status = CompactMemtable();
                delete immutable_memtable_;
                immutable_memtable_ = nullptr;
                if (!compact_status.ok())
                {
                    return compact_status;
                }
            }
        }

        if (!read_status.ok())
        {
            LOG_ERROR("[RecoverWal] stop at bad WAL tail: %s", read_status.ToString().c_str());
        }

        // 丢弃损坏或半条尾部，避免后续追加在坏尾部之后导致下次恢复中断。
        Status truncate_status = TruncateWal(kWalFileName, reader.LastValidOffset());
        if (!truncate_status.ok())
        {
            return truncate_status;
        }
        return Status::OK();
    }

    Status DBimpl::OpenWalForAppend()
    {
        if (!wal_enabled_)
        {
            return Status::OK();
        }
        delete wal_writer_;
        wal_writer_ = new WalWriter(kWalFileName);
        Status s = wal_writer_->status();
        if (!s.ok())
        {
            delete wal_writer_;
            wal_writer_ = nullptr;
        }
        return s;
    }

    Status DBimpl::AppendToWal(WriteBatch *batch)
    {
        if (!wal_enabled_)
        {
            return Status::OK();
        }
        if (wal_writer_ == nullptr)
        {
            Status s = OpenWalForAppend();
            if (!s.ok())
            {
                return s;
            }
        }
        return wal_writer_->AddRecord(WriteBatchInternal::Contents(batch));
    }

    void DBimpl::ResetWalAfterCleanShutdown()
    {
        delete wal_writer_;
        wal_writer_ = nullptr;
        Status s = RemoveWal(kWalFileName);
        if (!s.ok())
        {
            LOG_ERROR("[ResetWalAfterCleanShutdown] remove WAL failed: %s", s.ToString().c_str());
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
        // 检查数据库是否正在关闭
        if (shutting_down_.load(std::memory_order_acquire))
        {
            return;
        }
        
        Writer w(update); // 创建写任务对象

        // 获取独占锁（写锁）
        std::unique_lock<std::shared_mutex> lock(rwlock_);
        writers_.push_back(&w); // 将任务加入写队列

        // 当前任务不是队列头部时需等待（保证FIFO顺序）
        while (!w.done && &w != writers_.front())
        {
            // 检查数据库是否正在关闭
            if (shutting_down_.load(std::memory_order_acquire))
            {
                // 从队列中移除自己
                writers_.erase(std::remove(writers_.begin(), writers_.end(), &w), writers_.end());
                return;
            }
            // 释放写锁，让其他线程可以工作
            lock.unlock();
            
            // 短暂等待，使用带超时的条件变量
            {
                std::unique_lock<std::mutex> cv_lk(cv_lock_);
                w.cv.wait_for(cv_lk, std::chrono::milliseconds(10));
            }
            
            lock.lock();
        }

        // 任务已完成，直接返回
        if (w.done)
        {
            return;
        }
        
        // 再次检查是否正在关闭
        if (shutting_down_.load(std::memory_order_acquire))
        {
            // 唤醒所有等待的写任务
            while (!writers_.empty())
            {
                Writer *ready = writers_.front();
                writers_.pop_front();
                if (ready != &w)
                {
                    ready->done = true;
                    ready->cv.notify_one();
                }
            }
            return;
        }

        MakeRoomForWrite(lock); // 确保有空间写入（必要时触发压缩）

        Writer *last_writer = &w;

        // 写批量合并优化:将队列中连续的多个WriteBatch合并为一个（减少锁冲突）
        WriteBatch *writer_batch = BuildWriteBatchGroup(&last_writer);

        // 设置批处理的序列号。WAL 必须保存带确定 sequence 的 batch。
        // 每个操作分配唯一递增的sequence_number
        const int batch_count = WriteBatchInternal::Count(writer_batch);
        WriteBatchInternal::SetSequence(writer_batch, last_sequence_);

        // 先写 WAL 并同步，再写入 memtable，避免崩溃后出现无法恢复的内存可见写入。
        Status wal_status = AppendToWal(writer_batch);
        if (!wal_status.ok())
        {
            LOG_ERROR("[Write] append WAL failed: %s", wal_status.ToString().c_str());
        }
        else
        {
            last_sequence_ += batch_count;
            // 将批处理中的所有put和delete操作插入内存表
            writer_batch->InsertAll(memtable_);
        }

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
    // 在查磁盘SSTable中,为了优化查询,有两层保障,第1层是记录每个存储文件的最小键和最大键,如果要查的件不在这个范围内,就直接跳过,第2层则是使用布隆过滤器
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
            delete[] memtable_key.data(); // 释放动态分配的键内存(CreateMemtableKey中new出来的)
            return true;
        }
        // 再查不可变内存表
        else if (immutable_memtable_ != nullptr && imm->Get(memtable_key, value))
        {
            delete[] memtable_key.data();
            return true;
        }

        delete[] memtable_key.data(); // 释放内存表查询键
        // 在内存表中的查询结束,接下来开始查硬盘

        // 构建用于SSTable查询的内部键
        Slice internal_key = CreateInternalKey(key, UINT64_MAX, kTypeSeek);

        // 查磁盘上的SSTable。L0 文件可能范围重叠，必须从最新文件向旧文件查找。
        for (auto iter = this->file_metas_.rbegin(); iter != this->file_metas_.rend(); ++iter)
        {
            auto file_meta = *iter;
            if (file_meta == nullptr || file_meta->file_size < kFixedFooterSize ||
                !file_meta->smallest_key.Valid() || !file_meta->largest_key.Valid())
            {
                continue;
            }

            Slice smallest_user_key, largest_user_key;
            SequenceNumber tmp_sequence;
            ExtractUserKey(file_meta->smallest_key.ToSlice(), &smallest_user_key, &tmp_sequence);
            ExtractUserKey(file_meta->largest_key.ToSlice(), &largest_user_key, &tmp_sequence);
            if (smallest_user_key.Compare(key) > 0 || largest_user_key.Compare(key) < 0)
            {
                continue;
            }

            // 从缓存获取 SSTable；首次访问才读取 Footer、索引块和过滤器块。
            std::string file_name = "sstable_" + std::to_string(file_meta->number);
            std::shared_ptr<SSTable> table = table_cache_->FindTable(
                file_meta->number, file_name, file_meta->file_size);
            if (!table)
            {
                continue;
            }
            auto result = table->InternalGet(internal_key);

            if (!result.has_value())
            {
                continue; // 未找到，继续查下一个SSTable
            }

            *value = *result; // 找到值，赋值并返回
            delete[] internal_key.data();
            return true;
        }


        delete[] internal_key.data(); // 释放内部键内存
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
        if (immutable_memtable_ == nullptr)
        {
            return Status::OK();
        }

        // 生成新SSTable文件名
        const uint64_t file_number = sstable_number_;
        std::string new_sstable_name = "sstable_" + std::to_string(file_number);
        FileMeta *new_file_meta = new FileMeta(); // 创建新文件元信息
        new_file_meta->number = file_number;
        new_file_meta->file_size = 0;
        
        // 创建迭代器并构建SSTable
        Iterator *iter = this->immutable_memtable_->NewIterator();
        Status s = BuildTable(new_sstable_name, new_file_meta, iter);
        delete iter; // 释放迭代器，防止内存泄漏

        if (!s.ok())
        {
            delete new_file_meta;
            return s;
        }

        // 空 memtable 不会生成 SSTable，不能把未初始化的 FileMeta 放进元数据列表
        if (new_file_meta->file_size == 0)
        {
            delete new_file_meta;
            return Status::OK();
        }

        this->file_metas_.push_back(new_file_meta);
        sstable_number_++;
        return Status::OK();
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
        
        {
            std::lock_guard<std::mutex> lock(cv_lock_);
            background_compaction_schedule_ = false;     // 标记压缩任务已完成
            background_work_finish_signal_.notify_all(); // 通知等待的线程
        }
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
            // 先标记压缩任务已调度，防止重复调度
            background_compaction_schedule_ = true;
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
    void DBimpl::MakeRoomForWrite(std::unique_lock<std::shared_mutex> &lock)
    {
        while (true)
        {
            // 检查数据库是否正在关闭
            if (shutting_down_.load(std::memory_order_acquire))
            {
                return;
            }
            // 内存表未达最大容量，可直接写入
            if (memtable_->EstimatedSize() < kMaxMemtableSize)
            {
                return;
            }
            // 存在不可变内存表，等待压缩完成
            else if (immutable_memtable_ != nullptr)
            {
                if (!background_compaction_schedule_)
                {
                    MaybeScheduleCompaction();
                }

                // 释放写锁，让后台压缩线程可以获取锁
                lock.unlock();
                {
                    std::unique_lock<std::mutex> cv_lock(cv_lock_);
                    background_work_finish_signal_.wait_for(cv_lock, std::chrono::milliseconds(10));
                }
                // 重新获取写锁
                lock.lock();
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