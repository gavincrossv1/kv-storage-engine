// 该文件实现了网络操作相关的辅助类（Helper和epoll_helper），提供文件描述符非阻塞设置及epoll事件管理功能
#include "helper.h"

#include <fcntl.h>

#include "logger.h"

namespace koishidb
{
    // 将文件描述符设置为非阻塞模式
    // 参数fd: 要设置的文件描述符
    void Helper::set_non_block(const int fd)
    {
        // 获取当前文件描述符的标志
        int flags = fcntl(fd, F_GETFL, 0);

        // 获取标志失败时输出警告日志
        if (flags < 0)
        {
            LOG_WARN("[%d] set non-block io failed", fd);
            return;
        }

        // 设置非阻塞标志（保留原有标志，追加O_NONBLOCK）
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    // 向epoll实例添加事件
    // 参数epoll_fd: epoll实例的文件描述符
    // 参数fd: 要监控的文件描述符
    // 参数state: 要监控的事件类型（如EPOLLIN、EPOLLOUT等）
    void epoll_helper::create_event(int epoll_fd, int fd, int state)
    {
        struct epoll_event event{};
        event.events = state; // 设置事件类型
        event.data.fd = fd;   // 关联被监控的文件描述符

        // 调用epoll_ctl添加事件
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    }

    // 从epoll实例中删除事件
    // 参数epoll_fd: epoll实例的文件描述符
    // 参数fd: 要停止监控的文件描述符
    // 参数state: 要删除的事件类型（需与添加时一致）
    void epoll_helper::delete_event(int epoll_fd, int fd, int state)
    {
        struct epoll_event event{};
        event.events = state; // 设置事件类型（需与添加时匹配）
        event.data.fd = fd;   // 关联被监控的文件描述符

        // 调用epoll_ctl删除事件
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event);
    }

    // 修改epoll实例中已注册的事件
    // 参数epoll_fd: epoll实例的文件描述符
    // 参数fd: 要修改监控的文件描述符
    // 参数state: 新的事件类型
    void epoll_helper::modify_event(int epoll_fd, int fd, int state)
    {
        struct epoll_event event{};
        event.events = state; // 设置新的事件类型
        event.data.fd = fd;   // 关联被监控的文件描述符

        // 调用epoll_ctl修改事件
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
    }
}; // namespace koishidb

//对epoll操作的封装