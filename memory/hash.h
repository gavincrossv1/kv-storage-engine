// 该文件定义了HashTable类，基于std::unordered_map实现简单的键值对存储与查询功能
#ifndef KOISHIDB_HASH_H
#define KOISHIDB_HASH_H

#include <unordered_map>

namespace koishidb
{
    // 哈希表类，提供键值对的插入和查询操作
    class HashTable
    {
    public:
        // 插入或更新键值对
        // 参数key: 要插入或更新的键
        // 参数value: 与键关联的值
        void Put(int key, int value);

        // 根据键查询对应的值
        // 参数key: 要查询的键
        // 返回值: 键对应的value（若键不存在，行为取决于std::unordered_map的默认行为）
        int Get(int key);

    private:
        std::unordered_map<int, int> m; // 底层存储结构，使用无序映射实现哈希表
    };

} // namespace koishidb

#endif // KOISHIDB_HASH_H

//对unordered_map进行了简单的封装