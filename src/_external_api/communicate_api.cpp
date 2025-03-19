#include "communicate_api.h"

#include "common/socket_wrapper.h"
#include "utils/singleton.h"

namespace communicate
{

int SubscribebBase::handleMessage(const char *topic, std::shared_ptr<void> message)
{
    // 去除使用该库默认带的header


    return handleData(topic, message);
}


int Initialize(const char* cfgPath)
{

    return 0;
}

int Destroy()
{

    return 0;
}



}