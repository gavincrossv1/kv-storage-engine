// 该文件实现了SSTable类，负责SSTable的打开、数据块读取、内部查询及迭代器创建等核心功能
#include "sstable.h"

#include <fcntl.h>
#include <unistd.h>

#include "common.h"
#include "option.h"
#include "block.h"
#include "filterblock_reader.h"
#include "format.h"
#include "random_access_file.h"
#include "sstable_iterator.h"
#include "writable_file.h"
#include "logger.h"

namespace koishidb
{

    // SSTable的内部数据结构，存储SSTable的元信息和块数据
    struct SSTable::Rep
    {
        // 析构函数：释放资源
        ~Rep()
        {
            delete filter_block_reader; // 释放过滤器块读取器
            delete[] filter_data;       // 释放过滤器数据
            delete index_block;         // 释放索引块
        }

        const Option *option;                   // 数据库选项（包含比较器等）
        RandomAccessFile *file;                 // 随机访问文件对象
        Status status;                          // 状态信息
        FilterBlockReader *filter_block_reader; // 过滤器块读取器
        const char *filter_data;                // 过滤器块数据
        Block *index_block;                     // 索引块（存储数据块的句柄）
    };

    // SSTable析构函数：释放内部数据结构
    SSTable::~SSTable()
    {
        delete rep_;
    }

    // 打开SSTable文件并创建SSTable实例
    // 参数opt：数据库选项
    // 参数file：随机访问文件对象
    // 参数file_size：文件大小
    // 返回值：包含SSTable实例的optional（失败返回空）
    std::optional<SSTable *> SSTable::Open(const Option *opt, RandomAccessFile *file, size_t file_size)
    {
        // SSTable文件至少包含Footer（固定大小）
        if (file_size < kFixedFooterSize)
        {
            LOG_ERROR("corrupted sstable file");
            return {};
        }

        // 读取Footer（文件末尾的固定大小区域）
        char footer_space[kFixedFooterSize];
        Slice footer_content;
        Status s = file->Read(file_size - kFixedFooterSize, kFixedFooterSize, &footer_content, footer_space);
        if (!s.ok())
        {
            return {};
        }

        // 解码Footer获取索引块和过滤器块的句柄
        Footer footer;
        s = footer.DecodeFrom(&footer_content);
        if (!s.ok())
        {
            return {};
        }

        // 读取索引块
        auto index_block_content = ReadBlock(file, footer.index_handle());
        if (!index_block_content.has_value())
        {
            return {};
        }
        Block *block = new Block(index_block_content.value().get());

        // 读取过滤器块
        auto filter_block_content = ReadBlock(file, footer.filter_handle());
        if (!filter_block_content.has_value())
        {
            delete block; // 释放已分配的索引块
            return {};
        }

        // 初始化SSTable内部数据结构
        Rep *rep = new SSTable::Rep;
        rep->index_block = block;
        rep->filter_data = filter_block_content.value()->data(); // 过滤器数据
        rep->status = Status();
        rep->filter_block_reader = new FilterBlockReader(Slice(rep->filter_data)); // 过滤器读取器
        rep->file = file;
        rep->option = opt;

        // 创建SSTable实例
        SSTable *table = new SSTable(rep);
        table->file_ = file;
        return {table};
    }

    // 从一个文件中读取某一个数据块
    // 参数file：随机访问文件对象
    // 参数block_handle：数据块句柄（包含偏移量和大小）
    // 返回值：包含数据块内容的智能指针（optional，失败为空）
    std::optional<std::unique_ptr<BlockContent>> ReadBlock(RandomAccessFile *file, const BlockHandle &block_handle)
    {
        std::unique_ptr<BlockContent> result(new BlockContent); // 存储数据块内容

        // 分配缓冲区存储数据块
        char *block_content = new char[block_handle.size()];
        Status status = file->Read(block_handle.offset(), block_handle.size(), result.get(), block_content);
        if (!status.ok())
        {
            delete[] block_content; // 释放缓冲区
            return {};
        }

        // 校验读取大小是否正确
        if (result->size() != block_handle.size())
        {
            delete[] block_content;
            LOG_ERROR("read block error");
            return {};
        }

        return {std::move(result)};
    }

