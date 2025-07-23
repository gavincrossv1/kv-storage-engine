// 该文件定义了项目中各模块共用的常量、枚举类型和类型别名，提供基础的通用定义
#ifndef KOISHIDB_SRC_INCLUDE_COMMON_COMMON_H
#define KOISHIDB_SRC_INCLUDE_COMMON_COMMON_H

#include <cstddef>
#include <cstdint>

// 此文件定义可能在其他文件中使用的常量
namespace koishidb
{
    // 枚举类，表示键的类型，占用1字节
    enum class KeyType : uint8_t
    {
        kTypeValue = 0x01,    // 表示该键对应一个值（正常数据条目）
        kTypeDeletion = 0x02, // 表示该键是一个删除标记（墓碑条目）
    };

    // 类型别名，用于表示序列号(内部键的序列号)
    using SequenceNumber = uint64_t;

    // 用于Seek操作的默认键类型（使用kTypeValue作为查找基准）
    constexpr KeyType kTypeSeek = KeyType::kTypeValue;

    // 常量定义
    constexpr int kSkipListNodeMaxLevel = 20;       // 跳表节点的最大层级
    constexpr int kMemtableMaxSize = 4096;          // 内存表（mutable memtable）的最大大小
    constexpr int kWriteBatchHeader = 12;           // 写批处理（WriteBatch）的头部大小（字节）
    constexpr int kL0MaxFiles = 10;                 // L0层允许的最大文件数量（不过由于未实现磁盘层的压缩,这个暂未用到）
    constexpr int kMaxMemtableSize = 4096;          // 内存表的最大大小（与kMemtableMaxSize一致，可能用于兼容）
    constexpr int kBlockSize = 4096;                // 数据块（Block）的大小（字节）
    constexpr int kWritableFileBuffer = 65536;      // 可写文件（WritableFile）的缓冲区大小（字节）
    constexpr int kBloomFilterPerKeySize = 10;      // 布隆过滤器中每个键分配的位数
    constexpr int kFilterBaseLg = 11;               // 过滤器基数的对数（用于计算过滤器覆盖的键范围）
    constexpr int kFilterBase = 1 << kFilterBaseLg; // 每个布隆过滤器覆盖的字节范围(2048)//?
    constexpr int kFixedFooterSize = 40;            // SSTable文件固定的 Footer 大小（字节）
    constexpr int kManifestReserved = 4196;         // 清单文件（Manifest）预留的空间大小（字节）
}; // namespace koishidb

#endif

//对各种常量的定义