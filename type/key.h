// 该文件定义了InternalKey类及InternalKeyComparator类，内部键用于数据库内部管理，包含用户键、序列号和类型信息，比较器用于内部键的比较
#ifndef KOISHIDB_SRC_INCLUDE_TYPE_KEY_H
#define KOISHIDB_SRC_INCLUDE_TYPE_KEY_H

#include <string>

#include "common.h"
#include "slice.h"
#include "comparator.h"
#include "encode.h"
#include "util.h"

namespace koishidb
{

    // 内部键类，包含用户键、序列号和键类型，用于数据库内部管理
    // 无需显式查找键，可通过CreateMemtableKey创建，由InternalKeyComparator比较
    class InternalKey
    {
    public:
        // 默认构造函数
        InternalKey() = default;

        // 构造函数：根据字符串表示创建内部键
        // 参数rep：内部键的字符串表示(注意这个rep已经是内部键了,而不是外部的键)
        InternalKey(const std::string &rep);

        // 构造函数：根据用户键、序列号和键类型创建内部键
        // 参数user_key：用户提供的键
        // 参数sequence：序列号（用于版本控制）
        // 参数type：键类型（如值类型、删除标记）
        InternalKey(const Slice &user_key, SequenceNumber sequence, KeyType type);

        // 默认赋值运算符
        InternalKey &operator=(const InternalKey &that) = default;

        // 将内部键转换为字符串
        std::string ToString() const;

        // 将内部键转换为Slice
        Slice ToSlice() const;

        // 清空内部键内容
        void Clear();

        // 判断内部键是否有效（非空）
        bool Valid() const;

    private:
        std::string rep_; // 内部键的字符串表示（用户键 + 序列号 + 键类型）
    };

    // 将Slice转换为内部键
    // 参数key：待转换的Slice
    // 返回值：对应的InternalKey对象
    InternalKey TransToInternalKey(const Slice &key);

    // 内部键比较器，用于内部键的排序
    class InternalKeyComparator : public Comparator
    {
    public:
        // 默认构造函数
        InternalKeyComparator() = default;

        // 返回比较器的名称
        std::string Name() const override;

        // 比较两个内部键（Slice形式）
        // 参数a：第一个内部键
        // 参数b：第二个内部键
        // 返回值：-1表示a < b，1表示a > b，0表示相等
        int Compare(const Slice &a, const Slice &b) const override;

        // 比较两个内部键（InternalKey对象）
        // 参数a：第一个内部键
        // 参数b：第二个内部键
        // 返回值：-1表示a < b，1表示a > b，0表示相等
        int Compare(const InternalKey &a, const InternalKey &b) const;
    }; // class InternalKeyComparator
}; // namespace koishidb

#endif

//对内部键的实现,内部键等于外部键+序列号+类型(put,delete),内部键在磁盘上按字典序排列，排序时需先比较外部键，再比较序列号sequence（逆序，保证新版本优先被读到）。