// 该文件实现了RandomAccessFile类，提供对文件的随机读取功能，支持指定偏移量读取数据
#include "random_access_file.h"

#include <fcntl.h>
#include <unistd.h>

namespace koishidb
{
    // 构造函数：打开指定文件用于读取
    // 参数filename：要打开的文件名
    RandomAccessFile::RandomAccessFile(std::string filename) : filename_(filename)
    {
        // 以只读方式打开文件，获取文件描述符
        fd_ = ::open(filename.c_str(), O_RDONLY);

        // 若打开失败，记录错误状态
        if (fd_ == -1)
        {
            status_ = Status::Corruption("Open file error, check the filename");
        }
    }

    // 从指定偏移量读取数据
    // 参数offset：读取的起始偏移量（相对于文件开头）
    // 参数n：要读取的字节数
    // 参数result：输出参数，存储读取到的数据（通过Slice返回）
    // 参数scratch：用于临时存储读取数据的缓冲区
    // 返回值：操作状态（成功或失败信息）
    Status RandomAccessFile::Read(uint64_t offset, size_t n, Slice *result,char *scratch)
    {
        // 确保文件已成功打开（文件描述符有效）
        if (fd_ == -1)
        {
            return status_;
        }

        // 使用pread进行随机读取（不会改变文件指针位置）
        ssize_t read_size = ::pread(fd_, scratch, n, static_cast<off_t>(offset));

        // 将读取到的数据通过Slice返回（若读取失败则返回空Slice）
        *result = Slice(scratch, (read_size < 0) ? 0 : read_size);

        // 若读取失败，记录错误状态
        if (read_size < 0)
        {
            status_.Corruption("read file error");
        }

        return status_;
    }

    // 析构函数：关闭文件描述符
    RandomAccessFile::~RandomAccessFile()
    {
        if (fd_ != -1)
        {
            ::close(fd_);
        }
    }
}; // namespace koishidb

//定义了随机访问文件数据的类,“随机访问” 的本质是可以通过 “偏移量” 直接定位到文件的任意位置进行读写,read是顺序访问,pread是随机访问