// 该文件定义了RandomAccessFile类，提供线程安全的文件随机访问功能，支持从指定偏移量读取数据
#ifndef KOISHIDB_SRC_INCLUDE_DISK_RANDOM_ACCESS_FILE_H
#define KOISHIDB_SRC_INCLUDE_DISK_RANDOM_ACCESS_FILE_H

#include "status.h"
#include "slice.h"

namespace koishidb
{
    // 随机访问文件类，线程安全，通过文件名提供文件内容的随机读取功能
    class RandomAccessFile
    {
    public:
        // 构造函数：根据文件名打开文件
        // 参数filename: 要打开的文件名
        RandomAccessFile(std::string filename);

        // 获取文件操作状态
        // 返回值: 文件操作的状态（Status对象）
        Status status() const { return status_; }

        // 从文件的指定偏移量开始读取数据
        // 参数offset: 读取的起始偏移量
        // 参数n: 要读取的最大字节数
        // 参数result: 存储读取结果的Slice（可能指向scratch缓冲区）
        // 参数scratch: 临时缓冲区（用于存储读取的数据，需保持有效直到result使用完毕）
        // 返回值: 操作状态（成功返回OK，失败返回错误状态）
        // 线程安全：支持多线程并发调用
        Status Read(uint64_t offset, size_t n, Slice *result, char *scratch);

        // 析构函数：关闭文件
        ~RandomAccessFile();

    private:
        int fd_;                     // 文件描述符
        const std::string filename_; // 文件名
        Status status_;              // 文件操作状态
    };

}; // namespace koishidb

#endif

//定义了随机访问文件数据的类,“随机访问” 的本质是可以通过 “偏移量” 直接定位到文件的任意位置进行读写,read是顺序访问,pread是随机访问