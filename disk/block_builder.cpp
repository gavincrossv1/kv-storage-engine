#include "block_builder.h"

#include "common.h"
#include "option.h"
#include "comparator.h"
#include "encode.h"

// sstable-> block：SSTable由多个block组成
// internal_key -> 内部键（包含用户键、序列号和操作类型的组合键）

namespace koishidb
{

    // Key -> Internal_key -> 转换关系：用户键会被转换为内部键存储
    // Value -> might be empty：值可能为空（例如删除操作）
    // 使用Add方法时无需维护key/value的生命周期（由BlockBuilder管理）
    void BlockBuilder::Add(const Slice &key, const Slice &value)
    {
        // 数据格式 -> varint: Internal_key.size(), value.size()：先存储键和值的长度（变长编码）
        // 然后存储key, value的实际内容
        Slice last_key = Slice(last_key_); // 将最后一个键转换为Slice视图

        // 断言：确保新添加的键大于上一个键（维护有序性）
        // 空的last_key表示这是第一个键，无需比较
        assert(last_key.empty() || opt_->cmp->Compare(key, last_key) == 1);

        // 将键的长度以varint32格式写入缓冲区
        PutVarint32(key.size(), &rep_);
        // 将键的实际内容写入缓冲区
        rep_.append(key.data(), key.size());

        // 将值的长度以varint32格式写入缓冲区
        PutVarint32(value.size(), &rep_);
        // 将值的实际内容写入缓冲区
        rep_.append(value.data(), value.size());

        // 更新最后一个键为当前添加的键（用于下次比较）
        last_key_ = std::string(key.data(), key.size());
    }

    // 估计当前BlockBuilder已构建的块大小（字节数）
    size_t BlockBuilder::EstimatedSize()
    {
        return rep_.size();
    }

    // 重置BlockBuilder状态，准备构建新的块
    void BlockBuilder::Reset()
    {
        is_finished_ = false; // 标记为未完成状态
        rep_.clear();         // 清空缓冲区
        last_key_.clear();    // 清空最后一个键的记录
    }

    // finish often used to do the patch job.
    // 完成块的构建，通常用于执行最后的修补工作
    Slice BlockBuilder::Finish()
    {
        // 如何完成块的构建？此处直接标记完成状态
        is_finished_ = true;
        // 返回构建好的块数据（以Slice视图形式，避免复制）
        return Slice(rep_);
    }
}; // namespace koishidb

//对内存中数据块的先行构建,将已经全局有序的Immutable MemTable转换为磁盘格式,此时数据块中的数据格式就是内存表键的格式(注意,尽管看起来似乎和内存表没什么区别,但block_builder到物理存储用的是紧凑的线性字节缓冲区,而非跳表,到时候可以直接刷盘到磁盘上)