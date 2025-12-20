// 该文件实现了Memtable类及其迭代器MemtableIterator，提供内存表的插入、查询、大小估计及迭代功能
#include "memtable.h"
#include "common.h"
#include "comparator.h"
#include "key.h"

namespace koishidb
{

    // 内存表迭代器，用于遍历内存表并将数据 dump 到磁盘（无需支持键查找，仅需迭代功能）
    class MemtableIterator : public Iterator
    {
    public:
        ~MemtableIterator() = default;

        // 构造函数：接收内存表的底层表迭代器
        MemtableIterator(Memtable::Table *table) : iter_(table) {}

        // 判断当前迭代器是否有效（指向有效元素）
        bool Valid() const override
        {
            return iter_.Valid();
        }

        // 移动到下一个元素
        void Next() override
        {
            iter_++;
        }

        // 获取当前元素的内部键（从内存表键中提取）
        Slice Key() const override
        {
            Slice memtable_key = iter_.Key();
            Slice internal_key;
            ExtractInternalKey(memtable_key, &internal_key); // 提取内部键
            return internal_key;
        }

        // 获取当前元素的值（从内存表键中提取）
        Slice Value() const override
        {
            Slice memtable_key = iter_.Key();
            uint32_t internal_key_len;
            GetVarint32(&memtable_key, &internal_key_len); // 解析内部键长度
            memtable_key.Advance(internal_key_len);        // 跳过内部键
            uint32_t value_len;
            GetVarint32(&memtable_key, &value_len);       // 解析值长度
            return Slice(memtable_key.data(), value_len); // 返回值
        }

        // 移动到第一个元素
        // 注意：迭代器在构造时已经指向第一个元素，此方法主要用于重置
        void SeekToFirst() override
        {
            // 迭代器在构造函数中已经指向第一个元素
            // 这里不需要做任何操作，因为SkipList::Iterator在构造时就已经指向第一个节点
            // 如果表为空，iter_.Valid() 会返回 false，调用方应该检查这一点
        }

    private:
        Memtable::Table::Iterator iter_; // 底层表的迭代器

        // 以下方法无用（内存表迭代器无需支持这些操作）
        bool Seek(const Slice &target) override
        {
            return false;
        }

        void SeekToLast() override {}

        void Prev() override {}
    };

    // 估计内存表大小
    size_t Memtable::EstimatedSize()
    {
        return size_;
    }

    // 从内存表中查找键对应的值
    // 参数memtable_key：要查找的内存表键（包含内部键和值信息）
    // 参数result：存储查找结果的值
    // 返回值：找到返回true，否则返回false
    bool Memtable::Get(const Slice &memtable_key, std::string *result)
    {
        Slice ret;
        // 在跳表中查找第一个大于等于目标键的元素
        bool flag = table_->FindFirstGreaterOrEqual(memtable_key, &ret);
        if (!flag)
        {
            return false;
        }

        // 提取用户键和序列号，判断是否匹配
        Slice internal_key1, internal_key2;
        Slice key(memtable_key.data(), memtable_key.size());
        ExtractInternalKey(ret, &internal_key1);
        ExtractInternalKey(key, &internal_key2);

        Slice user_key1, user_key2;
        SequenceNumber s1, s2;
        ExtractUserKey(internal_key1, &user_key1, &s1);
        ExtractUserKey(internal_key2, &user_key2, &s2);

        InternalKeyComparator cmp;
        if (user_key1.Compare(user_key2) != 0)
        {
            return false; // 用户键不匹配，查找失败
        }

        // 从查找到的结果中提取值
        ExtractValueFromMemtable(ret, result);
        return true;
    }

    // 向内存表插入键值对（通过内存表键）
    // 参数memtable_key：内存表键（包含内部键和值信息）
    void Memtable::Insert(const Slice &memtable_key)
    {
        table_->Insert(memtable_key); // 插入到跳表
        size_ += memtable_key.size(); // 更新内存表大小
    }

    // 创建内存表迭代器
    Iterator *Memtable::NewIterator()
    {
        Iterator *iter = new MemtableIterator(table_);
        return iter;
    }
}; // namespace koishidb

//定义了内存表(核心就是跳表),主要实现了Get和Insert接口