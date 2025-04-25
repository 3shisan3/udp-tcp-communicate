#include "socket_wrapper.h"

#include "config_wrapper.h"
#include "protocol/udp/udp_enhanced.h"
#include "utils/singleton.h"

namespace communicate
{
// 设置一些配置文件中没有时的默认值
const std::string DEFAULT_PROTOCOL = "udp";

int SocketWrapper::initialize()
{
    auto &cfgInstance = SingletonTemplate<ConfigWrapper>::getSingletonInstance();
    // 获取配置文件描写的通讯协议
    std::string protocol = cfgInstance.getCfgInstance().getValue("protocol", DEFAULT_PROTOCOL);
    
    int ret = 0;
    if (protocol == "udp")
    {
        m_communicateImp_ = CommunicateInterface::Create<UdpCommunicateEnhanced>();
        ret = m_communicateImp_->initialize();
    }
    else
    {
        // 其他协议的实现
        return -1;
    }

    return ret;
}


}