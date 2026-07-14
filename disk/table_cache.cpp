#include "table_cache.h"

#include "block_cache.h"
#include "key.h"
#include "option.h"
#include "random_access_file.h"
#include "sstable.h"

namespace koishidb
{
    struct TableCache::Entry
    {
        uint64_t file_number;
        std::unique_ptr<RandomAccessFile> file;
        std::unique_ptr<Option> option;
        std::unique_ptr<SSTable> table;
    };

    TableCache::TableCache(size_t capacity)
        : capacity_(capacity), block_cache_(new BlockCache(64 * 1024 * 1024)) {}

    TableCache::~TableCache() = default;

    std::shared_ptr<SSTable> TableCache::FindTable(uint64_t file_number,
                                                   const std::string &file_name,
                                                   size_t file_size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cached = cache_.find(file_number);
        if (cached != cache_.end())
        {
            lru_.splice(lru_.begin(), lru_, cached->second);
            std::shared_ptr<Entry> entry = *cached->second;
            return std::shared_ptr<SSTable>(entry, entry->table.get());
        }

        std::unique_ptr<RandomAccessFile> file(new RandomAccessFile(file_name));
        if (!file->status().ok())
        {
            return {};
        }

        std::unique_ptr<Option> option(new Option(new InternalKeyComparator()));
        auto table_result = SSTable::Open(option.get(), file.get(), file_size,
                          file_number, block_cache_.get());
        if (!table_result.has_value())
        {
            return {};
        }

        std::shared_ptr<Entry> entry(new Entry);
        entry->file_number = file_number;
        entry->file = std::move(file);
        entry->option = std::move(option);
        entry->table.reset(*table_result);
        lru_.push_front(entry);
        cache_[file_number] = lru_.begin();

        if (capacity_ == 0)
        {
            cache_.erase(file_number);
            lru_.pop_front();
            return std::shared_ptr<SSTable>(entry, entry->table.get());
        }

        while (cache_.size() > capacity_)
        {
            auto oldest = --lru_.end();
            cache_.erase((*oldest)->file_number);
            lru_.erase(oldest);
        }
        return std::shared_ptr<SSTable>(entry, entry->table.get());
    }

    void TableCache::Evict(uint64_t file_number)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cached = cache_.find(file_number);
        if (cached != cache_.end())
        {
            lru_.erase(cached->second);
            cache_.erase(cached);
        }
        block_cache_->EvictFile(file_number);
    }
} // namespace koishidb