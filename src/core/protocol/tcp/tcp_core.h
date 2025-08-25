/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        tcp_core.h
Version:     1.0
Author:      cjx
start date:
Description: 实现最基本的TCP通信功能
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef TCP_CORE_H_  
#define TCP_CORE_H_  
  
#include "../communicate_interface.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector> 

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
#include <netinet/tcp.h>  // TCP协议相关头文件
    typedef int SocketType;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#ifdef THREAD_POOL_MODE
#include "threadpool_wrapper.h"
#endif
#include "common/config_wrapper.h"
  
class TcpCommunicateCore : public CommunicateInterface  
{  
public:  
    TcpCommunicateCore();
    virtual ~TcpCommunicateCore();

    TcpCommunicateCore(const TcpCommunicateCore &) = delete;
    TcpCommunicateCore &operator=(const TcpCommunicateCore &) = delete;
  
    // 基础功能实现  
    int initialize() override;
    // 发送会优先使用已经建立连接的源，后文 setDefSource 不会影响
    bool send(const std::string& dest_addr, int dest_port, const void* data, size_t size) override;  
    int addListenAddr(const char* addr, int port) override;  
    int addSubscribe(const char* addr, int port, communicate::SubscribebBase *sub) override;  
    void shutdown() override;
    // 建立连接优先使用的本地源地址
    void setDefSource(int port, std::string ip = "") override;
  
protected:  
    // TCP配置结构体  
    struct CoreConfig  
    {  
        int recv_timeout_ms = 100;  
        int send_timeout_ms = 100;
        int connect_timeout_ms = 3000;  // TCP特有：连接建立超时（三次握手最长等待时间）
        int max_send_packet_size = 1460;// 单个数据包最大大小（以太网MTU 1500 - TCP/IP头40
        LocalSourceAddr source_addr;    // 建立连接优先使用本地地址，port 0为端口系统自动分配 ip 空为默认网卡
        int thread_pool_size = 3;  
        int max_connections = 100;      // TCP特有：最大并发连接数（防资源耗尽）
        int listen_backlog = 10;        // TCP特有：监听队列长度
        int keepalive_time = 60;        // 保活机制，设置 0 为不启用保活机制
    } m_config;

#ifdef THREAD_POOL_MODE
    static std::unique_ptr<ThreadPoolWrapper> s_thread_pool_;
#endif
  
private:  
    class Impl;  
    std::unique_ptr<Impl> pimpl_;  
};  
  
#endif // TCP_CORE_H_