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
        task.second.running = false; // 通知所有任务停止
        if (task.second.thread.joinable())
        {
            task.second.thread.join(); // 等待任务线程结束
        }
    }
}

bool UdpCommunicateEnhanced::sendAsync(const std::string &dest_addr,
                                       int dest_port,
                                       const void *data,
                                       size_t size)
{
    // 创建线程安全的数据副本
    std::shared_ptr<std::vector<char>> buffer = std::make_shared<std::vector<char>>(size);
    memcpy(buffer->data(), data, size);

    // 启动异步发送线程
    std::thread([this, dest_addr, dest_port, buffer]() {
        try {
            this->send(dest_addr, dest_port, buffer->data(), buffer->size());
        } catch (...) {
            // 记录发送失败日志（后续拓展日志模块添加）
        }
    }).detach();

    return true;
}

int UdpCommunicateEnhanced::addPeriodicTask(
    int interval_ms,
    const std::string &dest_addr,
    int dest_port,
    int appoint_task_id,
    std::function<std::vector<char>()> data_generator)
{
    // 参数有效性检查
    if (interval_ms <= 0)
    {
        return -1; // ERR_INVALID_INTERVAL
    }
    if (dest_port <= 0 || dest_port > 65535)
    {
        return -2; // ERR_INVALID_PORT
    }
    if (dest_addr.empty())
    {
        return -3; // ERR_INVALID_ADDRESS
    }
    if (!data_generator)
    {
        return -4; // ERR_INVALID_GENERATOR
    }

    std::lock_guard<std::mutex> lock(task_mutex_);

    // 处理指定的任务ID
    int task_id = next_task_id_++;
    if (appoint_task_id != -1)
    {
        if (task_map_.count(appoint_task_id))
        {
            return -5; // ERR_DUPLICATE_TASK_ID
        }
        task_map_[appoint_task_id] = task_id;
    }

    // 预创建任务条目（确保线程安全）
    auto [it, inserted] = periodic_tasks_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(task_id),
        std::forward_as_tuple(true, std::thread()));

    if (!inserted)
    {
        return -6; // ERR_TASK_CREATION_FAILED
    }

    // 启动任务线程
    try
    {
        it->second.thread = std::thread([this, task_id, interval_ms,
                                         dest_addr, dest_port,
                                         data_generator]() {
            // 安全获取任务对象
            auto* task = this->getTask(task_id);
            if (!task || !task->running.load())
                return;

            while (task->running.load())
            {
                auto start = std::chrono::steady_clock::now();
                
                try {
                    // 生成并发送数据
                    auto data = data_generator();
                    if (!data.empty()) {
                        this->send(dest_addr, dest_port, data.data(), data.size());
                    }
                } catch (const std::exception& e) {
                    // 记录生成器异常（实际项目应添加日志）
                    break;
                }
                
                // 精确控制发送间隔
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                auto sleep_time = std::chrono::milliseconds(interval_ms) - elapsed;

                if (sleep_time.count() > 0) {
                    std::this_thread::sleep_for(sleep_time);
                } else {
                    // 发送超时警告（实际项目应添加日志）
                }
            }
        });
    }
    catch (...)
    {
        periodic_tasks_.erase(it); // 清理失败的任务条目
        return -7;                 // ERR_THREAD_CREATION_FAILED
    }

    return 0;
}

int UdpCommunicateEnhanced::removePeriodicTask(int appoint_task_id)
{
    std::lock_guard<std::mutex> lock(task_mutex_);

    // 查找映射的任务ID
    auto map_it = task_map_.find(appoint_task_id);
    if (map_it == task_map_.end())
    {
        return -1; // ERR_TASK_NOT_FOUND
    }

    int task_id = map_it->second;
    task_map_.erase(map_it); // 清理映射表

    // 查找并停止任务
    auto task_it = periodic_tasks_.find(task_id);
    if (task_it == periodic_tasks_.end())
    {
        return -1; // ERR_TASK_NOT_FOUND
    }

    task_it->second.running = false;
    if (task_it->second.thread.joinable())
    {
        task_it->second.thread.join();
    }
    periodic_tasks_.erase(task_it);

    return 0; // SUCCESS
}

int UdpCommunicateEnhanced::addPeriodicSendTask(const char *addr, int port,
                                                const void *pData, size_t size,
                                                int rate, int task_id)
{
    // 参数校验
    if (rate <= 0 || rate > 1000)
    {
        return -1; // ERR_INVALID_RATE
    }
    if (!pData || size == 0)
    {
        return -2; // ERR_INVALID_DATA
    }

    // 创建数据副本确保生命周期安全
    std::shared_ptr<std::vector<char>> dataCopy =
        std::make_shared<std::vector<char>>(size);
    memcpy(dataCopy->data(), pData, size);

    // 计算间隔时间
    int interval_ms = 1000 / rate;

    // 创建生成器lambda
    auto generator = [dataCopy]() -> std::vector<char>
    {
        return *dataCopy; // 返回数据副本
    };

    return addPeriodicTask(interval_ms, addr, port, task_id, generator);
}

int UdpCommunicateEnhanced::Subscribe(const char *addr, int port,
                                      communicate::SubscribebBase *pSubscribe)
{
    return receiveMessage(const_cast<char *>(addr), port, pSubscribe);
}

// 私有辅助函数实现
UdpCommunicateEnhanced::PeriodicTask *UdpCommunicateEnhanced::getTask(int task_id)
{
    std::lock_guard<std::mutex> lock(task_mutex_);
    auto it = periodic_tasks_.find(task_id);
    return (it != periodic_tasks_.end()) ? &it->second : nullptr;
}