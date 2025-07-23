// 该文件声明了TableBuilder类，定义了构建SSTable（排序字符串表）的接口，包括添加键值对、完成构建等功能
#ifndef KOISHIDB_SRC_INCLUDE_TABLE_BUILDER_H
#define KOISHIDB_SRC_INCLUDE_TABLE_BUILDER_H

#include <cstdint>

#include "status.h"
#include "block_builder.h"
#include "format.h"

namespace koishidb
{
    struct Option;
    class WritableFile;

    // TableBuilder类：用于构建SSTable（排序字符串表），将键值对组织成数据块、索引块和过滤器块
    class TableBuilder
    {
    public:
        // 禁用默认构造函数
        TableBuilder() = delete;

        // 禁用赋值运算符
        TableBuilder &operator=(const TableBuilder &that) = delete;

        // 析构函数
        ~TableBuilder();

        // 构造函数：初始化TableBuilder
        // 参数opt：数据库选项（包含比较器等）
        // 参数file：可写文件（用于写入SSTable内容）
        TableBuilder(const Option *opt, WritableFile *file);

        // 向SSTable添加键值对（键必须按顺序添加）
        // 参数key：键
        // 参数value：值
        void Add(const Slice &key, const Slice &value);

        // 完成SSTable构建，写入所有剩余数据并生成文件尾
        // 返回值：操作状态（成功或错误）
        Status Finish();

        // 放弃SSTable构建（不写入剩余数据）
        void Abandon();

        // 获取已添加的键值对数量
        uint64_t NumEntries() const;

        // 获取当前生成的文件大小
        uint64_t FileSize() const;

        // 获取当前状态
        Status status() const;

    private:
        // 检查当前状态是否正常
        bool ok() const { return status().ok(); }

        // 将块内容写入文件并更新块句柄
        // 参数block_builder：块构建器
        // 参数block_handle：块句柄（用于记录块的偏移量和大小）
        void WriteBlock(BlockBuilder *block_builder, BlockHandle *block_handle);

        // 将当前数据块刷新到文件
        void Flush();

        // 内部数据结构（声明）
        struct Rep;

        Rep *rep_; // 内部数据结构指针（存储构建过程中的临时数据和状态）
    };
}; // namespace koishidb

#endif

// 实现了将有序的键值对序列组织成磁盘上的 SSTable 文件
// 管理三种主要数据结构的构建：
// 数据块：存储实际的键值对
// 索引块：记录数据块的位置信息
// 过滤器块：加速键查询的布隆过滤器

// 最终生成的 SSTable 文件包含以下部分：
// 一个或多个数据块
// 过滤器块
// 索引块
// 文件尾(Footer) - 包含索引块和过滤器块的位置信息

// 添加键值对时需要添加的内容
// 数据块：实际的键值对数据
// 索引条目：每个数据块对应一个索引条目(键 → 块位置)
// 过滤器数据：所有键都会添加到布隆过滤器