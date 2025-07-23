// 该文件实现了FilterBlockReader类，用于读取和解析SSTable中的过滤器块，通过布隆过滤器判断键是否可能存在于数据块中
#include "filterblock_reader.h"

#include "common.h"

namespace koishidb
{

    // 构造函数：解析过滤器块内容，初始化读取器
    // 参数contents: 过滤器块的完整数据
    FilterBlockReader::FilterBlockReader(const Slice &contents): bloom_(kBloomFilterPerKeySize),data_(nullptr),offset_(nullptr),num_(0),base_lg_(0)
    {
        size_t n = contents.size();
        // 过滤器块最小为5字节（4字节偏移数组起始位置 + 1字节base_lg）
        if (n < 5)
        {
            return;
        }

        // 最后1字节存储base_lg（kFilterBase的对数）
        base_lg_ = contents[n - 1];
        // 倒数5-1字节存储偏移数组的起始位置（固定32位）
        uint32_t last_word = DecodeFixed32(contents.data() + n - 5);

        // 校验偏移数组起始位置的有效性
        if (last_word > n - 5)
        {
            return;
        }

        // 初始化数据指针：指向过滤器块起始位置
        data_ = contents.data();
        // 初始化偏移数组指针：指向偏移数组的起始位置
        offset_ = data_ + last_word;
        // 计算过滤器数量：偏移数组中每个过滤器占4字节
        num_ = (n - 5 - last_word) / 4;
    }

    // 判断键是否可能存在于指定偏移量的数据块对应的过滤器中
    // 参数block_offset: 数据块在SSTable中的偏移量
    // 参数key: 待检查的键
    // 返回值: true表示可能存在，false表示一定不存在
    bool FilterBlockReader::KeyMayMatch(uint64_t block_offset, const Slice &key)
    {
        // 根据数据块偏移量计算对应的过滤器索引（base_lg_为log2(kFilterBase)）
        uint64_t index = block_offset >> base_lg_;

        // 索引有效（在过滤器数量范围内）
        if (index < num_)
        {
            // 解析过滤器的起始偏移量和结束偏移量（每个偏移量占4字节）
            uint32_t start = DecodeFixed32(offset_ + index * 4);
            uint32_t limit = DecodeFixed32(offset_ + index * 4 + 4);

            // 校验过滤器范围的有效性
            if (start <= limit && limit <= static_cast<size_t>(offset_ - data_))
            {
                // 提取对应的过滤器数据
                Slice filter = Slice(data_ + start, limit - start);
                // 使用布隆过滤器判断键是否可能存在
                return bloom_.KeyMayMatch(key, filter);
            }
            else if (start == limit)
            {
                // 空过滤器：一定不存在匹配的键
                return false;
            }
        }

        // 索引无效或解析出错时，视为可能存在（避免误判不存在）
        return true;
    }
}; // namespace koishidb

//实现从过滤器块读取数据和通过过滤器判断一个键是否存在于一个数据块的函数