/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        looger_define.h
Version:     1.0
Author:      cjx
start date:
Description: 日志打印接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1            2025-05-03       cjx         create

*****************************************************************/

#ifndef LOGGER_DEFINE_
#define LOGGER_DEFINE_

#include <stdio.h>

// 宏字符串化工具
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

// 定义日志级别宏
#define LOG_LEVEL_TRACE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_CRITICAL 5

// 设置全局日志级别
#ifndef GLOBAL_LOG_LEVEL
    #define GLOBAL_LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifndef PROJECT_NAME
    #define PROJECT_NAME system
#endif

#ifdef ENABLE_LOGGING

#ifdef LOGGING_SCHEME_SPDLOG
    #include "logger_spdlog.h"
#else
    #include "logger_ostream.h"
#endif
namespace PROJECT_NAME::logger
{
// 运行时日志级别（初始化为编译时级别）
static inline int &getRuntimeLevel()
{
    static int runtimeLogLevel = GLOBAL_LOG_LEVEL;
    return runtimeLogLevel;
}

// 提供一个用户设置日志打印级别的接口
static inline void setRuntimeLogLevel(int level)
{
    getRuntimeLevel() = (level < GLOBAL_LOG_LEVEL) ? GLOBAL_LOG_LEVEL : level;
}

// 检查是否应该记录某级别日志
static inline bool shouldLog(int level)
{
    return level >= getRuntimeLevel();
}
}

#else // ENABLE_LOGGING 不开启时的空实现

#define LOG_TRACE(...)              ((void)0)
#define LOG_DEBUG(...)              ((void)0)
#define LOG_INFO(...)               ((void)0)
#define LOG_WARNING(...)            ((void)0)
#define LOG_ERROR(...)              ((void)0)
#define LOG_CRITICAL(...)           ((void)0)

namespace LogSystem {
    static inline void setRuntimeLogLevel(int) {}
    static inline int getRuntimeLevel() { return 0; }
}

#endif  // ENABLE_LOGGING

#endif  // LOGGER_DEFINE_