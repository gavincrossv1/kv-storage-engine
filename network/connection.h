// 该文件定义了connection类，用于管理客户端与服务器之间的连接状态，包括读写数据和请求信息
#ifndef KOISHIDB_SRC_INCLUDE_CONNECTION_H
#define KOISHIDB_SRC_INCLUDE_CONNECTION_H

#include <cstring>
#include <iostream>
#include <memory>

#include "request.h"

namespace koishidb
{
    // 引入标准库中的string和unique_ptr
    using std::string, std::unique_ptr;

    // 连接类，用于维护客户端连接的相关数据
    class connection
    {
    public:
        string read;             // 存储从客户端读取的数据
        string write;            // 存储待发送给客户端的数据
        unique_ptr<request> req; // 指向解析后的请求对象的智能指针

        // 构造函数：初始化请求指针为空
        connection()
        {
            req = nullptr;
        }
    };
}; // namespace koishidb

#endif

//一个tcp连接