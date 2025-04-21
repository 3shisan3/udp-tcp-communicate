/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        communicate_api.h
Version:     1.0
Author:      cjx
start date: 
Description: 封装好的对外使用接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef COMMUNICATE_API_H
#define COMMUNICATE_API_H

#include <memory>

namespace communicate
{

/* 消息基类，使用时继承重载其中消息处理函数进行解析 */
class SubscribebBase
{
public:
    virtual ~SubscribebBase() = default;

protected:
    /* @brief 处理接收到的数据 
       @param topic：消息的通道名
       @param data：收到的数据
       @return 
    */
    virtual int handleData(const char *topic, std::shared_ptr<void> data) = 0;

};

/**
 * @brief 根据配置文件初始化
 * @param cfgPath 配置文件路径
 * @return
 */
int Initialize(const char* cfgPath);

/**
 * @brief 销毁实例
 * @return
 */
int Destroy();

/**
 * @brief 向指定通道发送数据
 * @return
 */
int SendMessage(const char* topic, void* pData);

/**
 * @brief 订阅指定通道的数据
 * @return
 */
int Subscribe(const char* topic, SubscribebBase* pSubscribe);

/**
 * @brief 获取有效的通道名
 * @return
 */
// const std::vector<std::string>& GetAvailableTopics();

}


#endif  // COMMUNICATE_API_H