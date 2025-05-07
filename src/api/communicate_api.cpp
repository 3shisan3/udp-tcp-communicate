#include "communicate_api.h"

#include "logger_define.h"
#include "common/config_wrapper.h"
#include "common/socket_wrapper.h"
#include "utils/singleton.h"
#include "utils/utils_ways.h"

namespace communicate
{

int Initialize(const char* cfgPath)
{
    // 统一设置日志格式​​（包括时间、项目名、日志级别等）
    const std::string LOG_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [" TO_STRING(PROJECT_NAME) "] [%^%l%$] [%t] [%s:%#] %v";
#ifdef LOGGING_SCHEME_SPDLOG
    ::spdlog::default_logger()->set_pattern(LOG_PATTERN);
    ::spdlog::default_logger()->set_level(static_cast<spdlog::level::level_enum>(GLOBAL_LOG_LEVEL));
#endif

    int ret = 0;
    // 初始化配置
    auto &cfg = SingletonTemplate<ConfigWrapper>::getSingletonInstance();
    ret = cfg.loadCfgFile(cfgPath);
    // 初始化内部日志配置
    if (!ret)
    {
        auto &cfgInstance = cfg.getCfgInstance();
        int runtimeLogLevel = cfgInstance.getValue("runtime_log_level", GLOBAL_LOG_LEVEL);
        ::PROJECT_NAME::logger::setRuntimeLogLevel(runtimeLogLevel);

        std::string outPath = cfgInstance.getValue("log_save_path", (std::string)"");
        if (outPath.empty() || !is_path_creatable(outPath))
        {
            LOG_CRITICAL("log will be output to the console.");
        }
        else if (create_dir_if_not_exists(outPath))
        {
            std::string outFileName = outPath + "/" + TO_STRING(PROJECT_NAME) + "_log.txt";
            LOG_CRITICAL("latest log will be output to the {}.", outFileName);

            // 创建旋转文件日志记录器 - 每个文件最大5MB，保留5个文件
#ifdef LOGGING_SCHEME_SPDLOG
            auto rotating_logger = ::spdlog::rotating_logger_mt("rotating_logger", outFileName, 1048576 * 5, 5);
            rotating_logger->set_pattern(LOG_PATTERN);
            rotating_logger->set_level(static_cast<spdlog::level::level_enum>(runtimeLogLevel));
            spdlog::set_default_logger(rotating_logger);
#endif
        }
        else
        {
            LOG_WARNING("the configured address: {} is invalid, the log will be output to the console.", outPath);
        }
    }
    // 初始化socket
    if (!ret)
    {
        ret = SingletonTemplate<SocketWrapper>::getSingletonInstance().initialize();
    }

    return ret;
}

int Destroy()
{
    SingletonTemplate<SocketWrapper>::getSingletonInstance().destroy();

#ifdef LOGGING_SCHEME_SPDLOG
    if (auto logger = spdlog::get("rotating_logger")) {
        logger->flush(); // 确保所有日志写入文件
    }
    spdlog::shutdown();  // 停止异步线程，释放所有记录器
#endif

    return 0;
}

int SendGeneralMessage(const char* addr, int port, void *pData, size_t size)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 发送数据
    if(!communicateImp.send(addr, port, pData, size))
    {
        return -1;
    }
    return 0;
}

int AddPeriodicSendTask(const char* addr, int port, void *pData, size_t size, int rate, int task_id)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 添加周期发送任务
    return communicateImp.addPeriodicSendTask(addr, port, pData, size, rate, task_id);
}

int RemovePeriodicSendTask(int task_id)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 删除周期发送任务
    return communicateImp.removePeriodicTask(task_id);
}

int Subscribe(SubscribebBase *pSubscribe)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    return communicateImp.addSubscribe(nullptr, 0, pSubscribe);
}

int SubscribeRemote(const char *addr, int port, SubscribebBase *pSubscribe)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 可以添加云端通用匹配前缀
    return communicateImp.addSubscribe(addr, port, pSubscribe);
}

int SubscribeLocal(const char *addr, int port, SubscribebBase *pSubscribe)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    int ret = 0;
    ret = communicateImp.addListenAddr(addr, port);
    if (ret == 0)
    {
        addr = addr ? "localhost" : addr;       // 设置本地通用匹配前缀为"localhost"
        ret = communicateImp.addSubscribe(addr, port, pSubscribe);
    }
    return ret;
}

int AddListener(const char *addr, int port)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    return communicateImp.addListenAddr(addr, port);
}

void SetSendPort(int port)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    communicateImp.setSendPort(port);
}

}   // namespace communicate