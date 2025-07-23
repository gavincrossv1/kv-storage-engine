// 该文件定义了Option类，用于存储数据库的配置选项，主要包含比较器，负责其生命周期管理
#ifndef KOISHIDB_SRC_INCLUDE_COMMON_OPTION_H
#define KOISHIDB_SRC_INCLUDE_COMMON_OPTION_H

namespace koishidb
{
    // 前向声明
    class Comparator;

    // 数据库配置选项结构体，持有比较器并负责其销毁
    struct Option
    {
        // 默认构造函数：使用默认比较器
        Option();

        // 构造函数：使用指定的比较器
        Option(const Comparator *opt);

        // 析构函数：释放比较器
        ~Option();

        const Comparator *cmp; // 比较器（用于键比较）

        
    };

}; // namespace koishidb

#endif

//确保比较器正确初始化和释放(有多个比较器,这保证了无论是哪个比较器,都用Option这一个接口进行工作)