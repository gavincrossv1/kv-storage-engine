#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <atomic>

#include "db.h"
#include "db_impl.h"
#include "logger.h"
#include "network/connection.h"
#include "network/helper.h"
#include "network/parser.h"
#include "network/request.h"
#include "network/thread_pool.h"
#include "util/concurrent_hashmap.h"

// 常量定义
constexpr int thread_pool_size = 10; // 线程池大小，控制并发处理任务数量
constexpr int MAX_EVENTS = 128;      // epoll一次可处理的最大事件数
constexpr int FD_SIZE = 1024;        // epoll文件描述符集合的大小

// 使用命名空间，简化代码书写(引入命名空间中某个具体成员)
using koishidb::concurrent_hashmap;
using koishidb::connection;
using koishidb::epoll_helper;
using koishidb::Helper;
using koishidb::Parser;
using koishidb::request;
using koishidb::ThreadPool;

// 协议格式说明：
// 1\r\n           -> Put操作（插入数据）
// user_key \r\n   -> 键
// user_value \r\n -> 值
//
// 2\r\n           -> Get操作（获取数据）
// user_key \r\n   -> 键
//
// 3\r\n           -> Delete操作（删除数据）
// user_key \r\n   -> 键
//
// 响应格式：
// 1\r\n           -> 操作成功
// user_value\r\n  -> Get操作返回的值（仅Get操作有此部分）
// 0\r\n           -> 操作失败

// 全局连接存储，使用并发哈希表管理所有客户端连接
// 键为文件描述符，值为对应的连接对象

std::atomic<bool> running(true);

void signal_handler(int signum) 
{
    running = false;
}

concurrent_hashmap<int, std::shared_ptr<connection>> connection_storage;

