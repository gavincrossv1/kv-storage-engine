// 该文件声明了DBimpl类，作为数据库的具体实现，负责管理内存表、磁盘存储及后台压缩等核心功能
#ifndef KOISHIDB_SRC_INCLUDE_DB_IMPL_H
#define KOISHIDB_SRC_INCLUDE_DB_IMPL_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <shared_mutex>

#include "status.h"

#include "db.h"
#include "memtable.h"

namespace koishidb
{
    // 前向声明文件元信息结构体
    struct FileMeta;
    class WalWriter;

    // 数据库实现类，继承自DB接口，负责数据库存储部分的调度与管理
    class DBimpl : public DB
    {
    private:
        // 前向声明写任务结构体
        struct Writer;

    public:
        // 构造函数：初始化数据库状态
        explicit DBimpl(bool enable_wal = true);

        // 析构函数：释放资源，确保数据安全
        ~DBimpl();

        // 插入键值对
        void Put(const Slice &key, const Slice &value);

        // 查询键对应的值，找到返回true，否则返回false
        bool Get(const Slice &key, std::string *value);

        // 删除指定键
        void Delete(const Slice &key);

        // 处理批量写操作
        void Write(WriteBatch *update);

        // 当有新的写任务时，调用此函数确保有足够空间写入
        void MakeRoomForWrite(std::unique_lock<std::shared_mutex> &lock);


        // 可能调度后台压缩任务
        void MaybeScheduleCompaction();

        // 后台压缩的回调函数
        void BackGroundCall();

        // 后台压缩的核心逻辑
        void BackGroundCompaction();

        // 将不可变内存表压缩为SSTable并写入磁盘
        Status CompactMemtable();

        // 构建写批量组，合并多个写任务
        WriteBatch *BuildWriteBatchGroup(Writer **last_writer);

        // 读取manifest文件，恢复数据库元数据
        Status ReadManifest();

        // 持久化manifest文件，保存当前数据库元数据
        Status DumpManifest();

        // 启动时重放 WAL，恢复尚未落盘的写入
        Status RecoverWal();

        // 打开 WAL，后续写入追加日志
        Status OpenWalForAppend();

        // 将批量写入追加到 WAL
        Status AppendToWal(WriteBatch *batch);

        // 正常关闭并持久化 manifest 后清理 WAL
        void ResetWalAfterCleanShutdown();

    private:
        std::deque<Writer *> writers_;                          // 写任务队列，存储待处理的写操作
        std::shared_mutex rwlock_;                              // 读写锁，支持多读者单写者模式
        std::mutex cv_lock_;                                    // 条件变量专用锁
        Memtable *memtable_;                                    // 当前可写的内存表
        Memtable *immutable_memtable_;                          // 不可变内存表，等待压缩到磁盘
        std::condition_variable background_work_finish_signal_; // 用于通知后台压缩任务完成的条件变量
        bool background_compaction_schedule_;                   // 标记是否已调度后台压缩任务
        SequenceNumber last_sequence_;                          // 最后使用的序列号，用于保证操作顺序
        std::atomic<bool> shutting_down_;                       // 标记数据库是否正在关闭
        std::vector<FileMeta *> file_metas_;                    // 存储所有SSTable文件的元信息
        uint64_t sstable_number_;                               // 下一个可用的SSTable编号
        WalWriter *wal_writer_;                                 // 当前 WAL 写入器
        bool wal_enabled_;                                      // 是否启用 WAL
        Comparator *cmp_;                                       // 比较器（实际为内部键比较器）
    };
}; // namespace koishidb

#endif

//核心的数据库实现定义(manifest文件用来存储每个文件的FileMeta)