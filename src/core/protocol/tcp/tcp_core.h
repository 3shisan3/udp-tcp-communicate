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
    bool send(const std::string& dest_addr, int dest_port, const void* data, size_t size) override;  
    int addListenAddr(const char* addr, int port) override;  
    int addSubscribe(const char* addr, int port, communicate::SubscribebBase *sub) override;  
    void shutdown() override;  
    void setSendPort(int port) override;  
  
protected:  
    // TCP配置结构体  
    struct CoreConfig  
    {  
        int recv_timeout_ms = 100;  
        int send_timeout_ms = 100;
        int connect_timeout_ms = 3000;
        int max_send_packet_size = 1024;
        int max_receive_packet_size = 65535;
        int source_port = 0;  
        int thread_pool_size = 3;  
        int max_connections = 100;  // TCP特有：最大连接数  
        int listen_backlog = 10;    // TCP特有：监听队列长度
        int keepalive_time = 60;
    } m_config; 
  
private:  
    class Impl;  
    std::unique_ptr<Impl> pimpl_;  
};  
  
#endif // TCP_CORE_H_