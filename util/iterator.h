// 该文件定义了Iterator抽象类，提供遍历集合中元素的接口，支持移动、定位及获取键值等操作
#ifndef KOISHIDB_SRC_INCLUDE_UTIL_ITERATOR_H
#define KOISHIDB_SRC_INCLUDE_UTIL_ITERATOR_H

#include "slice.h"

namespace koishidb
{
    // 迭代器抽象类，用于遍历集合中的元素（如内存表、SSTable等）
    class Iterator
    {
    public:
        // 构造函数：默认初始化
        Iterator() = default;

        // 禁用拷贝构造函数
        Iterator(const Iterator &that) = delete;

        // 禁用赋值运算符
        Iterator &operator=(const Iterator &that) = delete;

        // 虚析构函数，确保派生类正确析构
        virtual ~Iterator() = default;

        // 判断当前迭代器是否指向有效元素
        // 返回值：true表示有效，false表示已到达末尾或未初始化
        virtual bool Valid() const = 0;

        // 将迭代器移动到前一个元素
        virtual void Prev() = 0;

        // 将迭代器移动到后一个元素
        virtual void Next() = 0;

        // 将迭代器移动到第一个元素
        virtual void SeekToFirst() = 0;

        // 将迭代器移动到最后一个元素
        virtual void SeekToLast() = 0;

        // 获取当前元素的键
        // 返回值：当前元素的键（以Slice形式表示）
        virtual Slice Key() const = 0;

        // 获取当前元素的值
        // 返回值：当前元素的值（以Slice形式表示）
        virtual Slice Value() const = 0;

        // 将迭代器定位到第一个键大于等于目标键的元素
        // 参数target：目标键
        // 返回值：true表示找到匹配位置，false表示定位失败
        virtual bool Seek(const Slice &target) = 0;
    };
}; // namespace koishidb

#endif

//迭代器抽象类的定义