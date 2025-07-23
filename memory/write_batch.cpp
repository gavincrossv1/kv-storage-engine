// 该文件实现了WriteBatch类及WriteBatchInternal类，支持批量写入操作（Put/Delete），可高效插入内存表
#include "write_batch.h"

#include "encode.h"

namespace koishidb
{
    // WriteBatch格式：头部包含8字节序列号 + 4字节操作计数，后续为批量操作内容
    WriteBatch::WriteBatch() { Clear(); }

    // 向批量操作中添加Put操作（插入键值对）
    // 参数key：键(指用户键)
    // 参数value：值
    void WriteBatch::Put(const Slice &key, const Slice &value)
    {
        assert(rep_.size() >= kWriteBatchHeader); // 确保头部已初始化

        // 操作计数+1
        WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);

        // 写入操作类型（值类型）
        rep_.push_back(static_cast<char>(KeyType::kTypeValue));
        // 写入键（长度前缀格式）
        PutLengthPrefixedSlice(&rep_, key);
        // 写入值（长度前缀格式）
        PutLengthPrefixedSlice(&rep_, value);
    }

    // 向批量操作中添加Delete操作（删除键）
    // 参数key：要删除的键
    void WriteBatch::Delete(const Slice &key)
    {
        assert(rep_.size() >= kWriteBatchHeader); // 确保头部已初始化

        // 操作计数+1
        WriteBatchInternal::SetCount(this, WriteBatchInternal::Count(this) + 1);

        // 写入操作类型（删除类型）
        rep_.push_back(static_cast<char>(KeyType::kTypeDeletion));
        // 写入键（长度前缀格式）
        PutLengthPrefixedSlice(&rep_, key);
    }

    // 清空批量操作（重置为初始状态）
    void WriteBatch::Clear()
    {
        rep_.clear();
        rep_.resize(kWriteBatchHeader); // 保留头部空间（8+4字节）
    }

    // 追加另一个批量操作到当前批量操作
    // 参数that：要追加的批量操作
    void WriteBatch::Append(const WriteBatch &that)
    {
        WriteBatchInternal::Append(&that, this);
    };

    // 将批量操作中的所有操作插入到内存表
    // 参数memtable：目标内存表
    void WriteBatch::InsertAll(Memtable *memtable)
    {
        int count = WriteBatchInternal::Count(this);                // 操作总数
        SequenceNumber number = WriteBatchInternal::Sequence(this); // 起始序列号
        Slice entries(rep_);
        entries.Advance(kWriteBatchHeader); // 跳过头部

        // 处理内存表键的lambda函数：根据操作类型生成内存表键
        auto ProcessMemtableKey = [&](char key_type) -> Slice
        {
            std::string memtable_key;
            number++; // 序列号递增（确保操作有序）

            switch (static_cast<KeyType>(key_type))
            {
            case KeyType::kTypeValue:
            {
                // 解析用户键和值，生成内存表键
                std::string user_key;
                GetLengthPrefixedSlice(&user_key, &entries);
                std::string value;
                GetLengthPrefixedSlice(&value, &entries);
                return CreateMemtableKey(user_key.data(), number, value.data(), KeyType::kTypeValue);
            }
            case KeyType::kTypeDeletion:
            {
                // 解析用户键，生成删除标记的内存表键
                std::string user_key;
                GetLengthPrefixedSlice(&user_key, &entries);
                return CreateMemtableKey(user_key.data(), number, " ", KeyType::kTypeDeletion);
            }
            default:
            {
                // 无效操作类型，返回空
                return {};
            }
            }
        };

        // 遍历writebatch中的所有操作，插入内存表
        for (int i = 0; i < count; i++)
        {
            char key_type = entries.data()[0]; // 获取操作类型
            entries.Advance(1);                // 跳过操作类型
            Slice memtable_key = ProcessMemtableKey(key_type);
            if (memtable_key.size() != 0)
            {
                memtable->Insert(memtable_key); // 插入内存表
            }
        }
    }

    // 估计批量操作的大小（字节数）
    // 返回值：批量操作的大小
    size_t WriteBatch::EstimatedSize() { return rep_.size(); }

    // ------------ WriteBatchInternal 实现 --------------------

    // 获取批量操作中的操作总数
    // 参数w：批量操作对象
    // 返回值：操作总数
    int WriteBatchInternal::Count(const WriteBatch *w)
    {
        return static_cast<int>(DecodeFixed32(w->rep_.data() + 8)); // 解析头部4字节计数
    }

    // 设置批量操作中的操作总数
    // 参数w：批量操作对象
    // 参数count：操作总数
    void WriteBatchInternal::SetCount(WriteBatch *w, int count)
    {
        EncodeFixed32(&w->rep_[8], count); // 编码到头部4字节计数位置
    }

    // 获取批量操作的起始序列号
    // 参数w：批量操作对象
    // 返回值：起始序列号
    SequenceNumber WriteBatchInternal::Sequence(WriteBatch *w)
    {
        return SequenceNumber(DecodeFixed64(w->rep_.data())); // 解析头部8字节序列号
    }

    // 设置批量操作的起始序列号
    // 参数w：批量操作对象
    // 参数seq：起始序列号
    void WriteBatchInternal::SetSequence(WriteBatch *w, SequenceNumber seq)
    {
        EncodeFixed64(w->rep_.data(), seq); // 编码到头部8字节序列号位置
    }

    // 将源批量操作追加到目标批量操作
    // 参数src：源批量操作
    // 参数dst：目标批量操作
    void WriteBatchInternal::Append(const WriteBatch *src, WriteBatch *dst)
    {
        // 更新目标操作总数（源+目标）
        SetCount(dst, Count(src) + Count(dst));
        assert(src->rep_.size() >= kWriteBatchHeader); // 确保源头部有效

        // 追加源的操作内容（跳过头部）
        dst->rep_.append(src->rep_.data() + kWriteBatchHeader,
                         src->rep_.size() - kWriteBatchHeader);
    }

}; // namespace koishidb

//为了提升put和delete的速度(锁竞争),我们把多个的put和delete操作封装为一个writebatch对象(包括序列号,操作次数,多个操作(操作类型+用户键+值)),再一起写入内存