// 该文件定义了FilterBlockBuilder类，用于构建SSTable中的过滤器块，基于布隆过滤器实现，支持快速判断键是否可能存在于数据块中
#ifndef KOISHIDB_SRC_INCLUDE_FILTERBLOCK_BUILDER_H
#define KOISHIDB_SRC_INCLUDE_FILTERBLOCK_BUILDER_H

#include <vector>

#include "common.h"
#include "status.h"
#include "slice.h"
#include "bloom_filter.h"

namespace koishidb
{

    // 过滤器块构建器类，用于构建SSTable中的过滤器块（包含多个布隆过滤器）
    class FilterBlockBuilder
    {
    public:
        // 构造函数：初始化布隆过滤器（每个键分配kBloomFilterPerKeySize位）
        FilterBlockBuilder() : bloom_(kBloomFilterPerKeySize) {}

        // 禁用拷贝构造函数
        FilterBlockBuilder(const FilterBlockBuilder &that) = delete;

        // 禁用赋值运算符
        FilterBlockBuilder &operator=(const FilterBlockBuilder &that) = delete;

        // 向过滤器块添加一个键
        void AddKey(const Slice &key);

        // 开始处理一个新的数据块，计算并生成该数据块对应的过滤器（若需要）
        // 参数offset: 数据块在SSTable中的偏移量
        void StartBlock(uint64_t offset);

        // 完成过滤器块的构建，返回构建好的过滤器块数据
        Slice Finish();

    private:
        // 生成过滤器（基于当前添加的所有键构建布隆过滤器）
        void GenerateFilter();

        BloomFilter bloom_;                    // 布隆过滤器实例，用于构建单个过滤器
        std::string keys_;                     // 扁平化存储所有添加的键(指用户键)（便于调试内存错误）
        std::vector<uint32_t> start_;          // 存储每个键在keys_中的起始位置
        std::vector<Slice> tmp_keys_;          // 临时存储键的Slice视图（用于构建布隆过滤器）
        std::vector<uint32_t> filter_offsets_; // 存储每个过滤器在结果中的起始偏移量
        std::string result_;                   // 最终的过滤器块数据
    };

}; // namespace koishidb

#endif

//一个 SSTable 只有一个过滤器块,一个过滤器块包含所有的布隆过滤器
//过滤器块格式如下:
// +-------------------------+-------------------------+-------------------------+
// |     过滤器数据区        |   过滤器偏移量数组      |         元数据          |
// | （多个布隆过滤器实例）  | （每个过滤器的起始位置） | （偏移量数组起始位置+基数）|
// +-------------------------+-------------------------+-------------------------+