/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        serialize_wrapper.h
Version:     1.0
Author:      cjx
start date:
Description: 序列化方法继承的基类
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef SERIALIZE_WRAPPER_H_
#define SERIALIZE_WRAPPER_H_



class SerializeWrapper
{
public:
    virtual ~SerializeWrapper() = default;

    // 序列化
    virtual bool serialize() = 0;
    // 反序列化
    virtual bool deserialize() = 0;

};

#endif // SERIALIZE_WRAPPER_H_