// 该文件实现了SSTableIterator类，用于遍历SSTable中的数据块，支持定位、移动等迭代操作
#include "sstable_iterator.h"

#include "option.h"
#include "block.h"
#include "block_iterator.h"
#include "format.h"
#include "sstable.h"

namespace koishidb
{

    // 移动迭代器到第一个数据项
    void SSTableIterator::SeekToFirst()
    {
        index_iter_->SeekToFirst(); // 索引迭代器定位到开头
        index_iter_->Next();        // 移动到第一个索引项
        UpdateDataIterator();       // 更新数据迭代器（指向第一个数据块）
    }

    // 移动迭代器到下一个数据项
    void SSTableIterator::Next()
    {
        assert(data_iter_ != nullptr); // 确保数据迭代器有效

        while (true)
        {
            data_iter_->Next();      // 移动到当前数据块的下一项
            if (data_iter_->Valid()) // 若当前数据块还有有效项，退出循环
            {
                break;
            }
            // 当前数据块已遍历完，移动到下一个数据块
            index_iter_->Next();
            if (index_iter_->Valid()) // 若还有下一个数据块，更新数据迭代器
            {
                UpdateDataIterator();
            }
            else // 所有数据块已遍历完，退出循环
            {
                break;
            }
        }
    }

    // 移动迭代器到上一个数据项（当前未实现）
    void SSTableIterator::Prev()
    {
        // 暂未实现
    }

    // 移动迭代器到最后一个数据项
    void SSTableIterator::SeekToLast()
    {
        index_iter_->SeekToLast(); // 索引迭代器定位到最后
    }

    // 获取当前数据项的值
    Slice SSTableIterator::Value() const
    {
        return data_iter_->Value();
    }

    // 获取当前数据项的键
    Slice SSTableIterator::Key() const
    {
        return data_iter_->Key();
    }

    // 判断迭代器是否指向有效数据项
    bool SSTableIterator::Valid() const
    {
        return data_iter_->Valid();
    }

    // 定位迭代器到第一个大于等于目标键的数据项
    // 参数target：目标键
    // 返回值：定位成功返回true，否则返回false
    bool SSTableIterator::Seek(const Slice &target)
    {
        index_iter_->Seek(target);                 // 索引迭代器定位到目标键所在的索引项
        Slice block_handle = index_iter_->Value(); // 获取目标数据块的句柄

        // 若目标数据块与当前数据块不同，更新数据迭代器
        if (std::string(block_handle.data(), block_handle.size()) != data_block_handle_)
        {
            UpdateDataIterator();
        }

        assert(data_iter_ != nullptr);
        return data_iter_->Seek(target); // 在数据块内定位目标键
    }

    // 读取指定数据块并创建对应的迭代器
    // 参数random_access_file：随机访问文件对象
    // 参数index_value：数据块句柄（编码后的字符串）
    // 返回值：数据块的迭代器（失败返回nullptr）
    Iterator *SSTableIterator::BlockReader(RandomAccessFile *random_access_file,
                                           const Slice &index_value)
    {
        BlockHandle block_handle;
        Slice input = index_value;

        // 解码数据块句柄
        auto s = block_handle.DecodeFrom(&input);
        if (!s.ok())
        {
            return nullptr;
        }

        // 读取数据块内容
        auto block_content = ReadBlock(random_access_file, block_handle);
        if (!block_content.has_value())
        {
            return nullptr;
        }

        // 创建数据块并返回其迭代器
        Block *block = new Block(std::move(block_content.value()->data),
                     block_content.value()->size);
        return new BlockIterator(block, opt_, true);
    }

    // 更新数据迭代器（根据当前索引项指向的数据块）
    void SSTableIterator::UpdateDataIterator()
    {
        if (!index_iter_->Valid()) // 若索引迭代器无效，数据迭代器置空
        {
            data_iter_ = nullptr;
            return;
        }

        Slice block_handle = index_iter_->Value(); // 获取当前索引项对应的数据块句柄

        // 释放旧的数据迭代器
        if (data_iter_ != nullptr)
        {
            delete data_iter_;
        }

        // 创建新的数据迭代器（指向当前数据块）
        data_iter_ = BlockReader(random_access_file_, block_handle);
        data_block_handle_ = std::string(block_handle.data(), block_handle.size()); // 记录当前数据块句柄

        assert(data_iter_ != nullptr);
        data_iter_->SeekToFirst(); // 数据迭代器定位到数据块开头
    }

    // 析构函数：释放迭代器资源
    SSTableIterator::~SSTableIterator()
    {
        if (data_iter_ != nullptr)
        {
            delete data_iter_; // 释放数据迭代器
        }
        delete index_iter_;
    }

    // 构造函数：初始化SSTable迭代器
    // 参数indexBlock：索引块（用于遍历数据块）
    // 参数random_access_file：SSTable的随机访问文件
    // 参数opt：数据库选项（包含比较器等）
    SSTableIterator::SSTableIterator(Block *indexBlock,RandomAccessFile *random_access_file,const Option *opt): opt_(opt)
    {
        index_iter_ = new BlockIterator(indexBlock, opt_); // 创建索引迭代器
        random_access_file_ = random_access_file;          // 关联文件对象
        data_iter_ = nullptr;                              // 初始化数据迭代器
    }

}; // namespace koishidb

//使用两层迭代器结构：index_iter_ 用于遍历索引块，data_iter_ 用于遍历具体的数据块,索引块存储了指向各个数据块的指针