#include "communicate_api.h"

#include <iostream>
#include <chrono>
#include <string>
#include <thread>

using namespace communicate;

// 打印接收到的内容
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

int main()
{
    // Initialize the API
    if (Initialize("../test.yaml"))
    {
        return -1; // Initialization failed
    }

    // Create a new message
    std::string msg = "Hello, World!";

    // Receive a message
    if (Subscribe("127.0.0.1", 2233, new TestHandler()))
    {
        return -1; // Subscribing failed
    }
    // Send the message
    if (SendMessage("127.0.0.1", 2233, &msg, sizeof(msg)))
    {
        return -1; // Sending failed
    }

    // subscribe to a message
    if (Subscribe(new TestHandler()))
    {
        return -1; // Subscribing failed
    }
    // send a periodic task
    int task_id = 233;
    int rate = 10; // 10 Hz
    std::string periodic_msg = "Periodic message";
    if (addPeriodicSendTask("localhost", 3322, &msg, sizeof(msg), rate, task_id))
    {
        return -1; // Adding periodic task failed
    }

    // 等待十秒钟
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 删除周期任务
    if (!removePeriodicSendTask(task_id))
    {
        return -1; // Removing periodic task failed
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Destroy the API
    if (!Destroy())
    {
        return -1; // Destruction failed
    }
    // 退出
    std::cout << "Exiting..." << std::endl;

    return 0;
}