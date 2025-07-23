#ifndef KOISHIDB_SRC_INCLUDE_DISK_BLOCK_BUILDER_H
#define KOISHIDB_SRC_INCLUDE_DISK_BLOCK_BUILDER_H
#include "slice.h"

namespace koishidb
{
    // 前向声明Option结构体
    struct Option;

    // 块构建器类，用于构建SSTable中的数据块
    class BlockBuilder
    {
    public:
        // 块数据格式：
        // 变长整数 -> 键的长度
        // 内部键(internal_key)
        // 变长整数 -> 值的长度
        // 值(value)
        // const对象只能被复制到const对象
        BlockBuilder(const Option *opt) : opt_(opt) 
        {

        }

        // 向块中添加键值对
        void Add(const Slice &key, const Slice &value);

        // 估计当前块的大小（字节数）
        size_t EstimatedSize();

        // 调用此方法完成块的构建，返回构建好的块数据
        Slice Finish();

        // 重置块构建器状态，准备构建新的块
        void Reset();

        // 判断当前块是否为空（没有添加任何数据）
        bool Empty() 
        { 
            return this->EstimatedSize() == 0; 
        }

        // 获取当前块构建器使用的配置选项
        const Option *GetOption() 
        { 
            return opt_; 
        }

        // 仅用于单元测试：获取当前构建的原始数据
        std::string Data() 
        { 
            return rep_; 
        }

    private:
        std::string rep_;          // 用于存储构建的块数据的缓冲区
        std::string last_key_;     // 上一个添加的键，用于确保键的有序性
        const Option *opt_;        // 配置选项，包含比较器等信息
        bool is_finished_;         // 标记块是否已完成构建
    };
}; // namespace koishidb

#endif

//对内存中数据块的先行构建,将已经全局有序的Immutable MemTable转换为磁盘格式,此时数据块中的数据格式就是内存表键的格式(注意,尽管看起来似乎和内存表没什么区别,但block_builder到物理存储用的是紧凑的线性字节缓冲区,而非跳表,到时候可以直接刷盘到磁盘上)