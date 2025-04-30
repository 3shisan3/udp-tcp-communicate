/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        struct_impl.h
Version:     1.0
Author:      cjx
start date: 
Description: 内置默认的数据结构实现
Version history
[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2025-03-18      cjx        create

*****************************************************************/

#pragma once

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