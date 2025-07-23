// 该文件实现了HashTable类的基本操作，提供键值对的存储和获取功能
#include "hash.h"

namespace koishidb 
{
    // 根据键获取对应的值
    // 参数key: 要查找的键
    // 返回值: 键对应的value
    int HashTable::Get(int key) 
    { 
        return m[key]; 
    }

    // 插入或更新键值对
    // 参数key: 要插入或更新的键
    // 参数value: 要关联的值
    void HashTable::Put(int key, int value) 
    { 
        m[key] = value; 
    }
}  // namespace koishidb

//对unordered_map进行了简单的封装