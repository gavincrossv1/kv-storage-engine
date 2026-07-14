// 该文件声明了SSTableIterator类，用于遍历SSTable中的数据项，支持定位、移动等迭代操作
#ifndef KOISHIDB_SRC_INCLUDE_SSTABLE_ITERATOR_H
#define KOISHIDB_SRC_INCLUDE_SSTABLE_ITERATOR_H

#include <cstdint>

#include "iterator.h"

namespace koishidb
{
    class RandomAccessFile;
    class Block;
    class Option;
    class BlockCache;

    // SSTable迭代器类，用于遍历SSTable中的数据项
    class SSTableIterator : public Iterator
    {
    public:
        // 构造函数：初始化SSTable迭代器
        // 参数indexBlock：索引块（用于遍历数据块）
        // 参数random_access_file：SSTable的随机访问文件
        // 参数opt：数据库选项（包含比较器等）
        SSTableIterator(Block *indexBlock, RandomAccessFile *random_access_file,
                        const Option *opt, uint64_t file_number = 0,
                        BlockCache *block_cache = nullptr);

        // 禁用拷贝构造函数
        SSTableIterator(const SSTableIterator &that) = delete;

        // 禁用赋值运算符
        SSTableIterator &operator=(const SSTableIterator &that) = delete;

        // 析构函数：释放资源
        ~SSTableIterator();

        // 判断迭代器是否指向有效数据项
        bool Valid() const override;

        // 移动迭代器到上一个数据项
        void Prev() override;

        // 移动迭代器到下一个数据项
        void Next() override;

        // 移动迭代器到第一个数据项
        void SeekToFirst() override;

        // 移动迭代器到最后一个数据项
        void SeekToLast() override;

        // 获取当前数据项的键
        Slice Key() const override;

        // 获取当前数据项的值
        Slice Value() const override;

        // 定位迭代器到第一个大于等于目标键的数据项
        // 参数target：目标键
        // 返回值：定位成功返回true，否则返回false
        bool Seek(const Slice &target) override;

    private:
        // 更新数据迭代器（根据当前索引项指向的数据块）
        void UpdateDataIterator();

        // 读取指定数据块并创建对应的迭代器
        // 参数random_access_file：随机访问文件对象
        // 参数index_value：数据块句柄（编码后的字符串）
        // 返回值：数据块的迭代器（失败返回nullptr）
        Iterator *BlockReader(RandomAccessFile *random_access_file,
                              const Slice &index_value);

        Iterator *index_iter_;                 // 索引块的迭代器
        Iterator *data_iter_;                  // 数据块的迭代器
        const Option *opt_;                    // 数据库选项（包含比较器）
        RandomAccessFile *random_access_file_; // SSTable的文件对象
        uint64_t file_number_;                // SSTable 文件编号
        BlockCache *block_cache_;             // 共享数据块缓存
        std::string data_block_handle_;        // 当前数据块的句柄（用于判断是否需要更新迭代器）
    };

}; // namespace koishidb

#endif

//使用两层迭代器结构：index_iter_ 用于遍历索引块，data_iter_ 用于遍历具体的数据块,索引块存储了指向各个数据块的指针