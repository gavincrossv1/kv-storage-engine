#include "block_iterator.h"

#include <iostream>

#include "option.h"
#include "block.h"
#include "key.h"
#include "util.h"

namespace koishidb
{
    // 块迭代器构造函数
    // 初始化迭代器，关联到指定的块并使用配置中的比较器
    BlockIterator::BlockIterator(Block *block, const Option *opt, bool owns_block)
        : data_(block->data), size_(block->size), cmp_(opt->cmp),
          owned_block_(owns_block ? block : nullptr)
    {
        SeekToFirst(); // 初始化为指向第一个元素
    }

    BlockIterator::~BlockIterator()
    {
        delete owned_block_;
    }

    // 将迭代器定位到块的第一个元素
    void BlockIterator::SeekToFirst()
    {
        offset_ = 0;              // 重置偏移量到块的起始位置
        key_.clear();             // 清空当前键
        value_ = Slice(data_, 0); // 初始值为空切片
    }

    // 将迭代器移动到下一个元素
    void BlockIterator::Next()
    {
        // 计算下一个条目的偏移量（当前值的结束位置）
        offset_ = value_.data() + value_.size() - data_;

        // 如果偏移量已超出块大小，则迭代器无效，直接返回
        if (!this->Valid())
        {
            return;
        }

        // 剩余未处理的块数据
        Slice rest_entries = Slice(data_ + offset_, size_ - offset_);

        // 解析下一个条目的键长度
        uint32_t internal_key_len;
        GetVarint32(&rest_entries, &internal_key_len);

        // 提取键内容
        key_ = std::string(rest_entries.data(), internal_key_len);
        rest_entries.Advance(internal_key_len); // 移动到值的起始位置

        // 解析值的长度
        uint32_t value_len;
        GetVarint32(&rest_entries, &value_len);

        // 提取值内容
        value_ = Slice(rest_entries.data(), value_len);
    }

    // 查找目标键，若找到则返回true（此时Key() == target）
    // 功能：定位到第一个大于等于target的条目
    bool BlockIterator::Seek(const Slice &target)
    {
        SeekToFirst(); // 先定位到起始位置
        Next();        // 移动到第一个元素

        // 线性遍历，直到找到大于等于目标的键
        while (Valid() && cmp_->Compare(Key(), target) == -1)
        {
            Next();
        }

        // 检查是否找到匹配的键
        if (Valid() && cmp_->Compare(Key(), target) == 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    // 返回当前迭代器指向的键
    Slice BlockIterator::Key() const
    {
        return key_;
    }

    // 返回当前迭代器指向的值
    Slice BlockIterator::Value() const
    {
        return value_;
    }

    // 判断迭代器是否有效（是否指向一个合法的元素）
    bool BlockIterator::Valid() const
    {
        return offset_ < size_;
    }

    // 将迭代器移动到上一个元素（未实现）
    void BlockIterator::Prev()
    {
        // 空实现
    }

    // 将迭代器定位到块的最后一个元素（待实现）//?
    void BlockIterator::SeekToLast()
    {
        // 待完成
    }
}; // namespace koishidb

//对内存中数据块的迭代器的实现