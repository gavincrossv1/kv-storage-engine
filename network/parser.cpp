// 该文件实现了Parser类，负责网络请求的读取、解析以及客户端信息打印功能
#include "parser.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

#include "logger.h"
#include "helper.h"

constexpr int MAX_BUF = 1024; // 最大缓冲区大小

namespace koishidb // 这个也需要加上命名空间
{

    // 从连接中读取数据并解析请求
    // 参数conn_fd: 连接的文件描述符
    // 参数conn: 连接对象（包含读取缓冲区和请求存储）
    // 返回值: 读取成功返回true，失败返回false
    bool Parser::read(int conn_fd, std::shared_ptr<connection> conn)
    {
        char buf[1024]; // 临时缓冲区
        int n = 0;      // 读取的字节数

        // 初始化请求对象
        conn->req = unique_ptr<request>(new request());
        string &read_storage = conn->read; // 引用连接的读取缓冲区

        // 从连接读取数据
        n = ::read(conn_fd, buf, sizeof(buf));
        if (n < 0)
        {
            // 读取失败，输出日志
            LOG_INFO("[%d] %d -> %s signal received \n", conn_fd, errno,
                     ::strerror(errno));
            return false;
        }
        if (n == 0) 
        {
            // 客户端关闭连接
            return false;
        }

        //LOG_INFO("[%d] received %d \n", conn_fd, n);
        // 将读取的数据追加到缓冲区
        read_storage.append(buf, n);

        // 解析所有读取到的数据
        parse_all(conn);
        return true;
    }

    // 解析缓冲区中的所有数据，填充请求对象
    // 参数conn: 连接对象（包含读取缓冲区和请求存储）
    // 参数conn_fd: 连接的文件描述符
    void Parser::parse_all(std::shared_ptr<connection> conn)
    {
        std::string &data = conn->read;
        while (true)
        {
            size_t end = data.find('\n');
            if (end == std::string::npos)
                break; // 未收到完整行

            std::string line = data.substr(0, end);
            data.erase(0, end + 1); // 移除已处理数据

            std::istringstream iss(line);
            std::string cmd, key;
            iss >> cmd >> key;

            // 跳过空行或无效命令
            if (cmd.empty() || key.empty())
            {
                continue;
            }

            conn->req = std::make_unique<request>();
            if (cmd == "put")
            {
                conn->req->op_code_ = 1;
                // put key value，value可能有空格
                std::string value;
                std::getline(iss, value);
                // 去除前导空格
                if (!value.empty() && value[0] == ' ')
                    value.erase(0, 1);
                conn->req->value_ = value;
            }
            else if (cmd == "get")
            {
                conn->req->op_code_ = 2;
            }
            else if (cmd == "delete")
            {
                conn->req->op_code_ = 3;
            }
            else
            {
                conn->req->op_code_ = 0; // 未知命令
            }
            conn->req->key_ = key;
            // 如果只处理一条命令，直接break；如需批量处理可扩展为队列
            break;
        }
    }

    // 打印客户端信息（主机名和端口）
    // 参数client_addr: 客户端地址结构体
    // 参数client_addr_len: 客户端地址长度
    void Parser::print_client_info(sockaddr *client_addr,
                                   socklen_t client_addr_len)
    {
        char host_name[MAX_BUF], host_port[MAX_BUF]; // 存储主机名和端口

        // 初始化缓冲区
        memset(host_name, 0, sizeof(host_name));
        memset(host_port, 0, sizeof(host_port));

        // 获取主机名和端口
        getnameinfo(client_addr, client_addr_len, host_name, sizeof(host_name),
                    host_port, sizeof(host_port), 0);

        // 输出客户端信息
        // LOG_INFO("Received Request from: %s, port: %s \n", host_name, host_port);
    }
}; // namespace koishidb

// 通信功能