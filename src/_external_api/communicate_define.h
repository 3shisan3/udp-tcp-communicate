/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        communicate_define.h
Version:     1.0
Author:      cjx
start date:
Description: 涉及通用数据结构
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#pragma once

#include <memory>

namespace communicate
{

/* 消息基类，使用时继承重载其中消息处理函数进行解析 */
class SubscribebBase
{
public:
    virtual ~SubscribebBase() = default;

public:
    // 有自定义头时重载
    virtual int handleMessage(const char *topic, std::shared_ptr<void> message);

protected:
    /* @brief 处理接收到的数据 
       @param topic：消息的通道名
       @param data：收到的数据
       @return 
    */
    virtual int handleData(const char *topic, std::shared_ptr<void> data) = 0;

};


/* 当前库默认应用层扩展头（tcp，udp传输层差异内容不做扩展 */
#pragma pack(push, 1)       // 强制一字节对齐（消除成员间填充字节）
struct PacketHeader
{
    uint16_t seq;           // 序列号
    uint32_t checksum;      // CRC32校验
    uint64_t timestamp;     // 时间戳
    uint16_t payload_len;   // 数据长度（header + data）
};
#pragma pack(pop)


}   // communicate