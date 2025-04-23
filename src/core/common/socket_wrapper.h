/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        socket_wrapper.h
Version:     1.0
Author:      cjx
start date:
Description: 协议继承的基类
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]

*****************************************************************/

#ifndef SOCKET_WRAPPER_H_
#define SOCKET_WRAPPER_H_

#include <map>
#include <memory>
#include <string>


#include "utils/singleton.h"

namespace communicate
{

class SocketWrapper
{
public:
    friend SingletonTemplate<SocketWrapper>;

    int initialize();


protected:
    // std::map<std::string, SubscribebBase *> m_addrDealFunc;

private:
    // std::shared_ptr<SocketWrapper> m_communicateImp_;
    
};


}   // communicate

#endif // SOCKET_WRAPPER_H_