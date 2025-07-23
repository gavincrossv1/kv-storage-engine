// 该文件实现了线程安全的并发哈希表，通过读写锁实现多线程环境下的安全访问
#ifndef KOISHIDB_SRC_INCLUDE_CONCURRENT_HASHMAP_H
#define KOISHIDB_SRC_INCLUDE_CONCURRENT_HASHMAP_H

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace koishidb
{
    // 引入标准库中的共享锁相关类和无序映射
    using std::shared_mutex, std::shared_lock, std::unique_lock;
    using std::unordered_map;

    // 并发哈希表模板类，支持多线程安全访问
    // 模板参数K: 键的类型，V: 值的类型
    template <class K, class V>
    class concurrent_hashmap
    {
    public:
        // 检查指定键是否存在
        int count(K key);

        // 获取指定键对应的值
        V get(K key);

        // 设置指定键的值（若键已存在则覆盖）
        void set(K key, V value);

        // 删除指定键
        void erase(K key);

    private:
        unordered_map<K, V> data;   // 底层存储数据的无序哈希表
        mutable shared_mutex mutex; // 共享互斥锁，支持多读者单写者模式
    };

    // 检查指定键是否存在
    // 参数key: 要检查的键
    // 返回值: 1表示存在，0表示不存在
    template <class K, class V>
    int concurrent_hashmap<K, V>::count(K key)
    {
        // 共享锁：允许多个线程同时读取
        shared_lock<shared_mutex> lock(this->mutex);
        return this->data.count(key);
    }

    // 获取指定键对应的值
    // 参数key: 要获取值的键
    // 返回值: 键对应的value（若键不存在，行为取决于底层unordered_map的默认构造）
    template <class K, class V>
    V concurrent_hashmap<K, V>::get(K key)
    {
        // 共享锁：允许多个线程同时读取
        shared_lock<shared_mutex> lock(this->mutex);
        return this->data[key];
    }

    // 设置指定键的值
    // 参数key: 要设置的键，value: 要关联的值
    template <class K, class V>
    void concurrent_hashmap<K, V>::set(K key, V value)
    {
        // 独占锁：仅允许一个线程写入，其他线程（读/写）需等待
        unique_lock<shared_mutex> lock(this->mutex);
        this->data[key] = value;
    }

    // 删除指定键
    // 参数key: 要删除的键
    template <class K, class V>
    void concurrent_hashmap<K, V>::erase(K key)
    {
        // 独占锁：仅允许一个线程写入，其他线程（读/写）需等待
        unique_lock<shared_mutex> lock(this->mutex);
        // 若键存在则删除
        if (this->data.count(key))
        {
            this->data.erase(key);
        }
    }
}; // namespace koishidb

#endif

//用于存储文件描述符和connection对象的对应关系