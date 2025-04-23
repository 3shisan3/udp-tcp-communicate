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


#include <atomic>
#include <chrono>
#include <string>
#include <vector>

class UdpEnhanced : public UdpCore
{
public:
    /// 数据处理模式
    enum class ProcessingMode
    {
        EventDriven, // 事件驱动（回调线程直接处理）
        ThreadPool   // 线程池模式
    };

    /// 周期任务结构
    struct PeriodicTask
    {
        std::string dest_addr;
        int dest_port;
        std::function<std::vector<char>()> data_generator;
        int interval_ms;
        std::atomic<bool> is_running{false};
        std::chrono::steady_clock::time_point last_send_time;
    };

    UdpEnhanced(ProcessingMode mode = ProcessingMode::ThreadPool,
                int thread_pool_size = 4);
    ~UdpEnhanced() override;

    // 实现高级接口
    bool sendAsync(const std::string &dest_addr, int dest_port,
                   const void *data, size_t size) override;

    int addPeriodicTask(
        int interval_ms,
        const std::string &dest_addr,
        int dest_port,
        std::function<std::vector<char>()> data_generator) override;

    bool removePeriodicTask(int task_id) override;

    // 扩展功能
    void addAddressHandler(
        const std::string &addr,
        std::function<void(const char *data, size_t size)> handler);

    void setProcessingMode(ProcessingMode mode);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // UDP_ENHANCED_H_