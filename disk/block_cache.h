#ifndef KOISHIDB_SRC_INCLUDE_DISK_BLOCK_CACHE_H
#define KOISHIDB_SRC_INCLUDE_DISK_BLOCK_CACHE_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace koishidb
{
    struct Block;
    class BlockHandle;
    class RandomAccessFile;

    // 以 SSTable 文件编号和块偏移为键，按总字节数限制缓存容量。
    class BlockCache
    {
    public:
        explicit BlockCache(size_t capacity_bytes);

        BlockCache(const BlockCache &) = delete;
        BlockCache &operator=(const BlockCache &) = delete;

        std::shared_ptr<Block> Get(uint64_t file_number,
                                   RandomAccessFile *file,
                                   const BlockHandle &handle);
        void EvictFile(uint64_t file_number);

    private:
        struct Key
        {
            uint64_t file_number;
            uint64_t block_offset;

            bool operator==(const Key &other) const
            {
                return file_number == other.file_number &&
                       block_offset == other.block_offset;
            }
        };

        struct KeyHash
        {
            size_t operator()(const Key &key) const;
        };

        struct Entry
        {
            Key key;
            std::shared_ptr<Block> block;
            size_t charge;
        };

        using CacheList = std::list<Entry>;

        const size_t capacity_bytes_;
        size_t usage_bytes_;
        CacheList lru_;
        std::unordered_map<Key, CacheList::iterator, KeyHash> cache_;
        std::mutex mutex_;
    };
} // namespace koishidb

#endif
