// 该文件定义了DB抽象类，作为数据库的核心接口，声明了数据操作的基本方法
#ifndef KOISHIDB_SRC_INCLUDE_DB_H
#define KOISHIDB_SRC_INCLUDE_DB_H

#include <string>

#include "write_batch.h"

namespace koishidb
{
    // 数据库抽象类，定义了数据操作的基本接口
    class DB
    {
    public:
        // 插入键值对
        // 参数key: 要插入的键
        // 参数value: 要插入的值
        virtual void Put(const Slice &key, const Slice &value) = 0;

        // 查询键对应的值
        // 参数key: 要查询的键
        // 参数value: 用于存储查询结果的指针（若找到）
        // 返回值: 找到键返回true，否则返回false
        virtual bool Get(const Slice &key, std::string *value) = 0;

        // 删除指定键
        // 参数key: 要删除的键
        virtual void Delete(const Slice &key) = 0;

        // 处理批量写操作
        // 参数update: 包含多个写操作的批量对象
        virtual void Write(WriteBatch *update) = 0;

        virtual ~DB() {} // 必须加上！
    };
}; // namespace koishidb

#endif

//对外暴露的数据库接口