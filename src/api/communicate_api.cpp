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
    if(!communicateImp.send(addr, port, pData, size))
    {
        return -1;
    }
    return 0;
}

int addPeriodicSendTask(const char* addr, int port, void *pData, size_t size, int rate, int task_id)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    // 添加周期发送任务
    return communicateImp.addPeriodicSendTask(addr, port, pData, size, rate, task_id);
}

int removePeriodicSendTask(int task_id)
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

void setSendPort(int port)
{
    auto &communicateImp = SingletonTemplate<SocketWrapper>::getSingletonInstance().getCommunicateImp();
    communicateImp.setSendPort(port);
}

}   // namespace communicate