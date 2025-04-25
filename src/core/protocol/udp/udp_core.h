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
    int receiveMessage(const char *addr, int port, communicate::SubscribebBase *sub) override;
    void shutdown() override;

    // 修改发送使用的端口
    void setSendPort(int port);

protected:
    // 配置参数结构体
    struct CoreConfig
    {
        int recv_timeout_ms = 100;  // 接收超时(毫秒)
        int send_timeout_ms = 100;  // 发送超时(毫秒)
        int max_packet_size = 1024; // 最大包大小
        int source_port = 0;        // 发送源端口，0表示系统自动分配
    } m_config;

    /* 拓展可实现 发向指定地址，或者指定类型的消息使用固定的端口
        std::unordered_map<std::string, int> port_mapping
        然后 sendData 函数中根据目的地址和端口进行查找
        动态管理同样，额外增加一个类故案例
    */

    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif // UDP_CORE_H_