#include "udp_enhanced.h"

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

UdpCommunicateEnhanced::UdpCommunicateEnhanced()
{
    LOG_TRACE("UdpCommunicateEnhanced constructor");
}

UdpCommunicateEnhanced::~UdpCommunicateEnhanced()
{
    LOG_INFO("Shutting down UdpCommunicateEnhanced and stopping all periodic tasks");
    std::lock_guard<std::mutex> lock(task_mutex_);
    for (auto &task : periodic_tasks_)
    {
        LOG_DEBUG("Stopping periodic task {}", task.first);
        task.second.running = false; // 通知所有任务停止
        if (task.second.thread.joinable())
        {
            task.second.thread.join(); // 等待任务线程结束
            LOG_DEBUG("Periodic task {} stopped", task.first);
        }
    }
    LOG_INFO("All periodic tasks stopped");
}

std::future<bool> UdpCommunicateEnhanced::sendAsync(const std::string &dest_addr,
                                       int dest_port,
                                       const void *data,
                                       size_t size)
{
    LOG_DEBUG("Starting async send to {}:{} (size: {})", dest_addr, dest_port, size);

    // 创建线程安全的数据副本
    std::shared_ptr<std::vector<char>> buffer = std::make_shared<std::vector<char>>(size);
    memcpy(buffer->data(), data, size);
    auto promise_ptr = std::make_shared<std::promise<bool>>();

    auto send_task = [this, dest_addr, dest_port, buffer, promise_ptr]()
    {
        try
        {
            LOG_TRACE("Async send thread started for {}:{}", dest_addr, dest_port);
            bool result = this->send(dest_addr, dest_port, buffer->data(), buffer->size());
            promise_ptr->set_value(result);
            LOG_DEBUG("Async send to {}:{} completed", dest_addr, dest_port);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Async send to {}:{} failed: {}", dest_addr, dest_port, e.what());
            promise_ptr->set_value(false);  // 不进行promise_ptr->set_exception，分析日志
        }
        catch (...)
        {
            LOG_ERROR("Async send to {}:{} failed with unknown exception", dest_addr, dest_port);
            promise_ptr->set_value(false);
        }
    };

    // 避免竞争条件
    auto future = promise_ptr->get_future();

#ifdef THREAD_POOL_MODE
    s_thread_pool_->enqueue(std::move(send_task));
#else
    std::thread(std::move(send_task)).detach();
#endif

    return future;
}

int UdpCommunicateEnhanced::addPeriodicTask(
    int interval_ms,
    const std::string &dest_addr,
    int dest_port,
    int appoint_task_id,
    std::function<std::vector<char>()> data_generator)
{
    LOG_DEBUG("Adding periodic task to {}:{} with interval {}ms (requested ID: {})", dest_addr, dest_port, interval_ms, appoint_task_id);

    // 参数有效性检查
    if (interval_ms <= 0)
    {
        LOG_ERROR("Invalid interval {}ms for periodic task", interval_ms);
        return -1; // ERR_INVALID_INTERVAL
    }
    if (dest_port <= 0 || dest_port > 65535)
    {
        LOG_ERROR("Invalid port {} for periodic task", dest_port);
        return -2; // ERR_INVALID_PORT
    }
    if (dest_addr.empty())
    {
        LOG_ERROR("Empty destination address for periodic task");
        return -3; // ERR_INVALID_ADDRESS
    }
    if (!data_generator)
    {
        LOG_ERROR("Invalid data generator for periodic task");
        return -4; // ERR_INVALID_GENERATOR
    }

    std::lock_guard<std::mutex> lock(task_mutex_);

    // 处理指定的任务ID
    int task_id = next_task_id_++;
    if (appoint_task_id != -1)
    {
        if (task_map_.count(appoint_task_id))
        {
            LOG_ERROR("Duplicate task ID {} requested", appoint_task_id);
            return -5; // ERR_DUPLICATE_TASK_ID
        }
        task_map_[appoint_task_id] = task_id;
        LOG_DEBUG("Mapped requested ID {} to internal ID {}", appoint_task_id, task_id);
    }

    // 预创建任务条目（确保线程安全）
    auto [it, inserted] = periodic_tasks_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(task_id),
        std::forward_as_tuple(true, std::thread()));

    if (!inserted)
    {
        LOG_ERROR("Failed to create periodic task entry for ID {}", task_id);
        return -6; // ERR_TASK_CREATION_FAILED
    }

    // 启动任务线程
    try
    {
        it->second.thread = std::thread([this, task_id, interval_ms,
                                         dest_addr, dest_port,
                                         data_generator]() {
            LOG_DEBUG("Periodic task {} thread started ({}:{} every {}ms)", task_id, dest_addr, dest_port, interval_ms);

            // 安全获取任务对象
            auto* task = this->getTask(task_id);
            if (!task || !task->running.load())
            {
                LOG_WARNING("Periodic task {} terminated immediately", task_id);
                return;
            }

            while (task->running.load())
            {
                auto start = std::chrono::steady_clock::now();
                
                try
                {
                    // 生成并发送数据
                    auto data = data_generator();
                    if (!data.empty())
                    {
                        LOG_TRACE("Periodic task {} generating data (size: {})", task_id, data.size());
                        this->send(dest_addr, dest_port, data.data(), data.size());
                        LOG_TRACE("Periodic task {} sent data", task_id);
                    }
                    else
                    {
                        LOG_WARNING("Periodic task {} generated empty data", task_id);
                    }
                }
                catch (const std::exception& e)
                {
                    // 记录生成器异常
                    LOG_ERROR("Periodic task {} data generator failed: {}", task_id, e.what());
                    break;
                }
                
                // 精确控制发送间隔
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                auto sleep_time = std::chrono::milliseconds(interval_ms) - elapsed;

                if (sleep_time.count() > 0)
                {
                    std::this_thread::sleep_for(sleep_time);
                }
                else
                {
                    // 发送超时警告
                    LOG_WARNING("Periodic task {} execution took longer than interval ({}ms > {}ms)", task_id, elapsed.count(), interval_ms);
                }
            }
        });
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to create thread for periodic task {}: {}", task_id, e.what());
        periodic_tasks_.erase(it); // 清理失败的任务条目
        return -7;                 // ERR_THREAD_CREATION_FAILED
    }
    catch (...)
    {
        LOG_ERROR("Failed to create thread for periodic task {}: unknown exception", task_id);
        periodic_tasks_.erase(it); // 清理失败的任务条目
        return -7;                 // ERR_THREAD_CREATION_FAILED
    }

    return 0;
}

