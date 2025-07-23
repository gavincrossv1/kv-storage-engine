// 该文件实现了多种数据编码/解码的工具函数，包括变长整数(varint)、固定定长整数的编解码，以及长度前缀缀切片的处理等
#include "encode.h"

namespace koishidb
{

    // 将32位无符号整数编码为变长整数格式
    // 参数value: 待编码的32位整数
    // 参数dst: 存储编码结果的缓冲区
    // 返回值: 编码后的字节数
    size_t EncodeVarint32(uint32_t value, char *dst)
    {
        uint8_t *ptr = reinterpret_cast<uint8_t *>(dst);
        size_t size = 0;
        static const uint8_t B = 128; // 用于标记后续还有字节

        while (true) // 依次循环处理一个字节
        {
            size++;
            if (value < B)
            {
                // 值小于128，无需后续字节，直接存储(第八位标记为零)
                *(ptr++) = value;
                break;
            }
            else
            {
                // 存储低7位并设置最高位为1（标记后续有字节）,0x7F为01111111,B为10000000
                *(ptr++) = (value & 0x7F) | B;
                value >>= 7; // 右移7位处理剩余部分
            }
        }
        return size;
    }
    // 这种编码方式对于小数值非常节省空间：
    //  0-127: 1字节
    //  128-16383: 2字节
    //  16384-2097151: 3字节
    //  2097152-268435455: 4字节
    //  更大的数: 5字节

    // 将64位无符号整数编码为变长整数格式
    // 参数dst: 存储编码结果的缓冲区
    // 参数v: 待编码的64位整数
    // 返回值: 指向编码后数据末尾的指针
    char *EncodeVarint64(char *dst, uint64_t v)
    {
        static const int B = 128; // 用于标记后续还有字节
        uint8_t *ptr = reinterpret_cast<uint8_t *>(dst);

        while (v >= B)
        {
            // 存储低7位并设置最高位为1（标记后续有字节）
            *(ptr++) = v | B;
            v >>= 7; // 右移7位处理剩余部分
        }

        // 存储最后一个字节（无需标记后续字节）
        *(ptr++) = static_cast<uint8_t>(v);
        return reinterpret_cast<char *>(ptr);
    }

    // 将32位无符号整数编码为变长格式并追加到字符串
    // 参数value: 待编码的32位整数
    // 参数dst: 目标字符串
    void PutVarint32(uint32_t value, std::string *dst)
    {
        char buf[5]; // 32位变长整数最大需要5字节
        size_t advance = EncodeVarint32(value, buf);
        dst->append(buf, advance); // 追加有效字节
    }

    // 将64位无符号整数编码为变长格式并追加到字符串
    // 参数dst: 目标字符串
    // 参数v: 待编码的64位整数
    void PutVarint64(std::string *dst, uint64_t v)
    {
        char buf[10]; // 64位变长整数最大需要10字节
        char *ptr = EncodeVarint64(buf, v);
        dst->append(buf, ptr - buf); // 追加有效字节
    }

    // 计算32位无符号整数编码为变长格式后的字节数(只计算字节数,不真的进行编码)
    // 参数value: 待计算的32位整数
    // 返回值: 编码后的字节数
    size_t EncodeVarint32Length(uint32_t value)
    {
        static const uint8_t B = 128;
        size_t size = 0;

        while (true)
        {
            size++;
            if (value < B)
            {
                break;
            }
            else
            {
                value >>= 7; // 右移7位继续计算
            }
        }

        return size;
    }

