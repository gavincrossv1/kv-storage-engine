// 该文件定义了Memtable类及其相关组件，内存表基于跳表实现，用于临时存储数据，支持插入、查询和迭代操作
#ifndef SRC_INCLUDE_MEMORY_MEMTABLE_H
#define SRC_INCLUDE_MEMORY_MEMTABLE_H

#include <atomic>
#include <iostream>
#include <queue>
#include <shared_mutex>
#include <string>

#include "logger.h"
#include "skiplist.h"
#include "table.h"
#include "comparator.h"
#include "iterator.h"

namespace koishidb
{

    class MemtableIterator;

    // 内存表键比较器：用户键相同时，序列号越大的内存表键越小,排的就更靠前（保证新值优先）
    class MemtableKeyComparator : public Comparator
    {
    public:
        MemtableKeyComparator() = default;

        // 比较两个内存表键
        int Compare(const Slice &memtable_key1, const Slice &memtable_key2) const
        {
            LookupKey key1(memtable_key1), key2(memtable_key2);
            Slice user_key1 = key1.UserKey(), user_key2 = key2.UserKey();
            SequenceNumber s1 = key1.GetSequence(), s2 = key2.GetSequence();

            // 先比较用户键
            int r = user_key1.Compare(user_key2);
            if (r == 0)
            {
                // 用户键相同，比较序列号（大的序列号优先级高）
                if (s1 == s2)
                {
                    return 0;
                }
                if (s1 > s2)
                {
                    return -1; // s1更大，memtable_key1更小
                }
                if (s1 < s2)
                {
                    return 1; // s1更小，memtable_key1更大
                }
            }
            return r;
        }

        // 返回比较器名称
        std::string Name() const
        {
            return "MemtableKeyComparator";
        }
    };

    // 内存表类：基于跳表实现，用于临时存储数据，支持插入、查询和迭代
    class Memtable
    {
    public:
        // 内存表的底层表类型（跳表）
        using Table = SkipList<Slice, MemtableKeyComparator>;

        // 构造函数：初始化跳表（使用内存表键比较器）
        Memtable(): table_(new SkipList<Slice, MemtableKeyComparator>(new MemtableKeyComparator())) 
        {

        }

        // 禁用拷贝构造函数
        Memtable(const Memtable &that) = delete;

        // 禁用赋值运算符
        Memtable &operator=(const Memtable &that) = delete;

        // 析构函数：释放跳表
        ~Memtable()
        {
            delete table_;
        }

        // 估计内存表大小
        size_t EstimatedSize();

        // 从内存表中查找键对应的值
        // 参数memtable_key：内存表键
        // 参数result：存储查找结果的值
        // 返回值：找到返回true，否则返回false
        bool Get(const Slice &memtable_key, std::string *result);

        // 向内存表插入键值对
        // 参数memtable_key：内存表键（包含内部键和值信息）
        void Insert(const Slice &memtable_key);

        // 创建内存表迭代器
        Iterator *NewIterator();

    private:
        friend class MemtableIterator; // 迭代器可访问私有成员

        Table *table_;             // 底层存储（跳表）
        int ref_;                  // 引用计数
        std::atomic<bool> imm_;    // 标记是否为不可变内存表
        std::shared_mutex rwlock_; // 读写锁（用于并发访问）
        size_t size_;              // 内存表大小（估计值）
    };

}; // namespace koishidb

#endif

//定义了内存表(核心就是跳表),主要实现了Get和Insert接口