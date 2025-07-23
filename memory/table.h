// 该文件定义了Table模板接口类，规定了键值对存储表的基本操作（获取、插入、删除、估计大小）
#ifndef KOISHIDB_SRC_INCLUDE_MEMORY_TABLE_H
#define KOISHIDB_SRC_INCLUDE_MEMORY_TABLE_H

#include <cstddef>

// Table是一个接口类，定义了键值对存储表的基本操作
namespace koishidb
{
    template <typename K, typename V>
    class Table
    {
    public:
        // 默认构造函数
        Table() = default;

        // 虚析构函数（确保子类析构函数被正确调用）
        virtual ~Table() = default;

        // 纯虚函数：根据键获取值
        // 参数key：要查询的键
        // 参数value：存储查询到的值（若存在）
        // 返回值：找到返回true，否则返回false
        virtual bool Get(const K &key, V &value) = 0;

        // 纯虚函数：插入键值对
        // 参数key：要插入的键
        // 参数value：要插入的值
        virtual void Put(const K &key, const V &value) = 0;

        // 纯虚函数：删除键
        // 参数key：要删除的键
        virtual void Delete(const K &key) = 0;

        // 纯虚函数：估计当前存储的键值对数量
        // 返回值：估计的键值对数量
        virtual size_t EstimatedSize() = 0;
    };
} // namespace koishidb

#endif

//内存表外部父类的实现