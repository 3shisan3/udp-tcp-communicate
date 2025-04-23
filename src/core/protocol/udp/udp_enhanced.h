/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        udp_enhanced.h
Version:     1.0
Author:      cjx
start date:
Description: 在UdpCore基础上添加：
            1. 周期任务管理
            2. 异步发送
            3. 地址过滤
            4. 多线程处理模式
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef UDP_ENHANCED_H_
#define UDP_ENHANCED_H_

#include "udp_core.h"

#include <functional>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstring>

class UdpCommunicateEnhanced : public UdpCommunicateCore
{
public:
    UdpCommunicateEnhanced();
    ~UdpCommunicateEnhanced() override;

    bool sendAsync(const std::string &dest_addr, int dest_port,
                   const void *data, size_t size) override;
    int addPeriodicTask(int interval_ms,
                        const std::string &dest_addr,
                        int dest_port,
                        std::function<std::vector<char>()> data_generator) override;
    int removePeriodicTask(int task_id) override;

    int addPeriodicSendTask(const char *addr, int port, void *pData, size_t size, int rate);
    int Subscribe(const char *addr, int port, communicate::SubscribebBase *pSubscribe);

private:
    struct PeriodicTask
    {
        std::atomic<bool> running{false};
        std::thread thread;

        PeriodicTask() = default;

        PeriodicTask(PeriodicTask &&other) noexcept
            : running(other.running.load()),
              thread(std::move(other.thread)) {}

        PeriodicTask(bool running, std::thread &&t)
            : running(running), thread(std::move(t)) {}
    };

    std::atomic<int> next_task_id_{0};
    std::mutex task_mutex_;
    std::unordered_map<int, PeriodicTask> periodic_tasks_;
};

#endif // UDP_ENHANCED_H_