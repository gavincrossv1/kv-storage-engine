// 该文件声明了多种数据编码/解码的工具函数，包括变长整数(varint)、固定长度整数的编解码，以及长度前缀切片的处理等，用于高效压缩和解析数据
#ifndef KOISHIDB_SRC_INCLUDE_UTIL_ENCODE_H
#define KOISHIDB_SRC_INCLUDE_UTIL_ENCODE_H

// 使用变长整数(varint)压缩数据以节省内存
#include <cassert>
#include <cstddef>

#include "slice.h"

namespace koishidb
{

    // 将32位无符号整数编码为变长整数格式
    // 参数value: 待编码的32位整数
    // 参数dst: 存储编码结果的缓冲区
    // 返回值: 编码后的字节数
    size_t EncodeVarint32(uint32_t value, char *dst);

    // 计算32位无符号整数编码为变长格式后的字节数
    // 参数value: 待计算的32位整数
    // 返回值: 编码后的字节数
    size_t EncodeVarint32Length(uint32_t value);

    // 将64位无符号整数编码为变长整数格式
    // 参数dst: 存储编码结果的缓冲区
    // 参数v: 待编码的64位整数
    // 返回值: 指向编码后数据末尾的指针
    char *EncodeVarint64(char *dst, uint64_t v);

    // 将64位无符号整数编码为变长格式并追加到字符串
    // 参数dst: 目标字符串
    // 参数v: 待编码的64位整数
    void PutVarint64(std::string *dst, uint64_t v);

    // 将变长格式的32位整数解码为普通32位整数
    // 参数dst: 指向变长编码数据的指针
    // 参数value: 存储解码结果的指针
    // 返回值: 指向解码后剩余数据的指针
    const char *DecodeVarint32(const char *dst, uint32_t *value);

    // 从指定内存区域解码64位变长整数
    // 参数p: 指向变长编码数据的起始指针
    // 参数limit: 内存区域的结束边界
    // 参数value: 存储解码结果的指针
    // 返回值: 指向解码后剩余数据的指针，解码失败返回nullptr
    const char *GetVarint64Ptr(const char *p, const char *limit, uint64_t *value);

    // 从Slice中解码64位变长整数，并自动推进Slice的指针
    // 参数input: 输入Slice（会被修改，指针推进）
    // 参数value: 存储解码结果的指针
    // 返回值: 解码成功返回true，失败返回false
    bool GetVarint64(Slice *input, uint64_t *value);

    // 将32位无符号整数编码为变长格式并追加到字符串
    // 参数value: 待编码的32位整数
    // 参数dst: 目标字符串
    void PutVarint32(uint32_t value, std::string *dst);

    // 从Slice中解码32位变长整数，并自动推进Slice的指针
    // 参数dst: 输入Slice（会被修改，指针推进）
    // 参数value: 存储解码结果的指针
    void GetVarint32(Slice *dst, uint32_t *value);

    // 从Slice中提取固定长度的字节到结果Slice，并推进输入Slice的指针
    // 参数dst: 输入Slice（会被修改，指针推进）
    // 参数result: 存储提取结果的Slice
    // 参数n: 要提取的字节数
    void GetFixedBytes(Slice *dst, Slice *result, size_t n);

    // 将Slice编码为长度前缀格式（变长长度 + 数据）并追加到字符串
    // 参数dst: 目标字符串
    // 参数value: 待编码的Slice
    void PutLengthPrefixedSlice(std::string *dst, const Slice &value);

    // 从Slice中解码长度前缀格式的数据（变长长度 + 数据），并推进输入Slice的指针
    // 参数dst: 存储解码后数据的字符串
    // 参数src: 输入Slice（会被修改，指针推进）
    void GetLengthPrefixedSlice(std::string *dst, Slice *src);

    // 解码固定32位小端整数
    // 参数ptr: 指向4字节数据的指针
    // 返回值: 解码后的32位整数
    uint32_t DecodeFixed32(const char *ptr);

    // 解码固定64位小端整数
    // 参数ptr: 指向8字节数据的指针
    // 返回值: 解码后的64位整数
    uint64_t DecodeFixed64(const char *ptr);

    // 将32位整数编码为固定4字节小端格式并追加到字符串
    // 参数dst: 目标字符串
    // 参数value: 待编码的32位整数
    void PutFixed32(std::string *dst, uint32_t value);

    // 将64位整数编码为固定8字节小端格式并追加到字符串
    // 参数dst: 目标字符串
    // 参数value: 待编码的64位整数
    void PutFixed64(std::string *dst, uint64_t value);

    // 将32位整数编码为固定4字节小端格式
    // 参数dst: 存储编码结果的缓冲区（至少4字节）
    // 参数value: 待编码的32位整数
    void EncodeFixed32(char *dst, uint32_t value);

    // 将64位整数编码为固定8字节小端格式
    // 参数dst: 存储编码结果的缓冲区（至少8字节）
    // 参数value: 待编码的64位整数
    void EncodeFixed64(char *dst, uint64_t value);

}; // namespace koishidb

#endif

//实现了32位整数,64位整数到变长编码(varint)和固定编码(fixed)的编码和解码