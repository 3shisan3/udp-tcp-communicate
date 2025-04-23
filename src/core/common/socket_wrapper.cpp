#include "socket_wrapper.h"

#include "config_wrapper.h"
#include "utils/singleton.h"

namespace communicate
{
// 设置一些配置文件中没有时的默认值
const std::string DEFAULT_PROTOCOL = "udp";
const int MAX_DATA_LEN = 1024;


int SocketWrapper::initialize()
{
    auto &cfgInstance = SingletonTemplate<ConfigWrapper>::getSingletonInstance();
    // 获取配置文件描写的通讯协议
    std::string protocol = cfgInstance.getCfgInstance().getValue("protocol", DEFAULT_PROTOCOL);
    // 获取发送数据的最大长度
    int maxDataLen = cfgInstance.getCfgInstance().getValue("max_data_len", MAX_DATA_LEN);

    
    // 这里可以添加初始化代码
    // 例如，创建socket，绑定地址等
    return 0;
}


}