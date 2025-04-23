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

    return 0;
}



}