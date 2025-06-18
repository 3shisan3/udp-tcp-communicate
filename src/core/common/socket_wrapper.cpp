#include "socket_wrapper.h"

#include "config_wrapper.h"
#include "protocol/udp/udp_enhanced.h"
#include "protocol/tcp/tcp_core.h"
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
    LOG_INFO("Protocol from config: {}", protocol);
    
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
    else if (protocol == "tcp")
    {
        LOG_INFO("Creating TCP communication instance");
        m_communicateImp_ = CommunicateInterface::Create<TcpCommunicateCore>();
        ret = m_communicateImp_->initialize();
        if (ret != 0)
            LOG_ERROR("Failed to initialize TCP communication, error code: %d", ret);
        else
            LOG_INFO("TCP communication initialized successfully");
    }
    else
    {
        // 其他协议的实现
        return -1;
    }

    return ret;
}

void SocketWrapper::destroy()
{
    LOG_INFO("Destroying socket wrapper");

    if (m_communicateImp_)
    {
        LOG_DEBUG("Destroying communication implementation");
        m_communicateImp_->shutdown();
        m_communicateImp_.reset();
    }

    LOG_INFO("Socket wrapper destroyed successfully");
}


}