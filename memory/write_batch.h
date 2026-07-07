// 该文件声明了WriteBatch类及WriteBatchInternal类，支持批量写入操作（Put/Delete），可高效插入内存表，内部维护操作序列
#ifndef KOISHIDB_SRC_INCLUDE_MEMORY_WRITE_BATCH_H
#define KOISHIDB_SRC_INCLUDE_MEMORY_WRITE_BATCH_H

#include <string>
#include <vector>

#include "common.h"
#include "memtable.h"
#include "status.h"
#include "util.h"

namespace koishidb
{
    // WriteBatch格式说明：
    // 头部（kWriteBatchHeader = 12字节）：8字节序列号 + 4字节操作计数
    // 内容：每个操作包含：1字节操作类型（KeyType） + 长度前缀的用户键 + （Put操作含）长度前缀的值
    class WriteBatch
    {
    public:
        // 构造函数：初始化批量操作（创建头部空间）
        WriteBatch();

        // 拷贝构造函数：默认实现
        WriteBatch(const WriteBatch &that) = default;

        // 赋值运算符：默认实现
        WriteBatch &operator=(const WriteBatch &that) = default;

        // 析构函数：默认实现
        ~WriteBatch() = default;

        // 添加Put操作（插入键值对）
        // 参数key：键
        // 参数value：值
        void Put(const Slice &key, const Slice &value);

        // 添加Delete操作（删除键）
        // 参数key：要删除的键
        void Delete(const Slice &key);

        // 将批量操作中的所有操作插入到内存表
        // 参数memtable：目标内存表
        void InsertAll(Memtable *memtable);

        // 追加另一个批量操作到当前批量操作
        // 参数that：要追加的批量操作
        void Append(const WriteBatch &that);

#ifdef DEBUG
        // 调试用：迭代批量操作中的所有键值对
        std::vector<std::pair<std::string, std::string>> Iterate();
#endif

        // 估计批量操作的大小（字节数）
        size_t EstimatedSize();

        // 清空批量操作
        void Clear();

    private:
        friend class WriteBatchInternal; // 允许内部类访问私有成员

        std::string rep_; // 存储批量操作的二进制数据（头部+操作序列）
    };

    // 内部工具类：提供静态方法操作WriteBatch，隐藏内部实现细节
    class WriteBatchInternal
    {
    public:
        // 获取批量操作中的操作总数
        // 参数w：批量操作对象
        // 返回值：操作总数
        static int Count(const WriteBatch *w);

        // 设置批量操作中的操作总数
        // 参数w：批量操作对象
        // 参数count：操作总数
        static void SetCount(WriteBatch *w, int count);

        // 获取批量操作的起始序列号
        // 参数w：批量操作对象
        // 返回值：起始序列号
        static SequenceNumber Sequence(WriteBatch *w);

        // 设置批量操作的起始序列号
        // 参数w：批量操作对象
        // 参数seq：起始序列号
        static void SetSequence(WriteBatch *w, SequenceNumber seq);

        // 将源批量操作追加到目标批量操作
        // 参数src：源批量操作
        // 参数dst：目标批量操作
        static void Append(const WriteBatch *src, WriteBatch *dst);

        // 将批量操作插入到内存表
        // 参数src：批量操作对象
        // 参数memtable：目标内存表
        static void InsertIntoMemtable(WriteBatch *src, Memtable *memtable);

        // 获取批量操作的二进制数据大小
        // 参数w：批量操作对象
        // 返回值：数据大小（字节数）
        static size_t Bytes(const WriteBatch *w) { return w->rep_.size(); };

        // 获取批量操作的原始二进制内容，用于 WAL 持久化
        static Slice Contents(const WriteBatch *w);

        // 从 WAL 恢复批量操作的原始二进制内容
        static Status SetContents(WriteBatch *w, const Slice &contents);

        // 校验原始二进制内容是否是完整、可解析的 WriteBatch
        static Status ValidateContents(const Slice &contents);
    };

#ifdef DEBUG
    // 调试用：迭代批量操作中的所有键值对
    inline std::vector<std::pair<std::string, std::string>> WriteBatch::Iterate()
    {
        int count = WriteBatchInternal::Count(this);
        SequenceNumber number = WriteBatchInternal::Sequence(this);
        Slice entries(rep_);
        entries.Advance(kWriteBatchHeader); // 跳过头部

        std::vector<std::pair<std::string, std::string>> result;
        for (int i = 0; i < count; i++)
        {
            char key_type = entries.data()[0]; // 获取操作类型
            entries.Advance(1);                // 跳过操作类型

            switch (static_cast<KeyType>(key_type))
            {
            case KeyType::kTypeValue:
            {
                // 解析Put操作的键和值
                std::string user_key;
                GetLengthPrefixedSlice(&user_key, &entries);
                std::string value;
                GetLengthPrefixedSlice(&value, &entries);
                result.push_back({user_key, value});
                break;
            }
            case KeyType::kTypeDeletion:
            {
                // 解析Delete操作的键
                std::string user_key;
                GetLengthPrefixedSlice(&user_key, &entries);
                result.push_back({user_key, ""});
                break;
            }
            default:
            {
                // 无效操作类型，返回空
                return {};
            }
            }
        }
        return result;
    }
#endif

}; // namespace koishidb

#endif


//为了提升put和delete的速度(锁竞争),我们把多个的put和delete操作封装为一个writebatch对象(包括序列号,操作次数,多个操作(操作类型+用户键+值)),再一起写入内存