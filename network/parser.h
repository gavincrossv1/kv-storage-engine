// 该文件声明了Parser类，提供网络请求的读取、解析以及客户端信息打印的接口
#ifndef KOISHIDB_SRC_INCLUDE_PARSER_H
#define KOISHIDB_SRC_INCLUDE_PARSER_H
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include <cstring>
#include <fstream>
#include <memory>

#include "connection.h"

namespace koishidb
{
    // 解析器类，负责读取和解析网络请求，以及打印客户端信息
    class Parser
    {
    public:
        // 打印客户端的主机名和端口信息
        // 参数client_addr: 客户端地址结构体
        // 参数client_addr_len: 客户端地址长度
        static void print_client_info(sockaddr *client_addr,
                                      socklen_t client_addr_len);

        // 从连接读取数据并解析为请求对象
        // 参数conn_fd: 连接的文件描述符
        // 参数conn: 连接对象（包含读取缓冲区和请求存储）
        // 返回值: 读取成功返回true，失败返回false
        static bool read(int conn_fd, std::shared_ptr<connection> conn);

    private:
        // 解析缓冲区中的所有数据，填充请求对象（内部实现）
        // 参数conn: 连接对象（包含读取缓冲区和请求存储）
        // 参数conn_fd: 连接的文件描述符
        static void parse_all(std::shared_ptr<connection> conn, int conn_fd);
    };

}; // namespace koishidb
#endif

//通信功能