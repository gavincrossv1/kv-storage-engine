// 该文件实现了各种工具函数，包括内部键处理、内存表键值操作、文件删除等功能
#include "util.h"

#include <fcntl.h>
#include <unistd.h>

#include "encode.h"

namespace koishidb
{

    // 从内存表键中提取内部键
    // 参数memtable_key：内存表中的键（包含内部键和值等信息）
    // 参数internal_key：存储提取出的内部键
    void ExtractInternalKey(Slice memtable_key, Slice *internal_key)
    {
        // 内存表键的格式：[内部键长度(varint32)][内部键内容][值长度(varint32)][值内容]
        uint32_t internal_key_len;
        GetVarint32(&memtable_key, &internal_key_len);                // 读取内部键长度
        GetFixedBytes(&memtable_key, internal_key, internal_key_len); // 读取内部键内容
    }

    // 从内部键中提取用户键和序列号
    // 参数internal_key：内部键（格式：[用户键][8字节标记(序列号+类型)]）
    // 参数user_key：存储提取出的用户键
    // 参数number：存储提取出的序列号
    void ExtractUserKey(const Slice &internal_key, Slice *user_key,
                        SequenceNumber *number)
    {
        // 用户键长度 = 内部键长度 - 8字节标记
        *user_key = Slice(internal_key.data(), internal_key.size() - 8);

        // 从标记中提取序列号（高56位，右移8位去除类型）
        *number = (*reinterpret_cast<const SequenceNumber *>(internal_key.data() + user_key->size()) >> 8);
    }

    // 创建内部键（用户键 + 序列号 + 键类型）
    // 参数user_key：用户键
    // 参数snapshot：序列号
    // 参数type：键类型（kTypeValue或kTypeDeletion）
    // 返回值：创建的内部键（需手动释放内存）
    Slice CreateInternalKey(const Slice &user_key, SequenceNumber snapshot,
                            KeyType type)
    {
        size_t usize = user_key.size();
        size_t needed = usize + 8;                                    // 用户键长度 + 8字节标记
        SequenceNumber tag = snapshot << 8 | static_cast<char>(type); // 组合序列号和类型

        // 分配内存存储内部键
        char *dst = new char[needed];
        memcpy(dst, user_key.data(), usize);                    // 复制用户键
        *reinterpret_cast<SequenceNumber *>(dst + usize) = tag; // 存储标记

        return Slice(dst, needed);
    }

    // 创建内存表键（包含内部键和值）
    // 参数user_key：用户键
    // 参数snapshot：序列号
    // 参数value：值
    // 参数key_type：键类型
    // 返回值：创建的内存表键（需手动释放内存）
    Slice CreateMemtableKey(const Slice &user_key, SequenceNumber snapshot,
                            const Slice &value, KeyType key_type)
    {
        size_t usize = user_key.size();
        size_t vsize = value.size();
        size_t needed = usize + 8 + vsize + 10; // 预留足够内存（包含变长编码空间）
        char *dst = new char[needed];//要在外部释放

        // 编码内部键长度（用户键长度 + 8字节标记）
        size_t advance = EncodeVarint32(usize + 8, dst);

        // 复制用户键
        memcpy(dst + advance, user_key.data(), usize);

        // 编码标记（序列号 + 类型）
        uint64_t tag = snapshot << 8 | static_cast<char>(key_type);
        const char *ptr = reinterpret_cast<const char *>(&tag);
        for (int i = 0; i < 8; i++)
        {
            dst[advance + usize + i] = ptr[i];
        }

        // 编码值长度并复制值
        char *ptr2 = dst + advance + usize + 8;
        size_t advance2 = EncodeVarint32(vsize, ptr2);
        memcpy(ptr2 + advance2, value.data(), vsize);

        // 计算总长度并返回内存表键
        size_t size = advance + usize + 8 + advance2 + vsize;
        return Slice(dst, size);
    }

    // 从内存表键中提取值
    // 参数memtable_key：内存表键
    // 参数result：存储提取出的值
    void ExtractValueFromMemtable(Slice memtable_key, std::string *result)
    {
        // 跳过内部键部分
        uint32_t internal_key_len;
        GetVarint32(&memtable_key, &internal_key_len);
        memtable_key.Advance(internal_key_len);

        // 读取值长度并提取值
        uint32_t value_len;
        GetVarint32(&memtable_key, &value_len);
        result->assign(memtable_key.data(), value_len);
    }

    // 删除指定的文件
    // 参数file_name：文件名
    // 返回值：成功返回true，失败返回false
    bool Remove(const std::string &file_name)
    {
        int result = ::remove(file_name.data());
        if (result == -1)
        {
            return false;
        }
        return true;
    }

} // namespace koishidb

//实现内存表键(键在内存中真正的存储格式)和内部键相关的操作