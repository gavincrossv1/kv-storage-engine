// 该文件定义了FilterBlockReader类，用于读取和解析SSTable中的过滤器块，通过布隆过滤器判断键是否可能存在于数据块中
#ifndef KOISHIDB_SRC_INCLUDE_DISK_FILTERBLOCK_READER_H
#define KOISHIDB_SRC_INCLUDE_DISK_FILTERBLOCK_READER_H

#include "slice.h"
#include "bloom_filter.h"

namespace koishidb
{

    // 过滤器块读取器类，用于解析过滤器块并判断键是否可能存在于对应的数据块中
    class FilterBlockReader
    {
    public:
        // 构造函数：解析过滤器块内容并初始化读取器
        // 前置条件："contents"必须在读取器生命周期内保持有效
        FilterBlockReader(const Slice &contents);

        // 判断键是否可能存在于指定偏移量的数据块对应的过滤器中
        // 参数block_offset: 数据块在SSTable中的偏移量
        // 参数key: 待检查的键
        // 返回值: true表示可能存在，false表示一定不存在
        bool KeyMayMatch(uint64_t block_offset, const Slice &key);

    private:
        const BloomFilter bloom_; // 布隆过滤器实例，用于判断键是否可能存在
        const char *data_;        // 指向过滤器数据的起始位置（过滤器块的起始处）
        const char *offset_;      // 指向偏移量数组的起始位置
        size_t num_;              // 偏移量数组中的条目数量（即过滤器的数量）
        size_t base_lg_;          // 编码参数（对应kFilterBaseLg，用于计算过滤器索引）
    };

}; // namespace koishidb

#endif

//实现从过滤器块读取数据和通过过滤器判断一个键是否存在于一个数据块的函数