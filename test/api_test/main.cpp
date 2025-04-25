#include "communicate_api.h"

#include <iostream>
#include <chrono>
#include <csignal>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>

using namespace communicate;

std::atomic<bool> running(true);

// 改进的测试处理器
class TestPeriodicHandler : public SubscribebBase
{
public:
    int handleMsg(std::shared_ptr<void> msg) override
    {
        const char *data = static_cast<const char *>(msg.get());
        std::cout << "[RECV] " << std::chrono::system_clock::now().time_since_epoch().count()
                  << " - " << data << std::endl;
        return 0;
    }
};

class TestHandler : public SubscribebBase
{
public:
    int handleMsg(std::shared_ptr<void> msg) override
    {
        // Assuming msg is a string for simplicity
        std::string *message = static_cast<std::string *>(msg.get());
        std::cout << "Received message: " << *message << std::endl;
        return 0; // Success
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

    // Create a new message
    std::string msg = "Hello, World!";
    // Receive a message by port
    if (Subscribe("127.0.0.1", 6666, new TestHandler()))
    {
        return -1; // Subscribing failed
    }
    if (SendMessage("127.0.0.1", 2233, &msg, sizeof(msg)))
    {
        return -1; // Sending failed
    }

    // 测试使用，临时改一下发送使用端口，为系统分配
    setSendPort(0);

    // 创建周期发送任务
    int task_id = 233;
    int rate = 10; // 10Hz

    // 使用堆分配数据确保生命周期
    auto periodic_data = std::make_shared<std::string>("Periodic message");

    int ret = addPeriodicSendTask("127.0.0.1", 3322, // 发送到3322端口
                                  periodic_data->data(),
                                  periodic_data->size(),
                                  rate, task_id);
    if (ret != 0)
    {
        std::cerr << "创建周期任务失败，错误码: " << ret << std::endl;
        Destroy();
        return -1;
    }

    std::cout << "周期任务已启动，10秒后停止..." << std::endl;

    // 等待10秒
    for (int i = 0; i < 10 && running; ++i)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 删除周期任务
    if (removePeriodicSendTask(task_id))
    {
        std::cerr << "删除周期任务失败" << std::endl;
    } else {
        std::cout << "周期任务已停止" << std::endl;
    }

    // 再等待5秒查看是否有残留消息
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 销毁API
    if (!Destroy()) {
        std::cerr << "API销毁失败" << std::endl;
        return -1;
    }

    std::cout << "测试完成" << std::endl;
    return 0;
}