int UdpCommunicateEnhanced::removePeriodicTask(int appoint_task_id)
{
    LOG_DEBUG("Removing periodic task with ID {}", appoint_task_id);
    std::lock_guard<std::mutex> lock(task_mutex_);

    // 查找映射的任务ID
    auto map_it = task_map_.find(appoint_task_id);
    if (map_it == task_map_.end())
    {
        LOG_WARNING("Periodic task {} not found for removal", appoint_task_id);
        return -1; // ERR_TASK_NOT_FOUND
    }

    int task_id = map_it->second;
    task_map_.erase(map_it); // 清理映射表

    // 查找并停止任务
    auto task_it = periodic_tasks_.find(task_id);
    if (task_it == periodic_tasks_.end())
    {
        LOG_WARNING("Internal periodic task {} not found for removal", task_id);
        return -1; // ERR_TASK_NOT_FOUND
    }

    task_it->second.running = false;
    if (task_it->second.thread.joinable())
    {
        task_it->second.thread.join();
    }
    periodic_tasks_.erase(task_it);
    LOG_INFO("Periodic task {} removed successfully", task_id);

    return 0; // SUCCESS
}

int UdpCommunicateEnhanced::addPeriodicSendTask(const char *addr, int port,
                                                const void *pData, size_t size,
                                                int rate, int task_id)
{
    LOG_DEBUG("Adding periodic send task to {}:{} at {}Hz (requested ID: {})", addr ? addr : "NULL", port, rate, task_id);

    // 参数校验
    if (rate <= 0 || rate > 1000)
    {
        LOG_ERROR("Invalid rate {}Hz for periodic send task", rate);
        return -1; // ERR_INVALID_RATE
    }
    if (!pData || size == 0)
    {
        LOG_ERROR("Invalid data for periodic send task (ptr: {}, size: {})", static_cast<const void*>(pData), size);
        return -2; // ERR_INVALID_DATA
    }

    // 创建数据副本确保生命周期安全
    std::shared_ptr<std::vector<char>> dataCopy =
        std::make_shared<std::vector<char>>(size);
    memcpy(dataCopy->data(), pData, size);

    // 计算间隔时间
    int interval_ms = 1000 / rate;
    LOG_TRACE("Calculated interval {}ms for rate {}Hz", interval_ms, rate);

    // 创建生成器lambda
    auto generator = [dataCopy]() -> std::vector<char>
    {
        return *dataCopy; // 返回数据副本
    };

    return addPeriodicTask(interval_ms, addr, port, task_id, generator);
}

// 私有辅助函数实现
UdpCommunicateEnhanced::PeriodicTask *UdpCommunicateEnhanced::getTask(int task_id)
{
    std::lock_guard<std::mutex> lock(task_mutex_);
    auto it = periodic_tasks_.find(task_id);
    if (it == periodic_tasks_.end())
    {
        LOG_DEBUG("Task {} not found in getTask", task_id);
    }
    return (it != periodic_tasks_.end()) ? &it->second : nullptr;
}