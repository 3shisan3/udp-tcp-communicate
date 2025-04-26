/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        communicate_interface.h
Version:     1.0
Author:      cjx
start date:
Description: 通信服务抽象类
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef COMMUNICATE_INTERFACE_H
#define COMMUNICATE_INTERFACE_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "communicate_api.h"

class CommunicateInterface
{
public:
    virtual ~CommunicateInterface() = default;

    /* **** 基础功能 **** */
    // 初始化
    virtual int initialize() = 0;
    // 发送消息
    virtual bool send(const std::string& dest_addr, int dest_port, const void* data, size_t size) = 0;
    // 增加监听ip和端口（本地）
    virtual int addListenAddr(const char* addr, int port) = 0;
    // 添加接收消息处理函数
    virtual int addSubscribe(const char* addr, int port, communicate::SubscribebBase *sub) = 0;
    // 销毁/停止
    virtual void shutdown() = 0;

    /* **** 高级功能接口（可选实现） **** */
    virtual bool sendAsync(const std::string &dest_addr, int dest_port, const void *data, size_t size)
    {
        // 默认实现为同步发送
        return send(dest_addr, dest_port, data, size);
    }
    // 周期发送固定数据
    virtual int addPeriodicSendTask(const char *addr, int port, const void *pData, size_t size, int rate, int task_id = -1)
    {
        return -1; // 默认不支持
    }
    virtual int removePeriodicTask(int task_id)
    {
        return -1; // 默认不支持
    }
    virtual void setSendPort(int port)
    {
        // 默认实现不设置发送端口
    }

    // 创建工厂函数
    template <typename T>
    static std::unique_ptr<CommunicateInterface> Create()
    {
        return std::make_unique<T>();
    }

protected:
    // 暂不使用功能
    virtual int addPeriodicTask(int interval_ms,
        const std::string &dest_addr,
        int dest_port,
        int appoint_task_id,
        std::function<std::vector<char>()> data_generator)
    {
        return -1; // 默认不支持
    }
};

#endif // COMMUNICATE_INTERFACE_H