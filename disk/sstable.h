// 该文件声明了SSTable类，定义了SSTable的接口，包括打开SSTable、创建迭代器、内部查询等功能
#ifndef KOISHIDB_SRC_INCLUDE_DISK_SSTABLE_H
#define KOISHIDB_SRC_INCLUDE_DISK_SSTABLE_H

#include <stddef.h>

#include <memory>
#include <optional>

#include "common.h"
#include "bloom_filter.h"
#include "iterator.h"

namespace koishidb
{
    struct BlockContent
    {
        std::unique_ptr<char[]> data;
        size_t size = 0;
    };

    class Option;
    class BlockCache;
    class RandomAccessFile;
    class Status;
    class BlockHandle;

    // SSTable类：存储排序的键值对，通过随机访问文件读取数据，支持多线程安全访问
    // 读取操作与TableBuilder的写入操作相对应
    class SSTable
    {
    public:
        // 打开SSTable文件并创建SSTable实例
        // 参数opt：数据库选项
        // 参数file：随机访问文件对象
        // 参数file_size：文件大小
        // 返回值：包含SSTable实例的optional（失败返回空）
        // 成功时：返回一个包含 SSTable* 的 optional。
        // 失败时：返回一个空的 optional（类似 std::nullopt）。
        static std::optional<SSTable *> Open(const Option *opt,
                             RandomAccessFile *file,
                             size_t file_size,
                             uint64_t file_number = 0,
                             BlockCache *block_cache = nullptr);

        // 禁用拷贝构造函数
        SSTable(const SSTable &that) = delete;

        // 禁用赋值运算符
        SSTable &operator=(const SSTable &that) = delete;

        // 析构函数：释放资源
        ~SSTable();

        // 创建SSTable的迭代器（用于遍历所有键值对）
        // 返回值：迭代器指针（需手动释放）
        Iterator *NewIterator() const;

        // 获取随机访问文件对象
        RandomAccessFile *random_access_file() const { return file_; }

        // 内部查询方法：根据键查找对应的值
        // 参数key：要查询的键
        // 返回值：包含值的optional（不存在或失败为空）
        std::optional<std::string> InternalGet(const Slice &key);

    private:
        // 内部数据结构（声明）
        struct Rep;

        // 构造函数：通过内部数据结构初始化
        explicit SSTable(Rep *rep): rep_(rep), bloom_filter_(kBloomFilterPerKeySize) 
        {

        }

        Rep *rep_;                 // 内部数据结构指针
        RandomAccessFile *file_;   // 随机访问文件对象（仅用于创建迭代器）
        BloomFilter bloom_filter_; // 布隆过滤器（用于快速判断键是否存在）
    };
}; // namespace koishidb

#endif

//定义了sstable的内存实例