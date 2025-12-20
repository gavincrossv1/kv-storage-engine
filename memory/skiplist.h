// 该文件定义了SkipList模板类，实现了跳表数据结构，支持键的插入和查找操作，用于内存表（Memtable）的底层存储
#ifndef KOISHIDB_SRC_INCLUDE_MEMORY_SKIPLIST_H
#define KOISHIDB_SRC_INCLUDE_MEMORY_SKIPLIST_H

#include <cassert>
#include <cstdlib>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "common.h"
#include "slice.h"
#include "util.h"

namespace koishidb
{

    // 跳表模板类，需通过比较器（Comparator）实现键的排序
    // 模板参数K：键的类型，Comparator：键的比较器类型
    template <typename K, typename Comparator>
    class SkipList
    {
    public:
        // 禁用拷贝构造函数
        SkipList(const SkipList &that) = delete;

        // 禁用赋值运算符
        SkipList &operator=(const SkipList &that) = delete;

        // 构造函数：接收比较器指针
        explicit SkipList(Comparator *cmp);

        // 析构函数：释放跳表节点和比较器
        ~SkipList();

        // 插入键到跳表中
        // 参数memtable_key：要插入的键
        void Insert(const K &memtable_key);

        // 查找第一个大于等于目标键的键
        // 参数memtable_key：目标键
        // 参数result：存储找到的键（若存在）
        // 返回值：找到返回true，否则返回false
        bool FindFirstGreaterOrEqual(const K &memtable_key, K *result) const;

        // 跳表节点结构体，存储键和各层级的下一个节点指针
        struct Node
        {
        public:
            // 获取节点存储的键
            const K &Key() const
            {
                return memtable_key_;
            }

            // 获取节点的层级
            int get_level() const
            {
                return level;
            }

            // 构造函数：根据键创建节点（自动生成随机层级）
            // 参数memtable_key：节点存储的键
            Node(K memtable_key) : memtable_key_(memtable_key)
            {
                level = RandomHeight();      // 生成随机层级
                next.assign(level, nullptr); // 初始化各层级的下一个节点指针
            }

            // 构造函数：创建指定层级的节点（用于哨兵节点）
            // 参数level：节点的层级
            Node(int level)
            {
                this->level = level;
                next.assign(level, nullptr);
            }

            // 获取第n层级的下一个节点
            // 参数n：层级索引（需小于节点层级）
            // 返回值：第n层级的下一个节点指针
            SkipList<K, Comparator>::Node *get_n_node(int n) const
            {
                assert(n < level);
                return next[n];
            }

            // 设置第n层级的下一个节点
            // 参数n：层级索引（需小于节点层级）
            // 参数node：要设置的下一个节点指针
            void set_n_node(int n, SkipList<K, Comparator>::Node *node)
            {
                assert(n < level);
                next[n] = node;
            }

        private:
            K memtable_key_;                                   // 节点存储的键(即内存表键)
            int level;                                         // 节点的层级(即这个节点有几个指针)
            std::vector<SkipList<K, Comparator>::Node *> next; // 各层级的下一个节点指针数组
        };

        // 跳表迭代器类，用于遍历跳表中的节点
        class Iterator
        {
        public:
            // 拷贝构造函数：默认实现
            Iterator(const Iterator &that) = default;

            // 赋值运算符：默认实现
            Iterator &operator=(const Iterator &that) = default;

            // 构造函数：根据跳表初始化迭代器（指向首节点）
            // 参数list：要遍历的跳表
            Iterator(const SkipList<K, Comparator> *list) : list_(list)
            {
                node_ = list->head_->get_n_node(0); // 初始指向第0层级的第一个节点
            }

            // 析构函数：默认实现
            ~Iterator() = default;

            // 获取当前节点的键
            K Key() const
            {
                return node_->Key();
            };

            // 判断迭代器是否有效（指向有效节点）
            bool Valid() const
            {
                return node_ != nullptr;
            }

            // 前缀递增：移动到下一个节点
            Iterator &operator++()
            {
                node_ = node_->get_n_node(0);
                return *this;
            }

            // 后缀递增：移动到下一个节点（返回原迭代器）
            Iterator operator++(int)
            {
                Iterator tmp = *this;
                ++*this;
                return tmp;
            }

            // 解引用：获取当前节点的键
            const K &operator*() const
            {
                return node_->Key();
            }

