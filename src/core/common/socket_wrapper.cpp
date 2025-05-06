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
    LOG_INFO("Start initializing socket wrapper");
    auto &cfgInstance = SingletonTemplate<ConfigWrapper>::getSingletonInstance();
    // 获取配置文件描写的通讯协议
    std::string protocol = cfgInstance.getCfgInstance().getValue("protocol", DEFAULT_PROTOCOL);
    LOG_INFO("Protocol from config: %s", protocol.c_str());
    
    int ret = 0;
    if (protocol == "udp")
    {
        LOG_INFO("Creating UDP enhanced communication instance");
        m_communicateImp_ = CommunicateInterface::Create<UdpCommunicateEnhanced>();
        ret = m_communicateImp_->initialize();
        if (ret != 0)
            LOG_ERROR("Failed to initialize UDP communication, error code: %d", ret);
        else
            LOG_INFO("UDP communication initialized successfully");
    }
    else
    {
        // 其他协议的实现
        return -1;
    }

    return ret;
}


}