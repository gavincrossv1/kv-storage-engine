// 该文件实现了布隆过滤器(Bloom Filter)，用于快速判断一个键是否可能存在于集合中，存在一定的误判率
#include "bloom_filter.h"

namespace koishidb
{
    // 布隆过滤器的哈希函数，用于计算键的哈希值
    // 参数key: 待计算哈希的键
    // 返回值: 32位哈希值
    static uint32_t BloomHash(const Slice &key)
    {
        // 调用基础哈希函数，使用0x66ccff作为初始种子
        return Hash(key.data(), key.size(), 0x66ccff);
    }

    // 布隆过滤器设计借鉴自LevelDB
    // 创建布隆过滤器
    // 参数keys: 键数组，用于构建过滤器
    // 参数n: 键的数量
    // 参数dst: 输出参数，用于存储构建好的布隆过滤器数据(就是一个数组)
    void BloomFilter::CreateFilter(const Slice *keys, int n, std::string *dst) const
    {
        // 计算布隆过滤器的大小（以位和字节为单位）
        size_t bits = n * bits_per_key_;

        // 对于小数据集，可能会有很高的误判率，因此强制设置最小布隆过滤器长度为64位
        if (bits < 64)
            bits = 64;

        // 计算所需字节数（向上取整）
        size_t bytes = (bits + 7) / 8;
        bits = bytes * 8; // 实际使用的位数（确保是8的倍数）

        const size_t init_size = dst->size();
        dst->resize(init_size + bytes, 0);             // 预留空间并初始化为0
        dst->push_back(static_cast<char>(k_));  // 存储哈希函数的数量k，用于后续验证

        char *array = &(*dst)[init_size];  // 指向过滤器数据的起始位置

        // 为每个键更新布隆过滤器
        for (int i = 0; i < n; i++)
        {
            // 使用哈希生成一系列哈希值
            uint32_t h = BloomHash(keys[i]);
            // 计算增量值（将哈希值右移17位再与左移15位的值进行或运算，实现循环右移17位）
            const uint32_t delta = (h >> 17) | (h << 15);

            // 对每个哈希函数，计算并设置对应的位(总共要进行k_次哈希操作)
            for (size_t j = 0; j < k_; j++)
            {
                const uint32_t bitpos = h % bits;         // 计算位位置
                array[bitpos / 8] |= (1 << (bitpos % 8)); // 设置对应位
                h += delta;                               // 更新哈希值
            }
        }
    }

    // 判断键是否可能存在于布隆过滤器中（可能存在误判）
    // 参数key: 待检查的键
    // 参数bloom_filter: 布隆过滤器数据
    // 返回值: true表示可能存在，false表示一定不存在
    bool BloomFilter::KeyMayMatch(const Slice &key, const Slice &bloom_filter) const
    {
        const size_t len = bloom_filter.size();
        // 过滤器长度小于2字节（至少需要1字节数据+1字节k值），视为不存在
        if (len < 2)
            return false;

        const char *array = bloom_filter.data();
        const size_t bits = (len - 1) * 8; // 计算实际有效的位数（减去存储k的1字节）

        // 使用编码的k值，以便能够读取使用不同参数生成的过滤器
        const size_t k = array[len - 1];
        // k值大于30，视为预留的新编码格式，暂认为匹配
        if (k > 30)
        {
            return true;
        }

        // 计算键的哈希值
        uint32_t h = BloomHash(key);
        const uint32_t delta = (h >> 17) | (h << 15); // 循环右移17位

        // 检查所有k个哈希位置
        for (size_t j = 0; j < k; j++)
        {
            const uint32_t bitpos = h % bits;
            // 如果有任何一位未设置，则键一定不存在
            if ((array[bitpos / 8] & (1 << (bitpos % 8))) == 0)
                return false;
            h += delta; // 更新哈希值
        }

        // 所有位都设置了，键可能存在
        return true;
    }

}; // namespace koishidb


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