            // 移动到下一个节点
            void Next()
            {
                assert(this->Valid());
                node_ = node_->get_n_node(0);
            }

        private:
            Node *node_;           // 当前指向的节点
            const SkipList *list_; // 关联的跳表
        };

    private:
        Comparator *cmp_;                     // 键的比较器
        std::shared_mutex rwlock_;            // 读写锁，支持并发访问
        SkipList<K, Comparator>::Node *head_; // 跳表的哨兵节点（头节点）
        size_t size_;                         // 跳表中的节点数量
    };

    // 构造函数：初始化跳表（创建哨兵节点）
    template <typename K, typename Comparator>
    SkipList<K, Comparator>::SkipList(Comparator *cmp) : cmp_(cmp)
    {
        head_ = new SkipList<K, Comparator>::Node(kSkipListNodeMaxLevel); // 哨兵节点层级为最大层级
    }

    // 析构函数：释放所有节点和比较器
    template <typename K, typename Comparator>
    SkipList<K, Comparator>::~SkipList()
    {
        auto ptr = head_;
        auto cur = ptr->get_n_node(0);
        // 遍历释放所有节点
        while (cur != nullptr)
        {
            auto next = cur->get_n_node(0);
            delete cur;
            cur = next;
        }
        delete head_; // 释放哨兵节点
        delete cmp_;  // 释放比较器
    }

    // 插入键到跳表中
    template <typename K, typename Comparator>
    void SkipList<K, Comparator>::Insert(const K &memtable_key)
    {
        // 注释：若需确保插入键唯一，可取消以下注释（检查键是否已存在）
        // K result;
        // bool ok = FindFirstGreaterOrEqual(memtable_key, &result);
        // if (ok == true && cmp_->Compare(result, memtable_key) == 0) {
        //     return;
        // }

        // 创建新节点（自动生成层级）
        auto new_node = new SkipList<K, Comparator>::Node(memtable_key);
        auto ptr = head_;
        int cur_level = kSkipListNodeMaxLevel - 1; // 从最高层级开始处理

        // 记录每一层的前驱节点
        SkipList<K, Comparator>::Node* update[kSkipListNodeMaxLevel];
        for(int i=0;i<kSkipListNodeMaxLevel;i++) update[i] = nullptr;

        // 逐层查找插入位置
        while (cur_level >= 0)
        {
            auto next_ptr = ptr->get_n_node(cur_level);
            // 找到当前层级中第一个大于等于新节点键的前趋节点
            if (next_ptr != nullptr && cmp_->Compare(next_ptr->Key(), memtable_key) < 0)
            {
                ptr = next_ptr;
                continue;
            }
            update[cur_level] = ptr;
            cur_level--; // 处理下一层级
        }

        // 插入新节点
        int new_level = new_node->get_level();
        for(int i=0;i<new_level;i++)
        {
            new_node->set_n_node(i, update[i]->get_n_node(i));
            update[i]->set_n_node(i, new_node);
        }
    }

    // 查找第一个大于等于目标键的键
    // 若找到，将结果存入result并返回true；否则返回false
    template <typename K, typename Comparator>
    bool SkipList<K, Comparator>::FindFirstGreaterOrEqual(const K &memtable_key,
                                                          K *result) const
    {
        auto ptr = head_;
        int cur_level = kSkipListNodeMaxLevel - 1; // 从最高层级开始查找

        while (ptr != nullptr)
        {
            auto next_ptr = ptr->get_n_node(cur_level);
            // 若下一个节点的键小于目标键，移动到下一个节点
            if (next_ptr != nullptr && cmp_->Compare(next_ptr->Key(), memtable_key) < 0)
            {
                ptr = next_ptr;
                continue;
            }
            // 若下一个节点的键等于目标键，找到结果
            if (next_ptr != nullptr && cmp_->Compare(next_ptr->Key(), memtable_key) == 0)
            {
                *result = next_ptr->Key();
                return true;
            }

            // 若已到最底层（层级0）
            if (cur_level == 0)
            {
                // 若存在下一个节点，其键为第一个大于目标键的键
                if (next_ptr != nullptr)
                {
                    *result = next_ptr->Key();
                    return true;
                }
                break; // 无符合条件的节点
            }
            else
            {
                cur_level--; // 降低层级继续查找
            }
        }

        return false; // 未找到符合条件的节点
    }

} // namespace koishidb

#endif

//跳表的实现,跳表中的一个节点存的是一个内存表键