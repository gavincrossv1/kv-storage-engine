// 该文件实现了InternalKey类及InternalKeyComparator类，内部键包含用户键、序列号和键类型，比较器用于内部键的排序
#include "key.h"

#include <string>

namespace koishidb
{

    // 构造函数：根据用户键、序列号和键类型创建内部键
    // 参数user_key：用户提供的键
    // 参数sequence：序列号（用于版本控制）
    // 参数type：键类型（如值类型、删除标记）
    InternalKey::InternalKey(const Slice &user_key, SequenceNumber sequence,KeyType type)
    {
        rep_ = user_key.ToString(); // 存储用户键
        // 构建标记（序列号左移8位 + 键类型）,为了紧凑排序
        uint64_t tag = sequence << 8 | static_cast<char>(type);
        rep_ += std::to_string(tag); // 拼接标记到用户键后//?
    }

    // 构造函数：根据字符串表示创建内部键
    // 参数rep：内部键的字符串表示
    InternalKey::InternalKey(const std::string &rep) : rep_(rep) {}

    // 将内部键转换为字符串
    std::string InternalKey::ToString() const
    {
        return rep_;
    }

    // 将内部键转换为Slice,这里是进行了string到slice的隐式类型转换,无问题,但写法不友好
    Slice InternalKey::ToSlice() const
    {
        return rep_;
        //return Slice(req_);
    }

    // 清空内部键内容
    void InternalKey::Clear()
    {
        rep_.clear();
    }

    // 判断内部键是否有效（非空）
    bool InternalKey::Valid() const
    {
        return !rep_.empty();
    }

    // 将Slice转换为内部键
    // 参数key：待转换的Slice
    // 返回值：对应的InternalKey对象
    InternalKey TransToInternalKey(const Slice &key)
    {
        return InternalKey(key.ToString());
    }

    // 返回比较器的名称
    std::string InternalKeyComparator::Name() const
    {
        return "InternalKeyComparator";
    }

    // 比较两个内部键（Slice形式）
    // 比较规则：先比较用户键，若相同则比较序列号（大的序列号在前）
    // 参数a：第一个内部键
    // 参数b：第二个内部键
    // 返回值：-1表示a < b，1表示a > b，0表示相等
    int InternalKeyComparator::Compare(const Slice &a, const Slice &b) const
    {
        Slice user_key1, user_key2;
        SequenceNumber seq1, seq2;

        // 提取用户键和序列号
        ExtractUserKey(a, &user_key1, &seq1);
        ExtractUserKey(b, &user_key2, &seq2);

        // 先比较用户键
        int r = user_key1.Compare(user_key2);
        if (r == 0)
        {
            // 用户键相同，比较序列号（大的序列号优先级高）
            if (seq1 < seq2)
            {
                r = +1;
            }
            else if (seq1 > seq2)
            {
                r = -1;
            }
        }
        return r;
    }

    // 比较两个内部键（InternalKey对象）
    // 参数a：第一个内部键
    // 参数b：第二个内部键
    // 返回值：-1表示a < b，1表示a > b，0表示相等
    int InternalKeyComparator::Compare(const InternalKey &a,
                                       const InternalKey &b) const
    {
        return Compare(a.ToSlice(), b.ToSlice());
    }

} // namespace koishidb

//对内部键的实现,内部键等于外部键+序列号+类型(put,delete),内部键在磁盘上按字典序排列，排序时需先比较外部键，再比较序列号sequence（逆序，保证新版本优先被读到）。