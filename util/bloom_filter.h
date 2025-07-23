// 该文件定义了布隆过滤器(Bloom Filter)的哈希函数及类结构，用于快速判断键是否可能存在于集合中
#ifndef KOISHIDB_SRC_INCLUDE_UTIL_BLOOM_FILTER_H
#define KOISHIDB_SRC_INCLUDE_UTIL_BLOOM_FILTER_H

#include "slice.h"
#include "encode.h"

namespace koishidb
{

    // 哈希函数，用于计算数据的哈希值
    // 参数data: 待哈希的数据起始地址
    // 参数n: 数据长度（字节数）
    // 参数seed: 哈希种子，用于生成不同的哈希结果(通过改变seed的来得到不同的哈希函数)
    // 返回值: 32位哈希值
    inline uint32_t Hash(const char *data, size_t n, uint32_t seed)
    {
        // 类似于murmur哈希算法
        const uint32_t m = 0xc6a4a793;
        const uint32_t r = 24;
        const char *limit = data + n; // 数据结束位置
        uint32_t h = seed ^ (n * m);  // 初始化哈希值

        // 按4字节为单位处理数据
        while (data + 4 <= limit)
        {
            uint32_t w = DecodeFixed32(data); // 解码4字节数据
            data += 4;
            h += w;
            h *= m;
            h ^= (h >> 16); // 哈希混淆
        }

        // 处理剩余不足4字节的数据
        switch (limit - data)
        {
        case 3:
            h += static_cast<uint8_t>(data[2]) << 16;
        case 2:
            h += static_cast<uint8_t>(data[1]) << 8;
        case 1:
            h += static_cast<uint8_t>(data[0]);
            h *= m;
            h ^= (h >> r); // 哈希混淆
            break;
        }
        return h;
    }

    // 布隆过滤器的基础实现
    class BloomFilter
    {
    public:
        // 构造函数：根据每个键分配的位数初始化布隆过滤器
        // 参数bits_per_key: 每个键分配的位数
        explicit BloomFilter(int bits_per_key) : bits_per_key_(bits_per_key)
        {
            // 理论计算表明，k = (m/n) * ln2 时效果最佳（m为总位数，n为键数量）
            // 此处用0.69近似ln2（约0.693）计算哈希函数数量k
            k_ = static_cast<size_t>(bits_per_key * 0.69);
            if (k_ < 1)
            {
                k_ = 1; // 哈希函数数量最小为1
            }
            if (k_ > 30)
            {
                k_ = 30; // 哈希函数数量最大为30
            }
        }

        // 根据一组键创建布隆过滤器
        // 参数keys: 键数组
        // 参数n: 键的数量
        // 参数dst: 输出参数，用于存储生成的布隆过滤器数据
        void CreateFilter(const Slice *keys, int n, std::string *dst) const;

        // 判断键是否可能存在于布隆过滤器中（可能存在误判）
        // 参数key: 待检查的键
        // 参数bloom_filter: 布隆过滤器数据
        // 返回值: true表示可能存在，false表示一定不存在
        bool KeyMayMatch(const Slice &key, const Slice &bloom_filter) const;

        int bits_per_key_; // 每个键分配的位数
        int k_;            // 哈希函数的数量(研究表明,k_是bits_per_key_*ln2时最佳)
    };

}; // namespace koishidb

#endif

// 布隆过滤器用于加速磁盘的get操作,一般来说,一个stable文件对应着多个布隆过滤器(但必须要注意,一个布隆过滤器和一个数据块并不是一一对应的关系,可能是多对一的关系)(LevelDB/RocksDB在打开SSTable文件时，会将布隆过滤器加载到内存,并使其长期加载到内存上),布隆过滤器用于判断一个键是否存在于一个文件中,可以判断一个建一定不存在于该文件中或可能存在于该文件中
// 示例说明
// 位数组大小：8位（实际应用中要大得多）
// 哈希函数数量(k)：2个
// 哈希函数1：h1(x) = x % 8
// 哈希函数2：h2(x) = (x + 3) % 8
// 添加元素 "apple" (假设哈希值为5)
// h1("apple") = 5 % 8 = 5
// h2("apple") = (5 + 3) % 8 = 0
// 查询元素 "orange" (假设哈希值为7)
// h1("orange") = 7 % 8 = 7
// h2("orange") = (7 + 3) % 8 = 2
// 检查第7位和第2位：
// 第7位=0 → "一定不存在"