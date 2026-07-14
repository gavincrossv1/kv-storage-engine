#include "block_cache.h"

#include "block.h"
#include "format.h"
#include "random_access_file.h"
#include "sstable.h"

namespace koishidb
{
    BlockCache::BlockCache(size_t capacity_bytes)
        : capacity_bytes_(capacity_bytes), usage_bytes_(0) {}

    size_t BlockCache::KeyHash::operator()(const Key &key) const
    {
        const size_t h1 = std::hash<uint64_t>{}(key.file_number);
        const size_t h2 = std::hash<uint64_t>{}(key.block_offset);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }

    std::shared_ptr<Block> BlockCache::Get(uint64_t file_number,
                                           RandomAccessFile *file,
                                           const BlockHandle &handle)
    {
        const Key key{file_number, handle.offset()};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto cached = cache_.find(key);
            if (cached != cache_.end())
            {
                lru_.splice(lru_.begin(), lru_, cached->second);
                return cached->second->block;
            }
        }

        // 磁盘读取不占用缓存锁，避免一个慢 IO 阻塞其他缓存命中。
        auto content = ReadBlock(file, handle);
        if (!content.has_value())
        {
            return {};
        }
        std::shared_ptr<Block> loaded(new Block(
            std::move(content.value()->data), content.value()->size));

        if (capacity_bytes_ == 0 || loaded->size > capacity_bytes_)
        {
            return loaded;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto cached = cache_.find(key);
        if (cached != cache_.end())
        {
            lru_.splice(lru_.begin(), lru_, cached->second);
            return cached->second->block;
        }

        lru_.push_front(Entry{key, loaded, loaded->size});
        cache_[key] = lru_.begin();
        usage_bytes_ += loaded->size;

        while (usage_bytes_ > capacity_bytes_ && !lru_.empty())
        {
            auto oldest = --lru_.end();
            usage_bytes_ -= oldest->charge;
            cache_.erase(oldest->key);
            lru_.erase(oldest);
        }
        return loaded;
    }

    void BlockCache::EvictFile(uint64_t file_number)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto iter = lru_.begin(); iter != lru_.end();)
        {
            if (iter->key.file_number != file_number)
            {
                ++iter;
                continue;
            }
            usage_bytes_ -= iter->charge;
            cache_.erase(iter->key);
            iter = lru_.erase(iter);
        }
    }
} // namespace koishidb