    // 将变长格式的32位整数解码为普通32位整数
    // 参数dst: 指向变长编码数据的指针
    // 参数value: 存储解码结果的指针
    // 返回值: 指向解码后剩余数据的指针
    // TODO: 需要处理无效数据的情况
    const char *DecodeVarint32(const char *dst, uint32_t *value)
    {
        // 32位变长整数最多占用5字节
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(dst);
        uint32_t part0 = *(ptr++);

        if (part0 < 0x80)
        {
            // 单字节情况
            *value = part0;
            return reinterpret_cast<const char *>(ptr);
        }

        // 多字节情况，逐步解析
        part0 -= 0x80; // 清除标记位
        uint32_t part1 = *(ptr++);
        part0 += part1 << 7;

        if (part1 < 0x80)
        {
            *value = part0;
            return reinterpret_cast<const char *>(ptr);
        }

        part0 -= 0x80 << 7; // 清除标记位
        uint32_t part2 = *(ptr++);
        part0 += part2 << 14;

        if (part2 < 0x80)
        {
            *value = part0;
            return reinterpret_cast<const char *>(ptr);
        }

        part0 -= 0x80 << 14; // 清除标记位
        uint32_t part3 = *(ptr++);
        part0 += part3 << 21;

        if (part3 < 0x80)
        {
            *value = part0;
            return reinterpret_cast<const char *>(ptr);
        }

        part0 -= 0x80 << 21; // 清除标记位
        uint32_t part4 = *(ptr++);
        part0 += part4 << 28;

        *value = part0;
        return reinterpret_cast<const char *>(ptr);
    }

    // 从Slice中解码32位变长整数，并自动推进Slice的指针
    // 参数dst: 输入Slice（会被修改，指针推进）
    // 参数value: 存储解码结果的指针
    void GetVarint32(Slice *dst, uint32_t *value)
    {
        const char *data = dst->data();
        const char *limit = data + dst->size();
        const char *q = DecodeVarint32(data, value);
        *dst = Slice(q, limit - q); // 更新Slice指向剩余数据
    }

    // 从指定内存区域解码64位变长整数
    // 参数p: 指向变长编码数据的起始指针
    // 参数limit: 内存区域的结束边界
    // 参数value: 存储解码结果的指针
    // 返回值: 指向解码后剩余数据的指针，解码失败返回nullptr
    const char *GetVarint64Ptr(const char *p, const char *limit, uint64_t *value)
    {
        uint64_t result = 0;

        // 最多处理64位（8字节），每次处理7位
        for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7)
        {
            uint64_t byte = *(reinterpret_cast<const uint8_t *>(p));
            p++;

            if (byte & 128)
            {
                // 最高位为1，后续还有字节
                result |= ((byte & 127) << shift);
            }
            else
            {
                // 最高位为0，最后一个字节
                result |= (byte << shift);
                *value = result;
                return reinterpret_cast<const char *>(p);
            }
        }