int main()
{
    signal(SIGINT, signal_handler); // 捕获Ctrl+C
    signal(SIGTERM, signal_handler);
    // 使用unique_ptr实现RAII，自动释放数据库资源
    // 初始化数据库实现类对象
    std::unique_ptr<koishidb::DB> db = std::make_unique<koishidb::DBimpl>(); // 毫无疑问,DBimpl是DB的子类

    // 创建线程池，用于并发处理客户端请求
    ThreadPool thread_pool(thread_pool_size);

    // 配置服务器监听地址和端口
    sockaddr_in server_addr{};
    server_addr.sin_port = htons(8096);              // 监听8096端口
    server_addr.sin_family = AF_INET;                // 使用IPv4协议
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有可用的网络接口

    // 创建监听socket
    // AF_INET: IPv4, SOCK_STREAM: 流式套接字(TCP), 0: 自动选择协议
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 设置端口复用
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval));

    // 绑定socket到指定地址和端口
    bind(listen_fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr));

    // 开始监听连接请求，等待队列长度为线程池大小
    listen(listen_fd, thread_pool_size);
    LOG_INFO("server run in port 8096"); // 记录服务器启动日志

    // 配置epoll
    // 创建epoll实例，参数为期望处理的最大文件描述符数量
    int epoll_fd = epoll_create(FD_SIZE);
    struct epoll_event events[MAX_EVENTS]; // 用于存储epoll返回的事件

    // 将监听socket加入epoll，监听读事件（新连接请求）
    epoll_helper::create_event(epoll_fd, listen_fd, EPOLLIN);

    // 主事件循环，持续处理各种事件
    while (running)
    {
        // 等待事件发生，-1表示无限期阻塞
        // 返回值为发生的事件数量
        const int active = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        // 遍历所有发生的事件
        for (int i = 0; i < active; i++)
        {
            const int fd = events[i].data.fd; // 事件对应的文件描述符
            const int ev = events[i].events;  // 事件类型

            // 处理新连接请求事件
            // 检查是否是监听socket且发生了读事件或错误事件
            if (fd == listen_fd && (ev & (EPOLLIN | EPOLLERR)))
            {
                sockaddr_in client_addr{}; // 客户端地址信息
                socklen_t client_addr_len = sizeof(client_addr);

                // 接受新连接
                // 返回值为新连接的文件描述符
                int conn_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);

                if (conn_fd != -1) // 连接建立成功
                {
                    LOG_INFO("[%d] Connection Established\n", conn_fd);

                    // 打印客户端信息（IP地址和端口）
                    Parser::print_client_info(reinterpret_cast<sockaddr *>(&client_addr), client_addr_len);

                    // 设置新连接为非阻塞模式
                    Helper::set_non_block(conn_fd);

                    // 将新连接加入epoll，使用边缘触发模式（EPOLLET）监听读事件
                    epoll_helper::create_event(epoll_fd, conn_fd, EPOLLIN | EPOLLET);

                    // 将新连接加入连接存储
                    // 如果已存在该fd的连接，先删除
                    if (connection_storage.count(conn_fd))
                    {
                        connection_storage.erase(conn_fd);
                    }
                    // 创建新的连接对象并存储
                    connection_storage.set(conn_fd, std::shared_ptr<connection>(new connection()));
                }
            }
            // 处理读事件（客户端发送数据）
            else if (ev & EPOLLIN)
            {
                int conn_fd = events[i].data.fd; // 连接的文件描述符
                // LOG_INFO("[%d] trigger epoll_in event \n", conn_fd);

                // 从连接存储中获取对应的连接对象(通过文件描述符拿到对应的connection对象)
                auto connection = connection_storage.get(conn_fd);

                // 将任务提交到线程池处理，实现异步处理
                std::future<bool> result = thread_pool.enqueue([&]() -> bool
                                                               {
                        // 读取并解析客户端请求
                        if (Parser::read(conn_fd, connection))
                        {
                            // 获取解析后的请求对象
                            std::unique_ptr<request>& req = connection->req;
                            bool flag = true;
                            
                            // 根据操作码处理不同请求
                            switch (req->op_code_)
                            {
                                // Put操作（插入数据）
                                case 1:
                                {
                                    LOG_INFO("Received Put op, key %s, value %s", req->key_.data(), req->value_.data());
                                    
                                    // 调用数据库接口插入数据
                                    db->Put(req->key_, req->value_);
                                    
                                    // 构造成功响应
                                    std::string server_answer = "1\r\n";
                                    //::明确指定调用的是系统全局的 write 函数
                                    ::write(conn_fd, server_answer.data(), server_answer.size());
                                    break;
                                }
                                
                                // Get操作（获取数据）
                                case 2:
                                {
                                    // 调用数据库接口查询数据
                                    auto result = db->Get(req->key_, &req->value_);
                                    LOG_INFO("Received Get op, key %s", req->key_.data());
                                    LOG_INFO("%s", req->value_.data());
                                    
                                    if (result && req->value_!=" ")  // 查询成功
                                    {
                                        LOG_INFO("success find");
                                        // 构造包含查询结果的响应
                                        std::string server_answer = "1\r\n";
                                        server_answer += req->value_;
                                        server_answer += "\r\n";
                                        ::write(conn_fd, server_answer.data(), server_answer.size());
                                    }
                                    else  // 查询失败（键不存在）
                                    {
                                        std::string server_answer = "0\r\n";
                                        ::write(conn_fd, server_answer.data(), server_answer.size());
                                    }
                                    break;
                                }
                                
                                // Delete操作（删除数据）
                                case 3:
                                {
                                    // 调用数据库接口删除数据
                                    db->Delete(req->key_);
                                    
                                    // 构造成功响应
                                    std::string server_answer = "1\r\n";
                                    ::write(conn_fd, server_answer.data(), server_answer.size());
                                    break;
                                }
                            }
                            // 处理完成后关闭连接
                            //close(conn_fd);
                            // 从连接存储中移除该连接
                            //connection_storage.erase(conn_fd);
                            return flag;
                        }
                        else
                        {
                            // 客户端关闭
                            close(conn_fd);
                            connection_storage.erase(conn_fd);
                        }
                        return false; });
            }
            // 注意：为了非阻塞，我们不应该调用future的get方法
        }
    }

    // 关闭监听socket（实际运行中由于主循环是无限的，这里可能永远不会执行）
    close(listen_fd);
    LOG_INFO("server shutdown");
    LOG_INFO("main return, db即将析构");
    return 0;
}