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