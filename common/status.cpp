// 该文件实现了Status类，用于表示操作的执行结果（成功或错误信息），支持多种错误类型及状态转换
#include "status.h"

namespace koishidb
{
    // 复制状态数据（用于状态的拷贝）
    // 参数state：源状态数据
    // 返回值：复制后的状态数据指针
    const char *Status::CopyState(const char *state)
    {
        uint32_t size;
        std::memcpy(&size, state, sizeof(size)); // 读取状态数据大小
        char *result = new char[size + 5];       // 分配新内存（4字节信息长度+1字节错误码+长度大小）
        std::memcpy(result, state, size + 5);    // 复制状态数据
        return result;
    }

    // 构造函数：根据错误码和消息创建状态
    // 参数code：错误码
    // 参数msg：主消息
    // 参数msg2：附加消息（可选）
    Status::Status(Code code, const Slice &msg, const Slice &msg2)
    {
        assert(code != kOk); // 成功状态无需通过此构造函数创建

        const uint32_t len1 = static_cast<uint32_t>(msg.size());
        const uint32_t len2 = static_cast<uint32_t>(msg2.size());
        const uint32_t size = len1 + (len2 ? (2 + len2) : 0); // 总消息大小（含分隔符）

        char *result = new char[size + 5];         // 分配内存（4字节信息长度+1字节错误码+长度大小）
        std::memcpy(result, &size, sizeof(size));  // 存储消息大小
        result[4] = static_cast<char>(code);       // 存储错误码
        std::memcpy(result + 5, msg.data(), len1); // 存储主消息

        // 若有附加消息，添加分隔符和附加消息
        if (len2)
        {
            result[5 + len1] = ':';
            result[6 + len1] = ' ';
            std::memcpy(result + 7 + len1, msg2.data(), len2);
        }

        state_ = result; // 保存状态数据
    }

    // 将状态转换为字符串（用于打印）
    std::string Status::ToString() const
    {
        if (state_ == nullptr)
        {
            return "OK"; // 成功状态
        }
        else
        {
            char tmp[30];
            const char *type;

            // 根据错误码确定类型字符串
            switch (code())
            {
            case kOk:
                type = "OK";
                break;
            case kNotFound:
                type = "NotFound: ";
                break;
            case kCorruption:
                type = "Corruption: ";
                break;
            case kNotSupported:
                type = "Not implemented: ";
                break;
            case kInvalidArgument:
                type = "Invalid argument: ";
                break;
            case kIOError:
                type = "IO error: ";
                break;
            default:
                std::snprintf(tmp, sizeof(tmp), "Unknown code(%d): ", static_cast<int>(code()));
                type = tmp;
                break;
            }

            // 构建结果字符串
            std::string result(type);
            uint32_t length;
            std::memcpy(&length, state_, sizeof(length)); // 读取消息长度
            result.append(state_ + 5, length);            // 追加消息内容
            return result;
        }
    }
}; // namespace koishidb

//用于表示一个函数的执行结果(正确或错误),专门用一个类进行分析,status_为空则表示成功,否则表示失败