// 该文件实现了C++11风格的线程池类，支持任务提交和并发执行
#ifndef KOISHIDB_SRC_INCLUDE_NETWORK_THREAD_POOL_H
#define KOISHIDB_SRC_INCLUDE_NETWORK_THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace koishidb
{
    // C++11风格的线程池类，管理一组工作线程并处理任务队列
    class ThreadPool
    {
    public:
        // 构造函数：初始化线程池并创建指定数量的工作线程
        // 参数threads：工作线程的数量
        ThreadPool(size_t);

        // 提交任务到线程池，返回一个future对象用于获取结果
        // 参数f：任务函数
        // 参数args：任务函数的参数
        // 返回值：包含任务返回值的future对象
        template <class F, class... Args>
        auto enqueue(F &&f, Args &&...args)
            -> std::future<typename std::result_of<F(Args...)>::type>;

        // 析构函数：停止所有工作线程并等待它们结束
        ~ThreadPool();

    private:
        // 工作线程列表
        std::vector<std::thread> workers;

        // 任务队列（存储待执行的任务）
        std::queue<std::function<void()>> tasks;

        // 同步机制
        std::mutex queue_mutex;            // 保护任务队列的互斥锁
        std::condition_variable condition; // 用于线程间通知的条件变量
        bool stop;                         // 标记线程池是否停止
    };

    // 构造函数实现：创建指定数量的工作线程
    inline ThreadPool::ThreadPool(size_t threads) : stop(false)
    {
        for (size_t i = 0; i < threads; ++i)
        {
            workers.emplace_back(
                [this]
                {
                    for (;;)
                    { // 工作线程主循环
                        std::function<void()> task;

                        {
                            // 加锁并等待任务或停止信号
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(
                                lock,
                                [this]
                                {
                                    return this->stop || !this->tasks.empty();
                                });

                            // 若线程池已停止且任务队列为空，退出线程
                            if (this->stop && this->tasks.empty())
                            {
                                return;
                            }

                            // 从队列中取出任务
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }

                        // 执行任务
                        task();
                    }
                });
        }
    }

    // 提交任务的实现
    template <class F, class... Args>
    auto ThreadPool::enqueue(F &&f, Args &&...args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using return_type = typename std::result_of<F(Args...)>::type;

        // 包装任务为packaged_task，以便获取future
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // 若线程池已停止，不能提交任务
            if (stop)
            {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            // 将任务加入队列（封装为无参函数）
            tasks.emplace(
                [task]()
                {
                    (*task)();
                });
        }

        // 通知一个工作线程有新任务
        condition.notify_one();
        return res;
    }

    // 析构函数实现：停止所有工作线程
    inline ThreadPool::~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true; // 设置停止标记
        }

        // 通知所有工作线程
        condition.notify_all();

        // 等待所有工作线程结束
        for (std::thread &worker : workers)
        {
            worker.join();
        }
    }
}; // namespace koishidb

#endif

// 仅用于服务端和客户端的网络通信处理