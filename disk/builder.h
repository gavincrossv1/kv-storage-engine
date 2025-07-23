// 该文件声明了BuildTable函数，用于根据迭代器中的数据构建SSTable文件，并更新文件元信息
#ifndef KOISHIDB_SRC_INCLUDE_DISK_BUILDER_H
#define KOISHIDB_SRC_INCLUDE_DISK_BUILDER_H

#include <string>

#include "status.h"
#include "format.h"
#include "iterator.h"

namespace koishidb
{
    // 使用迭代器中的数据构建SSTable文件
    // 后续可能会添加一些配置选项
    // 生成表文件并将文件元信息写入清单(manifest)
    // 参数file_name: 要生成的SSTable文件名称
    // 参数file_meta: 用于存储生成的SSTable文件元信息的指针
    // 参数iter: 提供数据的迭代器（通常是内存表的迭代器）
    // 返回值: 操作状态（成功或错误信息）
    Status BuildTable(const std::string &file_name, FileMeta *file_meta,Iterator *iter);
}; // namespace koishidb

#endif

//构建SSTable文件