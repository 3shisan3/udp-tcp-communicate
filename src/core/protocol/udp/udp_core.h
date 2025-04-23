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
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

// 平台相关定义
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
    typedef SOCKET SocketType;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
    typedef int SocketType;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

/**
 * @brief UDP核心通信类
 */
class UdpCommunicateCore : public CommunicateInterface
{
public:
    UdpCommunicateCore();
    virtual ~UdpCommunicateCore();

    // 禁止拷贝
    UdpCommunicateCore(const UdpCommunicateCore &) = delete;
    UdpCommunicateCore &operator=(const UdpCommunicateCore &) = delete;

    int initialize() override;
    bool send(const std::string &dest_addr, int dest_port,
              const void *data, size_t size) override;
    int receiveMessage(char *addr, int port, communicate::SubscribebBase *sub) override;
    void shutdown() override;

protected:
    // 配置参数结构体
    struct CoreConfig
    {
        int recv_timeout_ms = 100;  // 接收超时(毫秒)
        int send_timeout_ms = 100;  // 发送超时(毫秒)
        int max_packet_size = 1024; // 最大包大小
    } m_config;

    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif // UDP_CORE_H_