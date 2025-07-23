// 该文件实现了FilterBlockBuilder类，用于构建SSTable中的过滤器块（基于布隆过滤器），快速判断键是否可能存在于数据块中
#include "filterblock_builder.h"

#include "common.h"
#include "encode.h"

namespace koishidb
{
    // 向过滤器块添加一个键
    // 参数key: 待添加的键
    void FilterBlockBuilder::AddKey(const Slice &key)
    {
        Slice k = key;
        // 记录当前键在扁平化存储中的起始位置
        start_.push_back(keys_.size());//注意是keys_,而不是key
        // 将键的内容追加到扁平化存储中
        keys_.append(key.data(), key.size());
    }

    // 开始处理一个新的数据块，计算并生成该数据块对应的过滤器（若需要）
    // 参数block_offset: 数据块在SSTable中的偏移量
    void FilterBlockBuilder::StartBlock(uint64_t block_offset)
    {
        // 根据数据块偏移量计算对应的过滤器索引（每个过滤器覆盖kFilterBase大小的区域）
        uint64_t filter_index = (block_offset / kFilterBase);
        assert(filter_index >= filter_offsets_.size());

        // 生成所有未生成的过滤器（直到当前过滤器索引）(因为前面数据块的过滤器没有完全生成)
        while (filter_index > filter_offsets_.size())
        {
            GenerateFilter();
        }
    }

    // 生成过滤器（基于当前添加的所有键构建布隆过滤器）
    void FilterBlockBuilder::GenerateFilter()
    {
        const size_t num_keys = start_.size();
        if (num_keys == 0)
        {
            // 若当前没有键，直接记录过滤器在结果中的偏移量（空过滤器）
            filter_offsets_.push_back(result_.size());
            return;
        }

        // 在键的扁平化存储末尾添加终止标记（用于计算最后一个键的长度）
        start_.push_back(keys_.size());
        // 临时存储键的Slice视图（指向扁平化存储中的键）
        tmp_keys_.resize(num_keys);

        // 构建每个键的Slice视图
        for (size_t i = 0; i < num_keys; i++)
        {
            const char *base = keys_.data() + start_[i];
            size_t length = start_[i + 1] - start_[i];
            tmp_keys_[i] = Slice(base, length);
        }

        // 记录当前过滤器在结果中的起始偏移量
        filter_offsets_.push_back(result_.size());
        // 使用布隆过滤器对当前所有键构建过滤器，并追加到结果中
        bloom_.CreateFilter(&tmp_keys_[0], static_cast<int>(num_keys), &result_);

        // 清理临时数据，准备处理下一批键
        tmp_keys_.clear();
        keys_.clear();
        start_.clear();
    }

    // 完成过滤器块的构建，返回构建好的过滤器块数据（格式与LevelDB的过滤器块一致）
    Slice FilterBlockBuilder::Finish()
    {
        // 若还有未处理的键，生成对应的过滤器
        if (!start_.empty())
        {
            GenerateFilter();
        }

        // 存储过滤器偏移量数组的起始位置
        const uint32_t array_offset = result_.size();
        // 将所有过滤器的偏移量编码后追加到结果中
        for (size_t i = 0; i < filter_offsets_.size(); i++)
        {
            PutFixed32(&result_, filter_offsets_[i]);
        }

        // 追加过滤器偏移量数组的起始位置
        PutFixed32(&result_, array_offset);
        // 追加kFilterBase的对数（用于解析时计算过滤器索引）
        result_.push_back(kFilterBaseLg);

        // 返回完整的过滤器块数据
        return Slice(result_);
    }

}; // namespace koishidb

//一个 SSTable 只有一个过滤器块,一个过滤器块包含所有的布隆过滤器
//过滤器块格式如下:
// +-------------------------+-------------------------+-------------------------+
// |     过滤器数据区        |   过滤器偏移量数组      |         元数据          |
// | （多个布隆过滤器实例）  | （每个过滤器的起始位置） | （偏移量数组起始位置+基数）|
// +-------------------------+-------------------------+-------------------------+