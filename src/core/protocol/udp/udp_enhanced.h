/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        udp_enhanced.h
Version:     1.1
Author:      cjx
start date:
Description: 在UdpCore基础上添加：
            1. 周期任务管理（线程安全改进版）
            2. 异步发送（生命周期安全版）
            3. 地址过滤
            4. 多线程处理模式（增强健壮性）
Version history
[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
[1.1]    |   [2023-08-20]  |   [cjx]   |   [增强线程安全和异常处理]
*****************************************************************/

#ifndef UDP_ENHANCED_H_
#define UDP_ENHANCED_H_

#include "udp_core.h"

#include <atomic>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

class UdpCommunicateEnhanced : public UdpCommunicateCore
{
public:
    UdpCommunicateEnhanced();
    ~UdpCommunicateEnhanced() override;

    // 异步发送接口（线程安全）
    bool sendAsync(const std::string &dest_addr, int dest_port,
                   const void *data, size_t size) override;

    // 增强版周期任务接口（带完整错误处理）
    int addPeriodicTask(int interval_ms,
                        const std::string &dest_addr,
                        int dest_port,
                        int appoint_task_id = -1, // 默认-1表示自动分配ID
                        std::function<std::vector<char>()> data_generator = nullptr) override;

    // 安全删除周期任务（自动清理资源）
    int removePeriodicTask(int task_id) override;

    // 安全周期发送任务（数据生命周期保障）
    int addPeriodicSendTask(const char *addr, int port, const void *pData, size_t size, int rate, int task_id = -1) override;

private:
    // 周期任务结构体（线程安全设计）
    struct PeriodicTask
    {
        std::atomic<bool> running{false}; // 原子操作标志位
        std::thread thread;               // 任务线程

        PeriodicTask() = default;

        // 移动构造（确保线程对象安全转移）
        PeriodicTask(PeriodicTask &&other) noexcept
            : running(other.running.load()),
              thread(std::move(other.thread)) {}

        PeriodicTask(bool running, std::thread &&t)
            : running(running), thread(std::move(t)) {}
    };

    // 内部获取任务（带锁保护）
    PeriodicTask *getTask(int task_id);

    std::atomic<int> next_task_id_{1};                     // 原子任务ID计数器
    std::mutex task_mutex_;                                // 任务管理锁
    std::map<int, int> task_map_;                          // 外部ID到内部ID映射
    std::unordered_map<int, PeriodicTask> periodic_tasks_; // 任务存储
};

#endif // UDP_ENHANCED_H_