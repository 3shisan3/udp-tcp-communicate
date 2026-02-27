#include "communicate_api.h"

#include <iostream>
#include <chrono>
#include <csignal>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

using namespace communicate;

std::atomic<bool> running(true);

// 的测试处理器
class TestPeriodicHandler : public SubscribebBase
{
public:
    // 修改：增加len参数
    int handleMsg(std::shared_ptr<void> msg, size_t len) override
    {
        const char *data = static_cast<const char *>(msg.get());
        
        // 方法1：使用string的带长度构造函数（安全）
        std::string safe_str(data, len);
        
        std::cout << "[RECV] " << std::chrono::system_clock::now().time_since_epoch().count()
                  << " - " << safe_str << " (len=" << len << ")" << std::endl;
        return 0;
    }
};

class TestHandler : public SubscribebBase
{
public:
    // 修改：增加len参数
    int handleMsg(std::shared_ptr<void> msg, size_t len) override
    {
        // 假设接收的是字符串
        const char *data = static_cast<const char *>(msg.get());
        
        // 安全构造字符串
        std::string message(data, len);
        
        std::cout << "Received message: " << message << " (size=" << len << ")" << std::endl;
        return 0;
    }
};

void signalHandler(int signum)
{
    running = false;
}

int main()
{
    // 注册信号处理
    signal(SIGINT, signalHandler);

    // 初始化API
    if (Initialize("../test.yaml"))
    {
        std::cerr << "API初始化失败" << std::endl;
        return -1;
    }

    // 订阅接收处理
    if (Subscribe(new TestPeriodicHandler()))
    {
        std::cerr << "订阅失败" << std::endl;
        Destroy();
        return -1;
    }

    // 创建新消息
    std::string msg = "Hello, World!";
    
    // 监听由云端 127.0.0.1:6666 发出的消息
    if (SubscribeRemote("127.0.0.1", 6666, new TestHandler()))
    {
        std::cerr << "远程订阅失败" << std::endl;
        Destroy();
        return -1;
    }
    
    SetSendPort(6666);
    
    // 发送消息 - 注意：不需要+1
    if (::communicate::SendGeneralMessage("127.0.0.1", 1234, 
                                         (void*)msg.data(),  // 使用data()而不是c_str()
                                         msg.size()))       // 使用size()而不是size()+1
    {
        std::cerr << "发送失败" << std::endl;
        Destroy();
        return -1;
    }
    
    // 测试使用，临时改一下发送使用端口，为系统分配
    SetSendPort(0);

    // 创建周期发送任务
    int task_id = 233;
    int rate = 10; // 10Hz

    // 使用堆分配数据确保生命周期
    auto periodic_data = std::make_shared<std::string>("Periodic message");

    int ret = AddPeriodicSendTask("127.0.0.1", 3322, // 发送到3322端口
                                  (void*)periodic_data->data(),  // 使用data()
                                  periodic_data->size(),         // 发送端不用添加上终止符
                                  rate, task_id);
    if (ret != 0)
    {
        std::cerr << "创建周期任务失败，错误码: " << ret << std::endl;
        Destroy();
        return -1;
    }

    std::cout << "周期任务已启动（10Hz），10秒后停止..." << std::endl;
    std::cout << "注意：发送数据时不包含结束符，接收端使用长度参数安全处理" << std::endl;

    // 等待10秒
    for (int i = 0; i < 10 && running; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 删除周期任务
    if (RemovePeriodicSendTask(task_id) != 0)
    {
        std::cerr << "删除周期任务失败" << std::endl;
    } else {
        std::cout << "周期任务已停止" << std::endl;
    }

    // 再等待5秒查看是否有残留消息
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 销毁API
    if (Destroy() != 0) {
        std::cerr << "API销毁失败" << std::endl;
        return -1;
    }

    std::cout << "测试完成" << std::endl;
    return 0;
}