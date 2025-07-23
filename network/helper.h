// 该文件声明了网络操作相关的辅助类（Helper和epoll_helper），包含文件描述符非阻塞设置及epoll事件管理的接口
#ifndef KOISHIDB_SRC_INCLUDE_NETWORK_HELPER_H
#define KOISHIDB_SRC_INCLUDE_NETWORK_HELPER_H

#include <sys/epoll.h>

namespace koishidb
{
    // 网络操作辅助类，提供文件描述符相关的辅助功能
    class Helper
    {
    public:
        // 将文件描述符设置为非阻塞模式,目的是为了使用epoll的边缘触发ET
        // 参数fd: 要设置的文件描述符
        static void set_non_block(const int fd);
    };

    // epoll事件辅助类，提供epoll事件的添加、删除和修改功能
    class epoll_helper
    {
    public:
        // 向epoll实例添加事件
        // 参数epoll_fd: epoll实例的文件描述符
        // 参数fd: 要监控的文件描述符
        // 参数state: 要监控的事件类型（如EPOLLIN、EPOLLOUT等）
        static void create_event(int epoll_fd, int fd, int state);

        // 从epoll实例中删除事件
        // 参数epoll_fd: epoll实例的文件描述符
        // 参数fd: 要停止监控的文件描述符
        // 参数state: 要删除的事件类型
        static void delete_event(int epoll_fd, int fd, int state);

        // 修改epoll实例中已注册的事件
        // 参数epoll_fd: epoll实例的文件描述符
        // 参数fd: 要修改监控的文件描述符
        // 参数state: 新的事件类型
        static void modify_event(int epoll_fd, int fd, int state);
    };

}; // namespace koishidb

#endif

//对epoll操作的封装