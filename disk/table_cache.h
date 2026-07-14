#ifndef KOISHIDB_SRC_INCLUDE_DISK_TABLE_CACHE_H
#define KOISHIDB_SRC_INCLUDE_DISK_TABLE_CACHE_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace koishidb
{
    class BlockCache;
    class Option;
    class RandomAccessFile;
    class SSTable;

    // 缓存已打开的 SSTable 及其索引和过滤器，避免每次 Get 重复读取表元数据。
    class TableCache
    {
    public:
        explicit TableCache(size_t capacity);
        ~TableCache();

        TableCache(const TableCache &) = delete;
        TableCache &operator=(const TableCache &) = delete;

        std::shared_ptr<SSTable> FindTable(uint64_t file_number,
                                           const std::string &file_name,
                                           size_t file_size);
        void Evict(uint64_t file_number);

    private:
        struct Entry;
        using CacheList = std::list<std::shared_ptr<Entry>>;

        const size_t capacity_;
        std::unique_ptr<BlockCache> block_cache_;
        CacheList lru_;
        std::unordered_map<uint64_t, CacheList::iterator> cache_;
        std::mutex mutex_;
    };
} // namespace koishidb

#endif