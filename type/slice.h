// 该文件声明了Slice类，提供轻量级字符串视图功能，用于高效传递和操作字符串数据（避免不必要的内存复制）
#ifndef KOISHIDB_SRC_INCLUDE_TYPE_SLICE_H
#define KOISHIDB_SRC_INCLUDE_TYPE_SLICE_H

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

namespace koishidb
{
    // Slice类：轻量级字符串视图，存储字符串指针和长度，不管理内存所有权
    class Slice
    {
    public:
        // 默认构造函数：初始化空Slice
        Slice();

        // 构造函数：根据C字符串初始化（自动计算长度）
        // 参数d：C字符串指针（以'\0'结尾）
        Slice(const char *d);

        // 构造函数：根据std::string初始化
        // 参数s：std::string对象
        Slice(const std::string &s);

        // 构造函数：根据指针和长度初始化
        // 参数d：字符串数据指针
        // 参数size：字符串长度
        Slice(const char *d, size_t size);

        // 拷贝构造函数：默认实现
        Slice(const Slice &that) = default;

        // 赋值运算符：默认实现
        Slice &operator=(const Slice &that) = default;

        // 获取字符串数据的指针
        const char *data() const;

        // 获取字符串的长度
        size_t size() const;

        // 判断字符串是否为空
        bool empty() const;

        // 重载[]运算符：获取指定位置的字符
        // 参数n：字符索引（需小于字符串长度）
        char operator[](size_t n) const;

        // 重载==运算符：判断两个Slice是否相等
        bool operator==(const Slice &that) const;

        // 重载!=运算符：判断两个Slice是否不相等
        bool operator!=(const Slice &that) const;

        // 比较两个Slice的大小（字典序）
        // 返回值：-1表示当前Slice小，1表示当前Slice大，0表示相等
        int Compare(const Slice &that) const;

        // 跳过前n个字符（移动数据指针，减小长度）
        // 参数n：要跳过的字符数（需小于等于当前长度）
        void Advance(size_t n);

        // 追加数据到Slice（预留接口，实际实现需注意内存管理）
        void Append(const char *d, size_t n);

        // 清空Slice（指向空字符串）
        void Clear();

        // 将Slice转换为std::string
        std::string ToString() const;

    private:
        const char *data_; // 字符串数据指针(不拥有字符串,只是作为引用)
        size_t size_;      // 字符串长度
    };
}; // namespace koishidb

#endif

//内存数据片段,不持有实际数据，而是引用（指向）某块内存的起始地址和长度,高度减少拷贝消耗