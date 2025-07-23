// 该文件实现了BuildTable函数，用于将内存表（memtable）中的数据构建为SSTable文件并写入磁盘
#include "builder.h"

#include <fcntl.h>

#include "option.h"
#include "table_builder.h"
#include "writable_file.h"
#include "logger.h"

namespace koishidb
{

    // 可能我们可以直接使用配置选项
    // 需要检查file_meta->num来判断迭代器中是否有数据
    // 迭代器应是内存表迭代器，用于返回内存表中的条目
    // 若发生错误，此函数会自动删除生成的文件
    // 参数file_name: 要生成的SSTable文件名
    // 参数file_meta: 用于存储SSTable元信息的结构体
    // 参数iter: 内存表的迭代器，提供要写入的数据(即键值对有序排列的内存表)
    // 返回值: 操作状态（成功或错误信息）
    Status BuildTable(const std::string &file_name, FileMeta *file_meta,Iterator *iter)
    {
        iter->SeekToFirst(); // 将迭代器定位到第一个元素

        // 若迭代器无效（无数据），直接返回成功状态
        if (!iter->Valid())
        {
            return Status::OK();
        }

        // 打开文件，以只写、创建模式，权限为0777
        int fd = ::open(file_name.data(), O_WRONLY | O_CREAT, 0777);
        LOG_INFO("stared build sstable, name %s", file_name.data());

        // 若文件打开失败，返回IO错误状态
        if (fd == -1)
        {
            return Status::IOError("open file error, check the file name");
        }

        // 创建可写文件对象，管理文件描述符和文件名
        WritableFile writable_file(fd, file_name);

        // 若迭代器有效，记录最小键信息
        if (iter->Valid())
        {
            Slice internal_key = iter->Key();
            file_meta->smallest_key = TransToInternalKey(internal_key);
        }

        // 创建表构建器，使用内部键比较器
        const Option opt(new InternalKeyComparator());
        TableBuilder builder(&opt, &writable_file);

        // 遍历迭代器，将所有键值对添加到表中
        while (iter->Valid())
        {
            Slice internal_key = iter->Key(), value = iter->Value();
            builder.Add(internal_key, value);

            // 更新文件元信息中的最大键
            file_meta->largest_key = TransToInternalKey(internal_key);

            // 清理内存数据（此处存在内存泄漏，需后续处理memtable_key的释放）//?
            iter->Next();
        }

        // 完成表的构建
        Status s = builder.Finish();

        // 若构建失败，删除生成的文件并返回错误状态
        if (!s.ok())
        {
            Remove(file_name);
            return s;
        }

        // 更新文件元信息中的文件大小
        file_meta->file_size = builder.FileSize();
        PrintFileMeta(*file_meta); // 打印文件元信息

        // 配置选项会自动删除内部键比较器
        return Status::OK();
    }
}; // namespace koishidb

//构建SSTable文件