// 该文件实现了Slice类，提供轻量级字符串视图功能，高效管理字符串数据（避免不必要的内存复制）
#include "slice.h"

#include "logger.h"

namespace koishidb
{
    // 获取字符串数据的指针
    const char *Slice::data() const
    {
        return data_;
    }

    // 获取字符串的长度
    size_t Slice::size() const
    {
        return size_;
    }

    // 判断字符串是否为空
    bool Slice::empty() const
    {
        return size_ == 0;
    }

    // 默认构造函数：初始化空字符串
    Slice::Slice() : data_(""), size_(0) {}

    // 构造函数：根据C字符串初始化（自动计算长度）
    // 参数d：C字符串指针（以'\0'结尾）
    Slice::Slice(const char *d) : data_(d), size_(strlen(d)) {}

    // 构造函数：根据指针和长度初始化
    // 参数d：字符串数据指针
    // 参数size：字符串长度
    Slice::Slice(const char *d, size_t size) : data_(d), size_(size) {}

    // 构造函数：根据std::string初始化
    // 参数s：std::string对象
    Slice::Slice(const std::string &s) : data_(s.data()), size_(s.size()) {}

    // 重载[]运算符：获取指定位置的字符
    // 参数n：字符索引（需小于字符串长度）
    char Slice::operator[](size_t n) const
    {
        assert(n < size_);
        return data_[n];
    }

    // 重载==运算符：判断两个Slice是否相等
    bool Slice::operator==(const Slice &that) const
    {
        if (that.size() != size_)
        {
            return false;
        }
        // 比较内存中的字符（长度相同）
        int ret = memcmp(data_, that.data(), size_);
        return ret == 0;
    }

    // 重载!=运算符：判断两个Slice是否不相等
    bool Slice::operator!=(const Slice &that) const
    {
        return !(*this == that);
    }

    // 比较两个Slice的大小（字典序）
    // 返回值：-1表示当前Slice小，1表示当前Slice大，0表示相等
    int Slice::Compare(const Slice &that) const
    {
        const size_t min_len = (size_ < that.size_) ? size_ : that.size_;
        int r = memcmp(data_, that.data_, min_len); // 比较前min_len个字符

        if (r < 0)
        {
            return -1;
        }
        else if (r > 0)
        {
            return 1;
        }
        else
        {
            // 前min_len个字符相同，短的Slice更小
            if (size_ < that.size_)
            {
                r = -1;
            }
            else if (size_ > that.size_)
            {
                r = +1;
            }
            return r;
        }
    }

    // 跳过前n个字符（移动数据指针，减小长度）
    // 参数n：要跳过的字符数（需小于等于当前长度）
    void Slice::Advance(size_t n)
    {
        assert(n <= size_);
        data_ += n;
        size_ -= n;
    }

    // 清空Slice（指向空字符串）
    void Slice::Clear()
    {
        data_ = "";
        size_ = 0;
    }

    // 将Slice转换为std::string
    std::string Slice::ToString() const
    {
        return std::string(data_, size_);
    }
}; // namespace koishidb

//内存数据片段,不持有实际数据，而是引用（指向）某块内存的起始地址和长度,高度减少拷贝消耗