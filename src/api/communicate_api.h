/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        communicate_api.h
Version:     1.0
Author:      cjx
start date: 
Description: 基础的底层通信接口
    不包含序列化或加密内容，只关注数据的发送和接收
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef COMMUNICATE_API_H
#define COMMUNICATE_API_H

#include <memory>

namespace communicate
{

/* 消息抽象基类，使用时继承重载其中消息处理函数进行解析 */
class SubscribebBase
{
public:
    virtual ~SubscribebBase() = default;

public:
    /**
     *  @brief 处理接收到的数据
     *  @param topic   消息的通道名
     *  @param msg     收到的信息
     *  @return 错误码
     */
    virtual int handleMsg(std::shared_ptr<void> msg) = 0;
};

/**
 * @brief 根据配置文件初始化
 * @param cfgPath   配置文件路径
 * @return
 */
int Initialize(const char *cfgPath = nullptr);

/**
 * @brief 销毁实例
 * @return
 */
int Destroy();

/**
 * @brief 发送数据(默认向配置文件中所有配置的对象发送)
 * @param pData         发送的数据
 * @return
 */
int BroadcastMessage(void *pData, size_t size);

/**
 * @brief 发送数据
 * @param addr          发送的目标
 * @param pData         发送的数据
 * @return
 */
int SendGeneralMessage(const char *addr, int port, void *pData, size_t size);

/**
 * @brief 添加周期发送任务(更高级周期生成pData，暂不实现)
 * @param addr          发送的目标
 * @param pData         发送的数据
 * @param size          发送的数据大小
 * @param rate          发送的频率（HZ)
 * @param task_id       任务ID(主要用于删除任务)
 *                      -1表示不指定任务ID，系统自动分配
 * @return
 */
int AddPeriodicSendTask(const char *addr, int port, void *pData, size_t size, int rate, int task_id = -1);

/**
 * @brief 删除周期发送任务(添加时未指定，不支持删除)
 * @param task_id       任务ID
 * @return
 */
int RemovePeriodicSendTask(int task_id);

/**
 * @brief 订阅消息
 * @param pSubscribe    统一的接收消息的处理函数
 *                     （排除指定了的消息，所有订阅的消息都使用这个函数）
 * @return
 */
int Subscribe(SubscribebBase *pSubscribe);

/**
 * @brief 订阅消息
 *  优先使用指定发送端的处理函数->次选指定监听通道的处理函数->通用处理函数
 * @param addr          针对指定（发送方）来源的消息处理
 * @param port          端口号（发送方使用）
 * @param pSubscribe    接收消息的处理函数
 * @return
 */
int SubscribeRemote(const char *addr, int port, SubscribebBase *pSubscribe);

/**
 * @brief 订阅消息（未监听时，会增加监听）
 * @param addr          非多网卡且需特殊处理场景传空
 * @param port          端口号（本地监听的消息端口）
 * @param pSubscribe    接收消息的处理函数
 * @return
 */
int SubscribeLocal(const char *addr, int port, SubscribebBase *pSubscribe);

/**
 * @brief 添加本地监听端口（正常配置文件就设置好，特殊情况使用）
 * @param addr          本地网卡IP（默认传空即可）
 * @param port          端口号
 * @return
 */
int AddListener(const char *addr, int port);

// 设置发送使用的端口（非必要使用）
void SetSendPort(int port);

}


#endif  // COMMUNICATE_API_H