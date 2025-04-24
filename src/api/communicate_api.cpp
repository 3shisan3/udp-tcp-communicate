#include "communicate_api.h"

#include "common/config_wrapper.h"
#include "common/socket_wrapper.h"
#include "utils/singleton.h"

namespace communicate
{

int Initialize(const char* cfgPath)
{
    int ret = 0;
    // 初始化配置
    ret = SingletonTemplate<ConfigWrapper>::getSingletonInstance().loadCfgFile(cfgPath);
    // 初始化socket
    if (!ret)
    {
        ret = SingletonTemplate<SocketWrapper>::getSingletonInstance().initialize();
    }

    return ret;
}

int Destroy()
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    communicateImp.shutdown();
    return 0;
}

int SendMessage(const char* addr, int port, void *pData, size_t size)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 发送数据
    return communicateImp.send(addr, port, pData, size);
}

int addPeriodicSendTask(const char* addr, int port, void *pData, size_t size, int rate, int task_id)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 添加周期发送任务
    return communicateImp.addPeriodicSendTask(addr, port, pData, size, rate, task_id);
}

int removePeriodicTask(int task_id)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 删除周期发送任务
    return communicateImp.removePeriodicTask(task_id);
}

int Subscribe(SubscribebBase *pSubscribe)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 订阅消息
    return communicateImp.receiveMessage(nullptr, 0, pSubscribe);
}

int Subscribe(const char* addr, int port, SubscribebBase *pSubscribe)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 订阅消息
    return communicateImp.receiveMessage(addr, port, pSubscribe);
}

}   // namespace communicate