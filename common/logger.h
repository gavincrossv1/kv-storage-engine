// 该文件提供日志输出功能，支持不同级别（ERROR、WARN、INFO、DEBUG、TRACE）的日志，可通过编译选项控制日志级别
//===----------------------------------------------------------------------===//
//
//                         KoishiDB
//
// logger.h
//
// Identification: src/include/logger.h
//
//
//
//===----------------------------------------------------------------------===//

/***************************************************************************
 *   Copyright (C) 2008 by H-Store Project                                 *
 *   Brown University                                                      *
 *   Massachusetts Institute of Technology                                 *
 *   Yale University                                                       *
 *                                                                         *
 *   This software may be modified and distributed under the terms         *
 *   of the MIT license.  See the LICENSE file for details.                *
 *                                                                         *
 ***************************************************************************/

#ifndef KOISHIDB_SRC_INCLUDE_LOGGER_H
#define KOISHIDB_SRC_INCLUDE_LOGGER_H

/**
 * 日志功能用于输出调试、错误等信息。与性能计数器不同，
 * 日志通过LOG_LEVEL编译选项控制开关，目标是在关闭日志时不影响运行性能。
 * 使用LOG_XXX_ENABLED宏可在发布版本中完全移除调试代码。
 * @author Hideaki
 */

#include <ctime>
#include <string>

using cstr = const char *;

// 辅助函数：获取文件名中最后一个斜杠后的部分（短文件名）
static constexpr auto PastLastSlash(cstr a, cstr b) -> cstr
{
    return *a == '\0'  ? b
           : *b == '/' ? PastLastSlash(a + 1, a + 1)
                       : PastLastSlash(a + 1, b);
}

static constexpr auto PastLastSlash(cstr a) -> cstr
{
    return PastLastSlash(a, a);
}

// 宏：获取短文件名（不含路径）
#define __SHORT_FILE__                                \
    ({                                                \
        constexpr cstr sf__{PastLastSlash(__FILE__)}; \
        sf__;                                         \
    })

// 日志级别定义
static constexpr int LOG_LEVEL_OFF = 1000;  // 关闭所有日志
static constexpr int LOG_LEVEL_ERROR = 500; // 错误日志
static constexpr int LOG_LEVEL_WARN = 400;  // 警告日志
static constexpr int LOG_LEVEL_INFO = 300;  // 信息日志
static constexpr int LOG_LEVEL_DEBUG = 200; // 调试日志
static constexpr int LOG_LEVEL_TRACE = 100; // 跟踪日志
static constexpr int LOG_LEVEL_ALL = 0;     // 所有日志

#define LOG_LOG_TIME_FORMAT "%Y-%m-%d %H:%M:%S" // 日志时间格式
#define LOG_OUTPUT_STREAM stdout                // 日志输出流

// 编译选项：日志级别（未指定时根据调试模式自动设置）
#ifndef LOG_LEVEL
#ifndef NDEBUG
// 调试模式下默认日志级别为DEBUG
static constexpr int LOG_LEVEL = LOG_LEVEL_DEBUG;
#else
// 非调试模式下默认日志级别为INFO
static constexpr int LOG_LEVEL = LOG_LEVEL_INFO;
#endif
#endif

// 兼容不支持__FUNCTION__的编译器
#if !defined(__FUNCTION__) && !defined(__GNUC__)
#define __FUNCTION__ ""
#endif

// 输出日志头部信息（时间、文件、行号、函数、级别）
void OutputLogHeader(const char *file, int line, const char *func, int level);

// 错误日志宏定义（根据日志级别决定是否启用）
#ifdef LOG_ERROR_ENABLED
#undef LOG_ERROR_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR_ENABLED
#define LOG_ERROR(...)                                                        \
    OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_ERROR); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__);                                \
    fprintf(LOG_OUTPUT_STREAM, "\n");                                         \
    ::fflush(stdout)
#else
#define LOG_ERROR(...) ((void)0)
#endif

// 警告日志宏定义
#ifdef LOG_WARN_ENABLED
#undef LOG_WARN_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN_ENABLED
#define LOG_WARN(...)                                                        \
    OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_WARN); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__);                               \
    fprintf(LOG_OUTPUT_STREAM, "\n");                                        \
    ::fflush(stdout)
#else
#define LOG_WARN(...) ((void)0)
#endif

// 信息日志宏定义
#ifdef LOG_INFO_ENABLED
#undef LOG_INFO_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO_ENABLED
#define LOG_INFO(...)                                                        \
    OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_INFO); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__);                               \
    fprintf(LOG_OUTPUT_STREAM, "\n");                                        \
    ::fflush(stdout)
#else
#define LOG_INFO(...) ((void)0)
#endif

// 调试日志宏定义
#ifdef LOG_DEBUG_ENABLED
#undef LOG_DEBUG_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG_ENABLED
#define LOG_DEBUG(...)                                                        \
    OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_DEBUG); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__);                                \
    fprintf(LOG_OUTPUT_STREAM, "\n");                                         \
    ::fflush(stdout)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

// 跟踪日志宏定义
#ifdef LOG_TRACE_ENABLED
#undef LOG_TRACE_ENABLED
#endif
#if LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE_ENABLED
#define LOG_TRACE(...)                                                        \
    OutputLogHeader(__SHORT_FILE__, __LINE__, __FUNCTION__, LOG_LEVEL_TRACE); \
    ::fprintf(LOG_OUTPUT_STREAM, __VA_ARGS__);                                \
    fprintf(LOG_OUTPUT_STREAM, "\n");                                         \
    ::fflush(stdout)
#else
#define LOG_TRACE(...) ((void)0)
#endif

// 输出日志头部信息，格式为：[时间] [文件:行号:函数] [级别] -
inline void OutputLogHeader(const char *file, int line, const char *func,
                            int level)
{
    time_t t = ::time(nullptr);
    tm *curTime = localtime(&t); // 获取本地时间
    char time_str[32];           // 存储时间字符串
    ::strftime(time_str, 32, LOG_LOG_TIME_FORMAT, curTime);

    // 根据级别设置日志类型字符串
    const char *type;
    switch (level)
    {
    case LOG_LEVEL_ERROR:
        type = "ERROR";
        break;
    case LOG_LEVEL_WARN:
        type = "WARN ";
        break;
    case LOG_LEVEL_INFO:
        type = "INFO ";
        break;
    case LOG_LEVEL_DEBUG:
        type = "DEBUG";
        break;
    case LOG_LEVEL_TRACE:
        type = "TRACE";
        break;
    default:
        type = "UNKWN";
    }

    // 输出日志头部
    ::fprintf(LOG_OUTPUT_STREAM, "%s [%s:%d:%s] %s - ", time_str, file, line,
              func, type);
}

#endif

//日志的实现