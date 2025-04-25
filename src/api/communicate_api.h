/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
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
int Initialize(const char *cfgPath);

/**
 * @brief 销毁实例
 * @return
 */
int Destroy();

/**
 * @brief 发送数据
 * @param addr          发送的目标
 * @param pData         发送的数据
 * @return
 */
int SendMessage(const char *addr, int port, void *pData, size_t size);

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
int addPeriodicSendTask(const char *addr, int port, void *pData, size_t size, int rate, int task_id = -1);

/**
 * @brief 删除周期发送任务(添加时未指定，不支持删除)
 * @param task_id       任务ID
 * @return
 */
int removePeriodicSendTask(int task_id);

/**
 * @brief 订阅消息
 * @param pSubscribe    接收消息的处理函数
 * @return
 */
int Subscribe(SubscribebBase *pSubscribe);

/**
 * @brief 订阅消息
 * @param addr          针对指定来源的消息处理
 * @param pSubscribe    接收消息的处理函数
 * @return
 */
int Subscribe(const char *addr, int port, SubscribebBase *pSubscribe);

// 设置发送使用的端口（非必要使用）
void setSendPort(int port);

}


#endif  // COMMUNICATE_API_H