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

#include "communicate_define.h"

namespace communicate
{

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