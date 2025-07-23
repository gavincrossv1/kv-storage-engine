// 该文件定义了request类，用于表示网络请求，包含操作码、键和值信息
#ifndef KOISHIDB_SRC_INCLUDE_REQUEST_H
#define KOISHIDB_SRC_INCLUDE_REQUEST_H

#include <string>
#include <unordered_map>

namespace koishidb
{
    using std::string, std::unordered_map;

    // 请求类，用于存储网络请求的信息
    class request
    {
    public:
        int op_code_;       // 操作码（如GET、PUT等）
        std::string key_;   // 请求的键
        std::string value_; // 请求的值（用于PUT等操作）
    };
}; // namespace koishidb

#endif

//一个请求