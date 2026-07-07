// 教学版 WAL：按 record 追加 WriteBatch 原始内容，支持启动时顺序重放
#ifndef KOISHIDB_SRC_INCLUDE_DISK_WAL_H
#define KOISHIDB_SRC_INCLUDE_DISK_WAL_H

#include <cstdint>
#include <memory>
#include <string>

#include "slice.h"
#include "status.h"

namespace koishidb
{
    class WritableFile;

    class WalWriter
    {
    public:
        explicit WalWriter(const std::string &filename);
        ~WalWriter();

        WalWriter(const WalWriter &) = delete;
        WalWriter &operator=(const WalWriter &) = delete;

        Status status() const { return status_; }
        Status AddRecord(const Slice &record);

    private:
        std::string filename_;
        std::unique_ptr<WritableFile> file_;
        Status status_;
    };

    class WalReader
    {
    public:
        explicit WalReader(const std::string &filename);
        ~WalReader();

        WalReader(const WalReader &) = delete;
        WalReader &operator=(const WalReader &) = delete;

        Status status() const { return status_; }
        bool ReadRecord(std::string *record, Status *status);
        uint64_t LastValidOffset() const { return last_valid_offset_; }

    private:
        bool ReadBytes(size_t n, std::string *dst, Status *status);

        std::string filename_;
        int fd_;
        uint64_t offset_;
        uint64_t last_valid_offset_;
        Status status_;
    };

    Status TruncateWal(const std::string &filename, uint64_t size);
    Status RemoveWal(const std::string &filename);
}; // namespace koishidb

#endif
