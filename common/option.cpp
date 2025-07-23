// 该文件实现了Option类，用于管理数据库的配置选项，主要持有比较器并负责其生命周期管理
#include "option.h"

#include "memtable.h"
#include "comparator.h"

namespace koishidb
{
    // 默认构造函数：使用内存表键比较器
    Option::Option() : cmp(new MemtableKeyComparator()) {}

    // 析构函数：释放比较器
    Option::~Option()
    {
        if (cmp != nullptr)
        {
            delete cmp;
        }
    }

    // 构造函数：使用指定的比较器
    Option::Option(const Comparator *cmp) : cmp(cmp) {}
}; // namespace koishidb

//确保比较器正确初始化和释放(有多个比较器,这保证了无论是哪个比较器,都用Option这一个接口进行工作)