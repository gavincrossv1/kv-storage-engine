// 该文件声明了WritableFile类，提供带缓冲的文件写入接口，支持POSIX系统，可高效写入磁盘并管理文件描述符
#ifndef KOISHIDB_SRC_INCLUDE_DISK_WRITABLE_FILE_H
#define KOISHIDB_SRC_INCLUDE_DISK_WRITABLE_FILE_H

#include <unistd.h>

#include <string>

#include "common.h"
#include "status.h"

namespace koishidb
{

    // 可写文件类：仅支持POSIX系统调用，提供带缓冲的文件写入功能
    // 不可拷贝但可移动，通过文件描述符操作文件
    class WritableFile
    {
    public:
        // 构造函数：初始化可写文件
        // 参数fd：文件描述符
        // 参数filename：文件名
        WritableFile(int fd, std::string filename): pos_(0), filename_(filename), dir_(Dirname(filename)), fd_(fd) 
        {

        }

        // 析构函数：关闭文件描述符
        ~WritableFile()
        {
            if (fd_ > 0)
            {
                ::close(fd_); // 关闭文件
            }
        }

        // 向文件追加内容（使用缓冲区）
        // 参数content：要追加的内容
        // 返回值：操作状态（成功返回OK，失败返回错误状态）
        Status Append(const Slice &content);

        // 将缓冲区内容刷新到磁盘
        // 返回值：操作状态（成功返回OK，失败返回错误状态）
        Status Flush();

    private:
        // 无缓冲写入（直接调用系统write）
        // 参数data：要写入的数据
        // 参数size：数据大小
        // 返回值：操作状态
        Status WriteUnbuffered(const char *data, size_t size);

        // 从文件名中提取目录部分（静态方法）
        // 参数filename：完整文件名
        // 返回值：文件所在目录
        static std::string Dirname(const std::string &filename);

        // 将POSIX错误码转换为Status（静态方法）
        // 参数context：错误上下文
        // 参数error_number：错误码
        // 返回值：对应的Status对象
        static Status PosixError(const std::string &context, int error_number);

        std::string dir_;               // 文件所在目录
        std::string filename_;          // 文件名
        int fd_;                        // 文件描述符
        size_t pos_;                    // 当前缓冲区位置
        char buf_[kWritableFileBuffer]; // 写入缓冲区（减少磁盘IO）
    };
}; // namespace koishidb

#endif

//提供向文件中写入数据的方式