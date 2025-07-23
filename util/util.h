// 该文件声明了各种工具函数和辅助类，包括键处理、随机数生成、文件操作等功能
#ifndef KOISHIDB_SRC_INCLUDE_UTIL_UTIL_H
#define KOISHIDB_SRC_INCLUDE_UTIL_UTIL_H

#include <cstdlib>
#include <ctime>

#include "common.h"
#include "slice.h"
#include "encode.h"

// 内部件组成:[ varint32(internal_key_len) ] [ InternalKey ] [ varint32(value_len) ] [ Value ](内部键长度加内部键加值长度加值)
// varint32(internal_key_len)：变长编码的 32 位整数，表示 InternalKey 的长度。
// InternalKey：包含用户键（UserKey）+ 序列号（SequenceNumber）+ 键类型（KeyType）。
// varint32(value_len)：变长编码的 32 位整数，表示 Value 的长度。
// Value：实际存储的值数据。

namespace koishidb
{

    // 辅助类：用于解析内存表键的结构，提取内部键、用户键、值等部分
    class LookupKey
    {
    public:
        // 构造函数：解析内存表键
        // 参数memtable_key：内存表键
        LookupKey(const Slice &memtable_key) : data_(memtable_key.data()), size_(memtable_key.size())
        {
            Slice tmp = memtable_key;
            uint32_t internal_key_len;
            GetVarint32(&tmp, &internal_key_len); // 读取内部键长度

            // 计算各部分的偏移量
            kstart_ = tmp.data() - data_;       // 内部键起始位置
            kend_ = kstart_ + internal_key_len; // 内部键结束位置

            tmp.Advance(internal_key_len); // 跳过内部键
            uint32_t value_len;
            GetVarint32(&tmp, &value_len); // 读取值长度
            vstart_ = tmp.data() - data_;  // 值起始位置
        }

        // 获取完整的内存表键
        Slice MemtableKey() { return Slice(data_, size_); }

        // 获取内部键部分
        Slice InternalKey() { return Slice(data_ + kstart_, kend_ - kstart_); }

        // 获取用户键部分（内部键去除8字节标记）
        Slice UserKey()
        {
            return Slice(data_ + kstart_, kend_ - kstart_ - 8);
        }

        // 获取值部分
        Slice Value() { return Slice(data_ + vstart_, size_ - vstart_); }

        // 从内部键中获取序列号
        SequenceNumber GetSequence()
        {
            return (*reinterpret_cast<const SequenceNumber *>(data_ + kend_ - 8) >> 8);
        }

        // 从内部键中获取键类型
        KeyType GetKeyType()
        {
            uint64_t tag = *reinterpret_cast<const SequenceNumber *>(data_ + kend_ - 8);
            tag = tag & (0xff); // 取低8位（类型）
            if (tag == 0x01)
            {
                return KeyType::kTypeValue; // 值类型
            }
            else
            {
                return KeyType::kTypeDeletion; // 删除标记
            }
        }

    private:
        const char *data_;  // 内存表键数据指针
        const size_t size_; // 内存表键总长度
        uint32_t kstart_;   // 内部键起始偏移量
        uint32_t kend_;     // 内部键结束偏移量
        uint32_t vstart_;   // 值起始偏移量
    };

    // 从内存表键中提取内部键
    // 参数metable_key：内存表键
    // 参数internal_key：存储提取出的内部键
    void ExtractInternalKey(Slice metable_key, Slice *internal_key);

    // 从内部键中提取用户键和序列号
    // 参数internal_key：内部键
    // 参数user_key：存储提取出的用户键
    // 参数number：存储提取出的序列号
    void ExtractUserKey(const Slice &internal_key, Slice *user_key,
                        SequenceNumber *number);

    // 创建内部键（用户键 + 序列号 + 类型）
    // 参数user_key：用户键
    // 参数snapshot：序列号
    // 参数type：键类型
    // 返回值：创建的内部键
    Slice CreateInternalKey(const Slice &user_key, SequenceNumber snapshot,
                            KeyType type);

    // 创建内存表键（包含内部键和值）
    // 参数user_key：用户键
    // 参数snapshot：序列号
    // 参数value：值
    // 参数type：键类型
    // 返回值：创建的内存表键
    Slice CreateMemtableKey(const Slice &user_key, SequenceNumber snapshot,
                            const Slice &value, KeyType type);

    // 从内存表键中提取值
    // 参数memtable_key：内存表键
    // 参数result：存储提取出的值
    void ExtractValueFromMemtable(Slice memtable_key, std::string *result);

    // 初始化随机数生成器（用于跳表节点高度）
    inline void rng() { srand(time(NULL)); }

    // 生成跳表节点的随机高度（1到最大层级之间）
    inline int RandomHeight()
    {
        int height = 1;
        while (height < kSkipListNodeMaxLevel && rand() % 2 == 0)
        {
            height++;
        }
        return height;
    }

    // 删除指定的文件
    // 参数file_name：文件名
    // 返回值：成功返回true，失败返回false
    bool Remove(const std::string &file_name);
}; // namespace koishidb

#endif


//定义内存表键(键在内存中真正的存储格式)和内部键相关的操作