#include "udp_enhanced.h"

#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

UdpCommunicateEnhanced::UdpCommunicateEnhanced() {}
UdpCommunicateEnhanced::~UdpCommunicateEnhanced()
{
    std::lock_guard<std::mutex> lock(task_mutex_);
    for (auto &task : periodic_tasks_)
    {
        task.second.running = false;
        if (task.second.thread.joinable())
        {
            task.second.thread.join();
        }
    }
}

bool UdpCommunicateEnhanced::sendAsync(const std::string &dest_addr,
                                       int dest_port,
                                       const void *data,
                                       size_t size)
{
    std::thread([this, dest_addr, dest_port, data, size]()
                {
        std::vector<char> buffer(size);
        memcpy(buffer.data(), data, size);
        this->send(dest_addr, dest_port, buffer.data(), buffer.size()); })
        .detach();
    return true;
}

int UdpCommunicateEnhanced::addPeriodicTask(
    int interval_ms,
    const std::string &dest_addr,
    int dest_port,
    std::function<std::vector<char>()> data_generator)
{

    std::lock_guard<std::mutex> lock(task_mutex_);

    int task_id = next_task_id_++;
    periodic_tasks_.emplace(
        task_id,
        PeriodicTask{
            true,
            std::thread([this, task_id, interval_ms, dest_addr, dest_port, data_generator]()
            {
                auto& task = this->periodic_tasks_[task_id];
                while (task.running.load())
                {
                    auto start = std::chrono::steady_clock::now();
                    
                    auto data = data_generator();
                    this->send(dest_addr, dest_port, data.data(), data.size());
                    
                    auto end = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                    auto sleep_time = std::chrono::milliseconds(interval_ms) - elapsed;

                    if (sleep_time.count() > 0)
                    {
                        std::this_thread::sleep_for(sleep_time);
                    }
                }
            })
        });

    return task_id;
}

int UdpCommunicateEnhanced::removePeriodicTask(int task_id)
{
    std::lock_guard<std::mutex> lock(task_mutex_);

    if (auto it = periodic_tasks_.find(task_id); it != periodic_tasks_.end())
    {
        it->second.running = false;
        if (it->second.thread.joinable())
        {
            it->second.thread.join();
        }
        periodic_tasks_.erase(it);
        return 0;
    }
    return -1;
}

int UdpCommunicateEnhanced::addPeriodicSendTask(const char *addr, int port, void *pData, size_t size, int rate)
{
    int interval_ms = 1000 / rate;

    return addPeriodicTask(interval_ms, addr, port, [pData, size]()
                           {
        std::vector<char> buffer(size);
        memcpy(buffer.data(), pData, size);
        return buffer; });
}

int UdpCommunicateEnhanced::Subscribe(const char *addr, int port, communicate::SubscribebBase *pSubscribe)
{
    return receiveMessage(const_cast<char *>(addr), port, pSubscribe);
}