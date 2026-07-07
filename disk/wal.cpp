// 教学版 WAL 实现：record = fixed32(length) + fixed32(checksum) + payload
#include "wal.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "encode.h"
#include "writable_file.h"

namespace koishidb
{
    namespace
    {
        constexpr size_t kWalHeaderSize = 8;
        constexpr uint32_t kWalChecksumSeed = 2166136261u;
        constexpr size_t kMaxWalRecordSize = 64 * 1024 * 1024;

        uint32_t WalChecksum(const char *data, size_t n)
        {
            uint32_t h = kWalChecksumSeed;
            for (size_t i = 0; i < n; ++i)
            {
                h ^= static_cast<unsigned char>(data[i]);
                h *= 16777619u;
            }
            return h;
        }

        Status PosixError(const std::string &context, int error_number)
        {
            if (error_number == ENOENT)
            {
                return Status::NotFound(context, std::strerror(error_number));
            }
            return Status::IOError(context, std::strerror(error_number));
        }
    } // namespace

    WalWriter::WalWriter(const std::string &filename) : filename_(filename)
    {
        int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0)
        {
            status_ = PosixError(filename, errno);
            return;
        }
        file_.reset(new WritableFile(fd, filename));
    }

    WalWriter::~WalWriter() = default;

    Status WalWriter::AddRecord(const Slice &record)
    {
        if (!status_.ok())
        {
            return status_;
        }
        if (record.size() > kMaxWalRecordSize)
        {
            return Status::InvalidArgument("wal record too large");
        }

        std::string physical_record;
        PutFixed32(&physical_record, static_cast<uint32_t>(record.size()));
        PutFixed32(&physical_record, WalChecksum(record.data(), record.size()));
        physical_record.append(record.data(), record.size());

        Status s = file_->Append(Slice(physical_record.data(), physical_record.size()));
        if (!s.ok())
        {
            return s;
        }
        return file_->Sync();
    }

    WalReader::WalReader(const std::string &filename)
        : filename_(filename), fd_(-1), offset_(0), last_valid_offset_(0)
    {
        fd_ = ::open(filename.c_str(), O_RDONLY);
        if (fd_ < 0)
        {
            if (errno == ENOENT)
            {
                status_ = Status::OK();
            }
            else
            {
                status_ = PosixError(filename, errno);
            }
        }
    }

    WalReader::~WalReader()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
        }
    }

    bool WalReader::ReadBytes(size_t n, std::string *dst, Status *status)
    {
        dst->clear();
        dst->resize(n);
        size_t read_size = 0;
        while (read_size < n)
        {
            ssize_t r = ::read(fd_, &(*dst)[read_size], n - read_size);
            if (r > 0)
            {
                read_size += static_cast<size_t>(r);
                offset_ += static_cast<uint64_t>(r);
                continue;
            }
            if (r == 0)
            {
                dst->resize(read_size);
                *status = Status::OK();
                return false;
            }
            if (errno == EINTR)
            {
                continue;
            }
            *status = PosixError(filename_, errno);
            return false;
        }
        *status = Status::OK();
        return true;
    }

    bool WalReader::ReadRecord(std::string *record, Status *status)
    {
        record->clear();
        *status = Status::OK();
        if (!status_.ok())
        {
            *status = status_;
            return false;
        }
        if (fd_ < 0)
        {
            return false;
        }

        const uint64_t record_start = offset_;
        std::string header;
        if (!ReadBytes(kWalHeaderSize, &header, status))
        {
            if (!status->ok())
            {
                return false;
            }
            if (!header.empty())
            {
                *status = Status::Corruption("truncated wal header");
            }
            return false;
        }

        uint32_t length = DecodeFixed32(header.data());
        uint32_t expected_checksum = DecodeFixed32(header.data() + 4);
        if (length > kMaxWalRecordSize)
        {
            *status = Status::Corruption("wal record too large");
            return false;
        }

        std::string payload;
        if (!ReadBytes(length, &payload, status))
        {
            if (status->ok())
            {
                *status = Status::Corruption("truncated wal record");
            }
            return false;
        }

        uint32_t actual_checksum = WalChecksum(payload.data(), payload.size());
        if (actual_checksum != expected_checksum)
        {
            *status = Status::Corruption("wal checksum mismatch");
            return false;
        }

        last_valid_offset_ = record_start + kWalHeaderSize + length;
        *record = std::move(payload);
        return true;
    }

    Status TruncateWal(const std::string &filename, uint64_t size)
    {
        int fd = ::open(filename.c_str(), O_WRONLY);
        if (fd < 0)
        {
            if (errno == ENOENT)
            {
                return Status::OK();
            }
            return PosixError(filename, errno);
        }

        while (::ftruncate(fd, static_cast<off_t>(size)) != 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            Status s = PosixError(filename, errno);
            ::close(fd);
            return s;
        }
        ::close(fd);
        return Status::OK();
    }

    Status RemoveWal(const std::string &filename)
    {
        if (::unlink(filename.c_str()) == 0 || errno == ENOENT)
        {
            return Status::OK();
        }
        return PosixError(filename, errno);
    }
}; // namespace koishidb
