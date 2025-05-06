/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        logger_fprintf.h
Version:     1.0
Author:      cjx
start date:
Description: 使用fprintf打印时的内容封装
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1            2025-05-06       cjx         create

*****************************************************************/

#pragma once

#include "logger_define.h"

#include <cstdio>
#include <thread>
#include <string>

#include "utils/utils_ways.h"

namespace PROJECT_NAME::logger
{
// 获取日志输出的文件流指针
static inline FILE *getOutTarget()
{
    static FILE *outputTarget = stderr;
    return outputTarget;
}

// 提供一个用户设置日志打印级别的接口
static inline void setOutTarget(FILE *target)
{
    getRuntimeLevel() = target;
}

// 获取日志级别字符串
static inline const char *getLevelString(int level)
{
    switch (level)
    {
    case LOG_LEVEL_TRACE:
        return "trace";
    case LOG_LEVEL_DEBUG:
        return "debug";
    case LOG_LEVEL_INFO:
        return "info";
    case LOG_LEVEL_WARNING:
        return "warning";
    case LOG_LEVEL_ERROR:
        return "error";
    case LOG_LEVEL_CRITICAL:
        return "critical";
    default:
        return "unknown";
    }
}

// 日志输出函数
static inline void logOutput(int level, const char *file, int line, const char *format, ...)
{
    if (!shouldLog(level))
        return;

    va_list args;
    va_start(args, format);

    // 获取线程ID
    std::string thread_id = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));

    // 格式化输出
    fprintf(getOutTarget(), "[%s] [" TO_STRING(PROJECT_NAME) "] [%s] [%s] [%s:%d] ",
            getCurrentTime().c_str(),
            getLevelString(level),
            thread_id.c_str(),
            file,
            line);

    vfprintf(getOutTarget(), format, args);
    fprintf(getOutTarget(), "\n");
    fflush(getOutTarget());

    va_end(args);
}
}

// 定义所有日志级别的宏
#define LOG_TRACE(...) \
    do { \
        if constexpr (LOG_LEVEL_TRACE >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_TRACE)) { \
                PROJECT_NAME::logger::logOutput(LOG_LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_DEBUG(...) \
    do { \
        if constexpr (LOG_LEVEL_DEBUG >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_DEBUG)) { \
                PROJECT_NAME::logger::logOutput(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_INFO(...) \
    do { \
        if constexpr (LOG_LEVEL_INFO >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_INFO)) { \
                PROJECT_NAME::logger::logOutput(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_WARNING(...) \
    do { \
        if constexpr (LOG_LEVEL_WARNING >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_WARNING)) { \
                PROJECT_NAME::logger::logOutput(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_ERROR(...) \
    do { \
        if constexpr (LOG_LEVEL_ERROR >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_ERROR)) { \
                PROJECT_NAME::logger::logOutput(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_CRITICAL(...) \
    do { \
        if constexpr (LOG_LEVEL_CRITICAL >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_CRITICAL)) { \
                PROJECT_NAME::logger::logOutput(LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__); \
            } \
        } \
    } while (false)


