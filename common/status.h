// 该文件声明了Status类，用于表示操作的执行结果（成功或错误信息），定义了错误码、状态检查及转换方法
#ifndef KOISHIDB_SRC_INCLUDE_COMMON_STATUS_H
#define KOISHIDB_SRC_INCLUDE_COMMON_STATUS_H
#include <algorithm>
#include <string>

#include "slice.h"

namespace koishidb
{
    // 状态类：用于表示操作结果（成功或错误），借鉴自LevelDB的Status设计
    class Status
    {
    public:
        // 创建成功状态
        Status() noexcept : state_(nullptr) {}

        // 析构函数：释放状态数据
        ~Status() { delete[] state_; }

        // 拷贝构造函数
        Status(const Status &rhs);

        // 赋值运算符
        Status &operator=(const Status &rhs);

        // 移动构造函数
        Status(Status &&rhs) noexcept : state_(rhs.state_) { rhs.state_ = nullptr; }

        // 移动赋值运算符
        Status &operator=(Status &&rhs) noexcept;

        // 返回成功状态
        static Status OK() { return Status(); }

        // 返回各种错误状态
        static Status NotFound(const Slice &msg, const Slice &msg2 = Slice())
        {
            return Status(kNotFound, msg, msg2);
        }
        static Status Corruption(const Slice &msg, const Slice &msg2 = Slice())
        {
            return Status(kCorruption, msg, msg2);
        }
        static Status NotSupported(const Slice &msg, const Slice &msg2 = Slice())
        {
            return Status(kNotSupported, msg, msg2);
        }
        static Status InvalidArgument(const Slice &msg, const Slice &msg2 = Slice())
        {
            return Status(kInvalidArgument, msg, msg2);
        }
        static Status IOError(const Slice &msg, const Slice &msg2 = Slice())
        {
            return Status(kIOError, msg, msg2);
        }

        // 判断状态是否为成功
        bool ok() const { return (state_ == nullptr); }

        // 判断状态是否为"未找到"错误
        bool IsNotFound() const { return code() == kNotFound; }

        // 判断状态是否为"数据损坏"错误
        bool IsCorruption() const { return code() == kCorruption; }

        // 判断状态是否为"IO错误"
        bool IsIOError() const { return code() == kIOError; }

        // 判断状态是否为"不支持"错误
        bool IsNotSupportedError() const { return code() == kNotSupported; }

        // 判断状态是否为"无效参数"错误
        bool IsInvalidArgument() const { return code() == kInvalidArgument; }

        // 将状态转换为字符串（用于打印），成功状态返回"OK"
        std::string ToString() const;

    private:
        // 错误码枚举
        enum Code
        {
            kOk = 0,              // 成功
            kNotFound = 1,        // 未找到
            kCorruption = 2,      // 数据损坏
            kNotSupported = 3,    // 不支持的操作
            kInvalidArgument = 4, // 无效参数
            kIOError = 5          // IO错误
        };

        // 获取错误码（成功状态返回kOk）
        Code code() const
        {
            return (state_ == nullptr) ? kOk : static_cast<Code>(state_[4]);
        }

        // 构造函数：根据错误码和消息创建状态（内部使用）
        Status(Code code, const Slice &msg, const Slice &msg2);

        // 复制状态数据（内部使用）
        static const char *CopyState(const char *s);

        // 状态数据：nullptr表示成功状态；否则为动态分配的数组，格式为：
        // [0..3]：消息长度（uint32_t）
        // [4]：错误码（Code）
        // [5..]：消息内容
        const char *state_;
    };

    // 拷贝构造函数实现
    inline Status::Status(const Status &rhs)
    {
        state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
    }

    // 赋值运算符实现
    inline Status &Status::operator=(const Status &rhs)
    {
        if (state_ != rhs.state_)
        {
            delete[] state_;
            state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
        }
        return *this;
    }

    // 移动赋值运算符实现
    inline Status &Status::operator=(Status &&rhs) noexcept
    {
        std::swap(state_, rhs.state_);
        return *this;
    }

}; // namespace koishidb

#endif // KOISHIDB_STATUS_H

//用于表示一个函数的执行结果(正确或错误),专门用一个类进行分析,status_为空则表示成功,否则表示失败