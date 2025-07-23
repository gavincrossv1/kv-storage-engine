// 该文件定义了Comparator基类，提供键比较的接口，用于实现不同的键比较逻辑
#ifndef KOISHIDB_SRC_INCLUDE_UTIL_COMPARATOR_H
#define KOISHIDB_SRC_INCLUDE_UTIL_COMPARATOR_H

#include "common.h"
#include "slice.h"
#include "util.h"

namespace koishidb
{
    // 比较器基类，定义键比较的接口
    class Comparator
    {
    public:
        // 虚析构函数，确保派生类能够正确析构
        virtual ~Comparator() = default;

        // 比较两个键的大小
        // 参数a: 待比较的第一个键
        // 参数b: 待比较的第二个键
        // 返回值: -1表示a < b，1表示a > b，0表示a == b
        virtual int Compare(const Slice &a, const Slice &b) const = 0;

        // 返回比较器的名称，用于标识不同的比较器实现
        virtual std::string Name() const = 0;
    };
}; // namespace koishidb

#endif

//定义了比较器的父类,子类有内存表键的比较器和内部键的迭代器