    // 内部查询方法：根据键查找对应的值（先通过布隆过滤器过滤，再通过迭代器查找）
    // 参数key：要查询的键
    // 返回值：包含值的optional（不存在或失败为空）
    std::optional<std::string> SSTable::InternalGet(const koishidb::Slice &key)
    {
        Rep *r = rep_;

        //先通过布隆过滤器判断键是否可能存在（不存在则直接返回）//?
        // if (bloom_filter_.KeyMayMatch(key, r->filter_data) == false)
        // {
        //     return {};
        // }

        // 创建迭代器并查找键
        Iterator *iter = NewIterator();
        iter->SeekToFirst();
        bool flag = iter->Seek(key);

        //检查是否找到有效数据//?
        // if (flag == false)
        // {
        //     delete iter;
        //     return {};
        // }

        // 存储结果并释放迭代器
        std::string value = iter->Value().ToString();
        delete iter;
        return {value};
    }

    // 创建SSTable的迭代器（用于遍历数据块中的键值对）
    // 返回值：迭代器指针（需手动释放）
    Iterator *SSTable::NewIterator() const
    {
        return new SSTableIterator(rep_->index_block, rep_->file, rep_->option);
    }


    // 打印文件元信息（用于调试）
    // 参数meta：文件元信息
    void PrintFileMeta(FileMeta &meta)
    {
        LOG_INFO("file size %d", meta.file_size);
        LOG_INFO("file number %d", meta.number);

        // 提取最小键和最大键的用户键及序列号
        Slice smallest_key = meta.smallest_key.ToSlice(), largest_key = meta.largest_key.ToSlice();
        Slice user_key1, user_key2;
        SequenceNumber s1, s2;
        ExtractUserKey(smallest_key, &user_key1, &s1);
        ExtractUserKey(largest_key, &user_key2, &s2);

        LOG_INFO("smallest key's user_key %s, sequence number %lld",
                 std::string(user_key1.data(), user_key1.size()).data(), s1);
        LOG_INFO("largest key's user_key %s, sequence number %lld",
                 std::string(user_key2.data(), user_key2.size()).data(), s2);
    }

    // 将文件元信息编码并写入可写文件
    // 参数file_meta：文件元信息
    // 参数file：可写文件对象
    void EncodeFileMeta(FileMeta *file_meta, WritableFile &file)
    {
        std::string rep;
        // 编码文件大小、编号
        PutVarint64(&rep, file_meta->file_size);
        PutVarint64(&rep, file_meta->number);

        // 编码最小键（长度 + 内容）
        PutVarint64(&rep, file_meta->smallest_key.ToSlice().size());
        rep.append(file_meta->smallest_key.ToSlice().data(),
                   file_meta->smallest_key.ToSlice().size());

        // 编码最大键（长度 + 内容）
        PutVarint64(&rep, file_meta->largest_key.ToSlice().size());
        rep.append(file_meta->largest_key.ToSlice().data(),
                   file_meta->largest_key.ToSlice().size());

        // 写入文件并刷新
        file.Append(Slice(rep.data(), rep.size()));
        file.Flush();
    }

    // 从Slice中解码文件元信息（自动推进Slice指针）
    // 参数file_meta：存储解码结果的指针的指针
    // 参数slice：输入Slice（会被修改，指针推进）
    void DecodeFileMeta(FileMeta **file_meta, Slice *slice)
    {
        (*file_meta) = new FileMeta();

        // 解码文件大小、编号
        GetVarint64(slice, &(*file_meta)->file_size);
        GetVarint64(slice, &(*file_meta)->number);

        // 解码最小键
        uint64_t smallest_key_len;
        GetVarint64(slice, &smallest_key_len);
        (*file_meta)->smallest_key = TransToInternalKey(Slice(slice->data(), smallest_key_len));
        slice->Advance(smallest_key_len);

        // 解码最大键
        uint64_t largest_key_len;
        GetVarint64(slice, &largest_key_len);
        (*file_meta)->largest_key = TransToInternalKey(Slice(slice->data(), largest_key_len));
        slice->Advance(largest_key_len);
    }

}; // namespace koishidb


//定义了sstable的内存实例