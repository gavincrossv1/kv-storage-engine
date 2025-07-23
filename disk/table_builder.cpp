// 该文件实现了TableBuilder类，负责构建SSTable（排序字符串表），包括添加键值对、刷数据块、生成索引和过滤器等功能
#include "table_builder.h"

#include "common.h"
#include "option.h"
#include "block_builder.h"
#include "filterblock_builder.h"
#include "format.h"
#include "writable_file.h"
#include "comparator.h"

namespace koishidb
{

    // TableBuilder的内部数据结构，存储构建SSTable所需的临时数据和状态
    struct TableBuilder::Rep
    {
        // 构造函数：初始化内部状态
        // 参数opt：数据库选项（包含比较器等）
        // 参数file：可写文件对象
        Rep(const Option *opt, WritableFile *file)
            : opt(opt),
              file(file),
              data_block(opt),
              index_block(opt),
              filter_block(),
              offset(static_cast<uint64_t>(0)),
              num_entries(static_cast<uint64_t>(0)),
              closed(false),
              block_handle(),
              pending_index_block(false),
              last_key("") {}

        const Option *opt;               // 数据库选项（包含比较器）
        WritableFile *file;              // 可写文件（用于写入SSTable内容）
        BlockBuilder data_block;         // 数据块构建器
        BlockBuilder index_block;        // 索引块构建器
        FilterBlockBuilder filter_block; // 过滤器器块构建器（如布隆过滤器）
        uint64_t offset;                 // 当前写入偏移量
        uint64_t num_entries;            // 已添加的键值对数量
        bool closed;                     // 标记是否已关闭（调用Finish或Abandon后为true）
        BlockHandle block_handle;        // 数据块句柄（用于写入索引块）
        bool pending_index_block;        // 标记是否需要添加索引条目录
        std::string last_key;            // 上一个添加的键（用于确保键有序）
        Status status;                   // 当前状态
    };

    // 构造函数：初始化TableBuilder
    // 参数opt：数据库选项
    // 参数writableFile：可写文件
    TableBuilder::TableBuilder(const Option *opt, WritableFile *writableFile): rep_(new Rep(opt, writableFile)) 
    {

    }

    // 析构函数：释放内部数据结构
    TableBuilder::~TableBuilder() { delete rep_; }

    // 向SSTable添加键值对
    // 参数key：键
    // 参数value：值
    void TableBuilder::Add(const Slice &key, const Slice &value)
    {
        Rep *r = rep_;
        assert(!r->closed); // 确保未关闭

        // 确保键是有序的（当前键必须大于上一个键）
        assert(r->last_key.empty() || r->opt->cmp->Compare(key, r->last_key.c_str()) == 1);

        // 若存在待处理的索引块，添加索引
        if (r->pending_index_block)
        {
            assert(r->data_block.Empty()); // 确保数据块已清空

            // 向索引块添加条目（上一个键，数据块句柄）
            std::string block_handle;
            r->block_handle.EncodeTo(&block_handle);
            r->index_block.Add(Slice(r->last_key), Slice(block_handle));
            r->pending_index_block = false;
        }

        // 将键添加到过滤器（用于快速查询）
        r->filter_block.AddKey(key);

        // 将键值对添加到数据块
        r->data_block.Add(key, value);
        r->last_key.assign(key.data(), key.size()); // 更新上一个键
        r->num_entries++;                           // 增加计数

        // 若数据块大小达到阈值，刷新到文件
        if (r->data_block.EstimatedSize() >= kBlockSize)
        {
            Flush();
        }
    }

    // 将当前数据块刷新到文件
    void TableBuilder::Flush()
    {
        Rep *r = rep_;
        assert(!r->closed); // 确保未关闭

        if (r->data_block.Empty())
        {
            return; // 数据块为空，无需刷新
        }

        // 写入数据块并记录其句柄
        WriteBlock(&r->data_block, &r->block_handle);

        // 若操作成功，标记需要添加索引，并刷新文件缓冲区
        if (ok())
        {
            r->pending_index_block = true;
            r->status = r->file->Flush();//核心操作
            r->filter_block.StartBlock(r->offset); // 通知过滤器块当前数据块的偏移量
        }
    }

    // 将块内容写入缓冲区，并更新块句柄
    // 参数block_builder：块构建器
    // 参数block_handle：块句柄（用于记录块的偏移量和大小）
    void TableBuilder::WriteBlock(BlockBuilder *block_builder, BlockHandle *block_handle)
    {
        Rep *r = rep_;
        Slice block_content = block_builder->Finish(); // 完成块构建，获取内容

        // 将块内容写入文件缓冲区
        r->status = r->file->Append(block_content);
        if (r->status.ok())
        {
            // 更新块句柄（偏移量和大小）
            block_handle->set_offset(r->offset);
            block_handle->set_size(block_content.size());
            r->offset += block_content.size(); // 更新当前偏移量
        }

        block_builder->Reset(); // 重置块构建器，准备下一个块
    }

    // 完成SSTable构建，写入所有剩余数据并生成文件尾
    // 返回值：操作状态（成功或错误）
    Status TableBuilder::Finish()
    {
        Rep *r = rep_;
        assert(!r->closed); // 确保未关闭

        Flush();          // 刷新剩余的数据块
        r->closed = true; // 标记为已关闭

        if (!ok())
        {
            return status(); // 若状态不佳，直接返回错误
        }

        // 1. 写入过滤器块、2. 写入索引块、3. 写入文件尾
        BlockHandle index_block_handle, filter_block_handle;

        // 写入过滤器块
        Slice filter_block_contents = r->filter_block.Finish();
        r->status = r->file->Append(filter_block_contents);
        if (!r->status.ok())
        {
            return r->status;
        }
        filter_block_handle.set_offset(r->offset);
        filter_block_handle.set_size(filter_block_contents.size());
        r->offset += filter_block_contents.size();

        // 处理剩余的索引条目
        if (r->pending_index_block)
        {
            std::string block_handle;
            r->block_handle.EncodeTo(&block_handle);
            r->index_block.Add(Slice(r->last_key), Slice(block_handle));
            r->pending_index_block = false;
        }

        // 写入索引块
        WriteBlock(&r->index_block, &index_block_handle);
        if (!ok())
        {
            return status();
        }

        // 写入文件尾（包含索引块和过滤器块的句柄）
        Footer footer;
        footer.set_index(index_block_handle);
        footer.set_filter(filter_block_handle);

        std::string footer_encoded;
        footer.EncodeTo(&footer_encoded);
        r->status = r->file->Append(footer_encoded.data());
        if (!ok())
        {
            return status();
        }
        r->offset += footer_encoded.size(); // 更新偏移量（应等于kFixedFooterSize）

        r->file->Flush(); // 确保所有数据写入磁盘
        return status();
    }

    // 放弃SSTable构建（标记为关闭，不写入剩余数据）
    void TableBuilder::Abandon()
    {
        assert(!rep_->closed);
        rep_->closed = true;
    }

    // 获取已添加的键值对数量
    uint64_t TableBuilder::NumEntries() const { return rep_->num_entries; }

    // 获取当前生成的文件大小
    uint64_t TableBuilder::FileSize() const { return rep_->offset; }

    // 获取当前状态
    Status TableBuilder::status() const { return rep_->status; }
}; // namespace koishidb

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