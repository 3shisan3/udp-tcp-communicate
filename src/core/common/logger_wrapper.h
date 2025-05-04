/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        logger_wrapper.h
Version:     1.0
Author:      cjx
start date:
Description: 提供统一接口，允许用户替换实现

Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1            2025-05-03       cjx         create

*****************************************************************/

#ifndef LOGGER_WRAPPER_H_
#define LOGGER_WRAPPER_H_

#include <atomic>
#include <memory>

#ifndef PROJECT_NAME
    #define PROJECT_NAME project
#endif

namespace PROJECT_NAME::logger
{

enum class LogLevel
{
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class LoggerWrapper
{
public:
    virtual ~LoggerWrapper() = default;

    template <typename... Args>
    void log(LogLevel level, Args &&...args)
    {
        log_impl(level, std::forward<Args>(args)...);
    }

private:
    virtual void log_impl(LogLevel level, ...) = 0;
};

// 设置全局日志器（线程安全，禁止重复初始化）
void set_logger(std::unique_ptr<LoggerWrapper> logger);

}

#endif  // LOGGER_WRAPPER_H_