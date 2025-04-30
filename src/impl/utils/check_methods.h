/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT 
File:        check_methods.h
Version:     1.0
Author:      cjx
start date: 
Description: 自定义结构配套的相关校验方法
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2025-03-18      cjx        create

*****************************************************************/

#pragma once

#include <arpa/inet.h>
#include <zlib.h>

#include "struct_impl.h"

namespace communicate
{

uint32_t calculate_checksum(const void *data, size_t len)
{
    return htonl(crc32(0L, reinterpret_cast<const Bytef *>(data), len));
}

bool validate_packet(const PacketHeader &header, const char *payload)
{
    // 校验头部
    uint32_t received_checksum = ntohl(header.checksum);
    uint32_t actual_checksum = calculate_checksum(&header, sizeof(PacketHeader) - sizeof(uint32_t));

    // 校验载荷（若存在）
    if (payload && ntohs(header.payload_len) > 0)
    {
        actual_checksum = crc32(actual_checksum,
                                reinterpret_cast<const Bytef *>(payload),
                                ntohs(header.payload_len));
    }

    return (received_checksum == actual_checksum);
}


}   // communicate