// 该文件定义了Block结构体，用于表示SSTable中的数据块，存储块的原始数据及大小
#ifndef KOISHIDB_SRC_INCLUDE_DISK_BLOCK_H
#define KOISHIDB_SRC_INCLUDE_DISK_BLOCK_H

#include "slice.h"

namespace koishidb
{
    // 数据块结构体，用于封装SSTable中的一个数据块
    struct Block
    {
        // 构造函数：通过Slice初始化块数据
        // 参数block_data：包含块原始数据的Slice（指向外部数据，不负责内存管理）
        Block(const Slice *block_data): data(block_data->data()), size(block_data->size())
        {

        }

        // 默认构造函数：仅用于测试场景
        Block() = default;

        const char *data; // 指向块的原始数据（底层数据的起始地址）
        size_t size;      // 块的大小（字节数）
    };

}; // namespace koishidb

#endif

//定义了数据块在内存中的表示结构