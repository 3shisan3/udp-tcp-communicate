#include "logger_wrapper.h"

#include <iostream>

// 宏字符串化工具
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

namespace PROJECT_NAME::logger
{
namespace
{
    std::unique_ptr<LoggerWrapper> g_logger;
    std::atomic<bool> g_initialized = false;
}   // 匿名空间

// C++11参数展开工具
namespace detail
{
    // 递归终止条件
    void print_args(std::ostream &) {}
    // 递归展开
    template <typename T, typename... Args>
    void print_args(std::ostream &os, T &&arg, Args &&...rest)
    {
        os << std::forward<T>(arg);
        print_args(os, std::forward<Args>(rest)...);
    }
}

// 默认日志器（输出到 stderr）
class StderrLogger : public LoggerWrapper
{
private:
    // 实际处理日志的内部实现
    template <typename... Args>
    void log_impl(LogLevel level, Args&&... args)
    {
        static const char *level_names[] = {"TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};

        std::cerr << "[" << TO_STRING(PROJECT_NAME) << ":" << level_names[static_cast<int>(level)] << "] ";

// C++17 折叠表达式（C++11 需递归展开）
#if __cplusplus >= 201703L
            (std::cerr << ... << args) << std::endl;
#else
            print_args(std::cerr, std::forward<Args>(args)...);
#endif
    }

    // 必须提供的虚函数实现（即使不直接使用）
    void log_impl(LogLevel level, ...) override {}
};

/*  C11 expander 技巧（更简洁）
***
using expander = int[];
(void)expander{0, (void(std::cerr << std::forward<Args>(args)), 0)...};
std::cerr << std::endl;
***
*/

void set_logger(std::unique_ptr<LoggerWrapper> logger)
{
    if (g_initialized.exchange(true))
    {
        std::cerr << "Warning: Logger already initialized!" << std::endl;
        return;
    }
    g_logger = std::move(logger);
}

} //