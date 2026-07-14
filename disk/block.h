// 该文件定义了Block结构体，用于表示SSTable中的数据块，存储块的原始数据及大小
#ifndef KOISHIDB_SRC_INCLUDE_DISK_BLOCK_H
#define KOISHIDB_SRC_INCLUDE_DISK_BLOCK_H

#include <memory>

#include "slice.h"

namespace koishidb
{
    // 数据块结构体，用于封装SSTable中的一个数据块
    struct Block
    {
        // 接管 ReadBlock 分配的缓冲区，保证迭代器生命周期内数据始终有效。
        Block(std::unique_ptr<char[]> block_data, size_t block_size)
            : owned_data(std::move(block_data)), data(owned_data.get()), size(block_size)
        {

        }

        // 默认构造函数：仅用于测试场景
        Block() = default;

        std::unique_ptr<char[]> owned_data; // 拥有块的底层缓冲区
        const char *data = nullptr;         // 指向块的原始数据
        size_t size = 0;                    // 块的大小（字节数）
    };

}; // namespace koishidb

#endif

//定义了数据块在内存中的表示结构