        // 超出边界或数据无效
        return nullptr;
    }

    // 从Slice中解码64位变长整数，并自动推进Slice的指针
    // 参数input: 输入Slice（会被修改，指针推进）
    // 参数value: 存储解码结果的指针
    // 返回值: 解码成功返回true，失败返回false
    bool GetVarint64(Slice *input, uint64_t *value)
    {
        const char *p = input->data();
        const char *limit = p + input->size();
        const char *q = GetVarint64Ptr(p, limit, value);

        if (q == nullptr)
        {
            return false;
        }
        else
        {
            *input = Slice(q, limit - q); // 更新Slice指向剩余数据
            return true;
        }
    }

    // 从Slice中提取固定长度的字节到结果Slice，并推进输入Slice的指针
    // 参数dst: 输入Slice（会被修改，指针推进）
    // 参数result: 存储提取结果的Slice
    // 参数n: 要提取的字节数
    void GetFixedBytes(Slice *dst, Slice *result, size_t n)
    {
        assert(n <= dst->size());                       // 确保有足够的字节
        *result = Slice(dst->data(), n);                // 提取n字节
        *dst = Slice(dst->data() + n, dst->size() - n); // 更新输入Slice
    }

    // 将Slice编码为长度前缀格式（变长长度 + 数据）并追加到字符串
    // 参数dst: 目标字符串
    // 参数value: 待编码的Slice
    void PutLengthPrefixedSlice(std::string *dst, const Slice &value)
    {
        PutVarint32(value.size(), dst);          // 先写入长度（变长编码）
        dst->append(value.data(), value.size()); // 再写入数据
    }

    // 从Slice中解码长度前缀格式的数据（变长长度 + 数据），并推进输入Slice的指针
    // 参数dst: 存储解码后数据的字符串
    // 参数src: 输入Slice（会被修改，指针推进）
    void GetLengthPrefixedSlice(std::string *dst, Slice *src)
    {
        uint32_t len;
        GetVarint32(src, &len);        // 先解码长度
        dst->append(src->data(), len); // 再提取对应长度的数据
        src->Advance(len);             // 推进输入Slice的指针
    }

    // 解码固定32位小端整数
    // 参数ptr: 指向4字节数据的指针
    // 返回值: 解码后的32位整数
    uint32_t DecodeFixed32(const char *ptr)
    {
        const uint8_t *const buffer = reinterpret_cast<const uint8_t *>(ptr);
        return (static_cast<uint32_t>(buffer[0])) |
               (static_cast<uint32_t>(buffer[1]) << 8) |
               (static_cast<uint32_t>(buffer[2]) << 16) |
               (static_cast<uint32_t>(buffer[3]) << 24);
    }

    // 解码固定64位小端整数
    // 参数ptr: 指向8字节数据的指针
    // 返回值: 解码后的64位整数
    uint64_t DecodeFixed64(const char *ptr)
    {
        const uint8_t *const buffer = reinterpret_cast<const uint8_t *>(ptr);
        return (static_cast<uint64_t>(buffer[0])) |
               (static_cast<uint64_t>(buffer[1]) << 8) |
               (static_cast<uint64_t>(buffer[2]) << 16) |
               (static_cast<uint64_t>(buffer[3]) << 24) |
               (static_cast<uint64_t>(buffer[4]) << 32) |
               (static_cast<uint64_t>(buffer[5]) << 40) |
               (static_cast<uint64_t>(buffer[6]) << 48) |
               (static_cast<uint64_t>(buffer[7]) << 56);
    }

    // 将32位整数编码为固定4字节小端格式
    // 参数dst: 存储编码结果的缓冲区（至少4字节）
    // 参数value: 待编码的32位整数
    void EncodeFixed32(char *dst, uint32_t value)
    {
        uint8_t *const buffer = reinterpret_cast<uint8_t *>(dst);
        buffer[0] = static_cast<uint8_t>(value);
        buffer[1] = static_cast<uint8_t>(value >> 8);
        buffer[2] = static_cast<uint8_t>(value >> 16);
        buffer[3] = static_cast<uint8_t>(value >> 24);
    }

    // 将64位整数编码为固定8字节小端格式
    // 参数dst: 存储编码结果的缓冲区（至少8字节）
    // 参数value: 待编码的64位整数
    void EncodeFixed64(char *dst, uint64_t value)
    {
        uint8_t *const buffer = reinterpret_cast<uint8_t *>(dst);
        buffer[0] = static_cast<uint8_t>(value);
        buffer[1] = static_cast<uint8_t>(value >> 8);
        buffer[2] = static_cast<uint8_t>(value >> 16);
        buffer[3] = static_cast<uint8_t>(value >> 24);
        buffer[4] = static_cast<uint8_t>(value >> 32);
        buffer[5] = static_cast<uint8_t>(value >> 40);
        buffer[6] = static_cast<uint8_t>(value >> 48);
        buffer[7] = static_cast<uint8_t>(value >> 56);
    }

    // 将32位整数编码为固定4字节小端格式并追加到字符串
    // 参数dst: 目标字符串
    // 参数value: 待编码的32位整数
    void PutFixed32(std::string *dst, uint32_t value)
    {
        char buf[sizeof(value)]; // 4字节缓冲区
        EncodeFixed32(buf, value);
        dst->append(buf, sizeof(buf)); // 追加4字节
    }

    // 将64位整数编码为固定8字节小端格式并追加到字符串
    // 参数dst: 目标字符串
    // 参数value: 待编码的64位整数
    void PutFixed64(std::string *dst, uint64_t value)
    {
        char buf[sizeof(value)]; // 8字节缓冲区
        EncodeFixed64(buf, value);
        dst->append(buf, sizeof(buf)); // 追加8字节
    }

}; // namespace koishidb

//实现了32位整数,64位整数到变长编码(varint)和固定编码(fixed)的编码和解码