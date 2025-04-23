/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        udp_core.h
Version:     1.0
Author:      cjx
start date:
Description: 实现最基本的UDP通信功能
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef UDP_CORE_H_
#define UDP_CORE_H_

#include "../communicate_interface.h"

#include <memory>

class UdpCore : public CommunicateInterface
{
public:
    UdpCore();
    ~UdpCore() override;

    // 实现基类接口
    int initialize() override;
    void shutdown() override;
    bool send(const std::string &dest_addr, int dest_port,
              const void *data, size_t size) override;
    int receiveMessage(char* addr, int port, SubscribebBase *sub) override;

protected:
    // 配置参数结构体
    struct CoreConfig
    {
        int recv_timeout_ms = 100;  // 接收超时(毫秒)
        int send_timeout_ms = 100;  // 发送超时(毫秒)
        int max_packet_size = 1024; // 最大包大小
    } m_config;

private:
    // PIMPL模式隐藏实现细节
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // UDP_CORE_H_