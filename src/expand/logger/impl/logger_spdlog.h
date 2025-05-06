/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        looger_spdlog.h
Version:     1.0
Author:      cjx
start date:
Description: 使用spdlog打印时的内容封装
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1            2025-05-03       cjx         create

*****************************************************************/
#pragma once

#include "logger_define.h"

// 包含spdlog头文件前，先定义该宏
#define SPDLOG_ACTIVE_LEVEL GLOBAL_LOG_LEVEL

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h> 

// 定义所有日志级别的宏
#define LOG_TRACE(...) \
    do { \
        if constexpr (LOG_LEVEL_TRACE >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_TRACE)) { \
                SPDLOG_TRACE(__VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_DEBUG(...) \
    do { \
        if constexpr (LOG_LEVEL_DEBUG >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_DEBUG)) { \
                SPDLOG_DEBUG(__VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_INFO(...) \
    do { \
        if constexpr (LOG_LEVEL_INFO >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_INFO)) { \
                SPDLOG_INFO(__VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_WARNING(...) \
    do { \
        if constexpr (LOG_LEVEL_WARNING >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_WARNING)) { \
                SPDLOG_WARN(__VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_ERROR(...) \
    do { \
        if constexpr (LOG_LEVEL_ERROR >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_ERROR)) { \
                SPDLOG_ERROR(__VA_ARGS__); \
            } \
        } \
    } while (false)

#define LOG_CRITICAL(...) \
    do { \
        if constexpr (LOG_LEVEL_CRITICAL >= GLOBAL_LOG_LEVEL) { \
            if (PROJECT_NAME::logger::shouldLog(LOG_LEVEL_CRITICAL)) { \
                SPDLOG_CRITICAL(__VA_ARGS__); \
            } \
        } \
    } while (false)

