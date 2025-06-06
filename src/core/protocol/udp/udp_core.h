/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
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

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

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

#ifdef THREAD_POOL_MODE
#include "threadpool_wrapper.h"
#endif
#include "common/config_wrapper.h"

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
    bool send(const std::string &dest_addr, int dest_port, const void *data, size_t size) override;
    int addListenAddr(const char* addr, int port) override;
    int addSubscribe(const char *addr, int port, communicate::SubscribebBase *sub) override;
    void shutdown() override;

    // 修改发送使用的端口
    void setSendPort(int port) override;

protected:
    // 配置参数结构体
    struct CoreConfig
    {
        int recv_timeout_ms = 100;          // 接收超时(毫秒)
        int send_timeout_ms = 100;          // 发送超时(毫秒)
        int max_send_packet_size = 1024;    // 最大包大小
        int max_receive_packet_size = 65507;// 最大包大小（IP 层限制（65535 字节） - IP/UDP 头（28 字节）​​ ≈ ​​65507 字节）
        int source_port = 0;                // 发送源端口，0表示系统自动分配
        size_t thread_pool_size = 3;        // 线程池大小配置
    } m_config;

#ifdef THREAD_POOL_MODE
    static std::unique_ptr<ThreadPoolWrapper> s_thread_pool_;
#endif

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif // UDP_CORE_H_