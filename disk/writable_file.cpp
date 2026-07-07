// 该文件实现了WritableFile类，提供带缓冲的文件写入功能，支持POSIX系统调用，确保高效写入磁盘
#include "writable_file.h"

#include <sys/errno.h>

#include <cstring>

#include "status.h"

namespace koishidb
{
    // 向文件追加内容（使用缓冲区减少磁盘IO）
    // 参数content：要追加的内容
    // 返回值：操作状态（成功返回OK，失败返回错误状态）
    Status WritableFile::Append(const Slice &content)
    {
        size_t write_size = content.size();
        const char *write_data = content.data();

        // 先将内容尽可能放入缓冲区
        size_t copy_size = std::min(write_size, kWritableFileBuffer - pos_);
        std::memcpy(buf_ + pos_, write_data, copy_size);
        write_data += copy_size;
        write_size -= copy_size;
        pos_ += copy_size;

        // 若内容已全部放入缓冲区，直接返回成功
        if (write_size == 0)
        {
            return Status::OK();
        }

        // 缓冲区已满，先刷新缓冲区到磁盘
        Status status = Flush();
        if (!status.ok())
        {
            return status;
        }

        // 小数据写入缓冲区，大数据直接写入磁盘
        if (write_size < kWritableFileBuffer)
        {
            std::memcpy(buf_, write_data, write_size);
            pos_ = write_size;
            return Status::OK();
        }

        // 直接写入磁盘（无缓冲）
        return WriteUnbuffered(write_data, write_size);
    }

    // 将缓冲区内容刷新到磁盘
    // 返回值：操作状态（成功返回OK，失败返回错误状态）
    Status WritableFile::Flush()
    {
        Status status = WriteUnbuffered(buf_, pos_); // 写入缓冲区内容
        pos_ = 0;                                    // 重置缓冲区位置
        return status;
    }

    // 将缓冲区内容同步到磁盘
    // 返回值：操作状态（成功返回OK，失败返回错误状态）
    Status WritableFile::Sync()
    {
        Status status = Flush();
        if (!status.ok())
        {
            return status;
        }

        while (::fsync(fd_) != 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return PosixError(filename_, errno);
        }
        return Status::OK();
    }

    // 无缓冲将内存数据写入磁盘（直接调用系统write函数）
    // 参数data：要写入的数据
    // 参数size：数据大小
    // 返回值：操作状态（成功返回OK，失败返回错误状态）
    Status WritableFile::WriteUnbuffered(const char *data, size_t size)
    {
        while (size > 0)
        {
            ssize_t write_result = ::write(fd_, data, size); // 调用系统write
            if (write_result < 0)
            {
                if (errno == EINTR)
                {
                    continue; // 被信号中断，重试
                }
                // 写入失败，返回错误状态
                return PosixError(filename_, errno);
            }
            data += write_result; // 移动数据指针
            size -= write_result; // 减少剩余大小
        }
        return Status::OK();
    }

    // 静态方法：将POSIX错误码转换为Status
    // 参数context：错误上下文（文件名）
    // 参数error_number：错误码
    // 返回值：对应的Status对象（NotFound或IOError）
    Status WritableFile::PosixError(const std::string &context, int error_number)
    {
        if (error_number == ENOENT)
        {
            return Status::NotFound(context, std::strerror(error_number));
        }
        else
        {
            return Status::IOError(context, std::strerror(error_number));
        }
    }

    // 静态方法：从文件名中提取目录部分
    // 参数filename：完整文件名
    // 返回值：文件所在目录（无目录时返回"."）
    std::string WritableFile::Dirname(const std::string &filename)
    {
        auto pos = filename.rfind('/'); // 查找最后一个'/'
        if (pos == std::string::npos)
        {
            return std::string("."); // 无目录时返回当前目录
        }
        return filename.substr(0, pos); // 返回目录部分
    }
}; // namespace koishidb

//提供向文件