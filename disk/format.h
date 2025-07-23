// 该文件定义了磁盘存储相关的数据结构和格式，包括文件元信息、块句柄、Footer等，用于SSTable文件的组织与解析
//SSTable文件包括:数据块(存储真实数据),元数据块(存储与数据相关的辅助信息),索引块(数据块或元数据块的偏移和大小),Footer(索引块和过滤器块的句柄)
#ifndef KOISHIDB_SRC_INCLUDE_DISK_FORMAT_H
#define KOISHIDB_SRC_INCLUDE_DISK_FORMAT_H

#include <optional>

#include "common.h"
#include "status.h"
#include "sstable.h"
#include "writable_file.h"
#include "key.h"
#include "slice.h"
#include "encode.h"

namespace koishidb
{
    // 一个SSTable(Sorted String Table)文件的元信息结构体，记录文件的基本属性
    struct FileMeta
    {
        uint64_t file_size;       // 文件大小（字节）
        uint64_t number;          // SSTable编号编号（唯一标识）
        InternalKey smallest_key; // 文件中最小的内部键
        InternalKey largest_key;  // 文件中最大的内部键
    };

    // 块句柄结构体，用于定位SSTable文件中一个数据块的位置和大小
    class BlockHandle
    {
    public:
        // 块句柄编码后的最大长度
        enum
        {
            kMaxEncodedLength = 10 + 10
        };

        // 构造函数：初始化偏移量和大小为最大值（表示未初始化）
        BlockHandle();

        // 获取块在文件中的偏移量
        uint64_t offset() const
        {
            return offset_;
        }

        // 设置块在文件中的偏移量
        void set_offset(uint64_t offset)
        {
            offset_ = offset;
        }

        // 获取存储的块的大小
        uint64_t size() const
        {
            return size_;
        }

        // 设置存储的块的大小
        void set_size(uint64_t size)
        {
            size_ = size;
        }

        // 将块句柄编码并追加到字符串
        void EncodeTo(std::string *dst) const;

        // 从Slice中解码块句柄
        Status DecodeFrom(Slice *input);

    private:
        uint64_t offset_; // 一个数据块在文件中的偏移量
        uint64_t size_;   // 一个数据块的大小（字节）
    };

    // 构造函数实现：初始化偏移量和大小为最大值（~0(0取反),即最大值2^64-1,表示未使用）
    inline BlockHandle::BlockHandle(): offset_(~static_cast<uint64_t>(0)), size_(~static_cast<uint64_t>(0))
    {

    }

    // 将块句柄编码并追加到目标字符串
    inline void BlockHandle::EncodeTo(std::string *dst) const
    {
        // 确保所有字段都已被设置（断言）
        assert(offset_ != ~static_cast<uint64_t>(0));
        assert(size_ != ~static_cast<uint64_t>(0));

        // 使用变长编码写入偏移量和大小
        PutVarint64(dst, offset_);
        PutVarint64(dst, size_);
    }

    // 从Slice中解码块句柄
    inline Status BlockHandle::DecodeFrom(Slice *input)
    {
        // 解码偏移量和大小，若成功返回OK状态
        if (GetVarint64(input, &offset_) && GetVarint64(input, &size_))
        {
            return Status::OK();
        }
        else
        {
            return Status::Corruption("bad block handle");
        }
    }

    // SSTable文件的Footer结构体，记录索引块和过滤器块的位置
    class Footer
    {
    public:
        // 构造函数：默认初始化
        Footer() = default;

        // 禁用拷贝构造函数
        Footer(const Footer &) = delete;

        // 禁用赋值运算符
        Footer &operator=(const Footer &) = delete;

        // 设置索引块的句柄
        void set_index(BlockHandle index_block_handle)
        {
            index_block_handle_ = index_block_handle;
        }

        // 设置过滤器块的句柄
        void set_filter(BlockHandle filter_block_handle)
        {
            filter_block_handle_ = filter_block_handle;
        }

        // 获取索引块的句柄
        BlockHandle index_handle()
        {
            return index_block_handle_;
        }

        // 获取过滤器块的句柄
        BlockHandle filter_handle()
        {
            return filter_block_handle_;
        }

        // 将Footer编码并追加到字符串
        void EncodeTo(std::string *dst);

        // 从Slice中解码Footer
        Status DecodeFrom(Slice *s);

    private:
        BlockHandle index_block_handle_;  // 索引块的句柄
        BlockHandle filter_block_handle_; // 过滤器块的句柄
    };

    // 将Footer编码并追加到目标字符串
    inline void Footer::EncodeTo(std::string *dst)
    {
        int begin = dst->size();
        // 编码索引块句柄和过滤器块句柄
        index_block_handle_.EncodeTo(dst);
        filter_block_handle_.EncodeTo(dst);
        // 填充空白字节，确保Footer总大小为kFixedFooterSize
        int paddings = kFixedFooterSize - (dst->size() - begin);
        dst->append(paddings, static_cast<char>(0));
    }

    // 从Slice中解码Footer
    inline Status Footer::DecodeFrom(Slice *input)
    {
        // 先解码索引块句柄
        auto s = index_block_handle_.DecodeFrom(input);
        if (!s.ok())
        {
            return s;
        }
        // 再解码过滤器块句柄
        s = filter_block_handle_.DecodeFrom(input);
        return s;
    }

    // 根据块句柄从文件中读取对应的块内容
    // 参数file: 随机访问文件对象
    // 参数blockHandle: 块句柄（包含偏移量和大小）
    // 返回值: 包含块内容的智能指针（可选，失败时为空）
    std::optional<std::unique_ptr<BlockContent>> ReadBlock(
        RandomAccessFile *file, const BlockHandle &blockHandle);

    // 打印格式化的文件元信息（用于调试）
    void PrintFileMeta(FileMeta &meta);

    // 将文件元信息编码并写入可写文件
    void EncodeFileMeta(FileMeta *file_meta, WritableFile &file);

    // 从Slice中解码文件元信息（会自动推进Slice指针）
    // 参数file_meta: 存储解码结果的指针的指针
    // 参数slice: 输入Slice（会被修改，指针推进）
    void DecodeFileMeta(FileMeta **file_meta, Slice *slice);

}; // namespace koishidb

#endif

//对SSTable文件格式的基础定义,包括FileMeta(文件元信息),BlockHandle(某个块的句柄:块的偏移和大小),Footer(索引块和过滤器块的BlockHandle)