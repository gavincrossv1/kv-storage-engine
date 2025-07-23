// 该文件定义了BlockIterator类，用于遍历和访问SSTable中数据块（Block）的键值对
#ifndef KOISHIDB_SRC_INCLUDE_DISK_BLOCK_ITERATOR_H
#define KOISHIDB_SRC_INCLUDE_DISK_BLOCK_ITERATOR_H

#include "common.h"
#include "comparator.h"
#include "iterator.h"

namespace koishidb
{
    // 前向声明Block结构体
    struct Block;
    // 前向声明Option类
    class Option;

    // 块迭代器类，继承自基础迭代器接口，用于遍历Block中的键值对
    class BlockIterator : public Iterator
    {
    public:
        // 默认作为内部键（internal key）的迭代器
        // 构造函数：关联到指定的Block，并使用Option中的配置（如比较器）
        BlockIterator(Block *block, const Option *opt);

        // 析构函数：使用默认实现
        ~BlockIterator() = default;

        // 禁用拷贝构造函数
        BlockIterator(const BlockIterator &that) = delete;

        // 禁用赋值运算符
        BlockIterator &operator=(const BlockIterator &that) = delete;

        // 重写父类方法：返回当前迭代器指向的键
        Slice Key() const override;

        // 重写父类方法：返回当前迭代器指向的值
        Slice Value() const override;

        // 重写父类方法：判断迭代器是否有效（是否指向一个合法的键值对）
        bool Valid() const override;

        // 重写父类方法：查找目标键，定位到第一个大于等于目标的键值对
        bool Seek(const Slice &target) override;

        // 重写父类方法：将迭代器定位到第一个键值对
        void SeekToFirst() override;

        // 重写父类方法：将迭代器定位到最后一个键值对
        void SeekToLast() override;

        // 重写父类方法：将迭代器移动到下一个键值对
        void Next() override;

        // 重写父类方法：将迭代器移动到上一个键值对
        void Prev() override;

    private:
        std::string key_;             // 当前键的字符串存储
        const char *const data_;      // 指向Block数据的起始地址（常量指针，指向常量数据）
        const size_t size_;           // Block数据的总大小（字节数）
        const Comparator *const cmp_; // 比较器（用于比较内部键，常量指针，指向常量对象）
        uint32_t offset_;             // 当前在data_中的偏移量（指向待解析数据的起始位置）
        Slice value_;                 // 当前键对应的值（以Slice形式存储，避免复制）
    };
}; // namespace koishidb
#endif

//对内存中数据块的迭代器的实现