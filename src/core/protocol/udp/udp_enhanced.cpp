#include "udp_enhanced.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

class UdpEnhanced::Impl
{
public:
    Impl(ProcessingMode mode, int thread_pool_size)
        : mode_(mode), running_(false)
    {

        if (mode == ProcessingMode::ThreadPool)
        {
            for (int i = 0; i < thread_pool_size; ++i)
            {
                workers_.emplace_back([this]()
                                      {
                    while (running_) {
                        processNextJob();
                    } });
            }
        }
    }

    ~Impl()
    {
        stop();
    }

    void start()
    {
        running_ = true;
        // 启动周期任务线程
        task_thread_ = std::thread(&Impl::taskScheduler, this);
    }

    void stop()
    {
        running_ = false;

        // 停止周期任务
        for (auto &task : periodic_tasks_)
        {
            task.is_running = false;
        }

        // 停止线程池
        if (mode_ == ProcessingMode::ThreadPool)
        {
            cv_.notify_all();
            for (auto &worker : workers_)
            {
                if (worker.joinable())
                    worker.join();
            }
        }

        // 停止任务线程
        if (task_thread_.joinable())
        {
            task_thread_.join();
        }
    }

    // 异步发送实现
    bool asyncSend(const std::string &dest_addr, int dest_port,
                   const void *data, size_t size)
    {
        // 实际项目中应该使用线程池或IO多路复用
        std::thread([this, dest_addr, dest_port, data = std::vector<char>(static_cast<const char *>(data), static_cast<const char *>(data) + size)]()
                    { sendTo(dest_addr, dest_port, data.data(), data.size()); })
            .detach();
        return true;
    }

    // 周期任务管理
    int addPeriodicTask(const PeriodicTask &task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        periodic_tasks_.push_back(task);
        periodic_tasks_.back().is_running = true;
        periodic_tasks_.back().last_send_time = std::chrono::steady_clock::now();
        return periodic_tasks_.size() - 1;
    }

    bool removePeriodicTask(int task_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (task_id < 0 || task_id >= periodic_tasks_.size())
        {
            return false;
        }
        periodic_tasks_[task_id].is_running = false;
        periodic_tasks_.erase(periodic_tasks_.begin() + task_id);
        return true;
    }

    // 地址过滤
    void addAddressHandler(const std::string &addr,
                           std::function<void(const char *, size_t)> handler)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        addr_handlers_[addr] = std::move(handler);
    }

    // 处理接收到的数据
    void handleReceive(const std::string &src_addr, int src_port,
                       const char *data, size_t size)
    {
        // 1. 检查是否有特定地址处理器
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = addr_handlers_.find(src_addr);
            if (it != addr_handlers_.end())
            {
                it->second(data, size);
                return;
            }
        }

        // 2. 根据模式处理
        if (mode_ == ProcessingMode::EventDriven)
        {
            processData(src_addr, src_port, data, size);
        }
        else
        {
            std::lock_guard<std::mutex> lock(mutex_);
            job_queue_.push({src_addr, src_port,
                             std::vector<char>(data, data + size)});
            cv_.notify_one();
        }
    }

private:
    void taskScheduler()
    {
        while (running_)
        {
            auto now = std::chrono::steady_clock::now();

            std::vector<PeriodicTask> tasks_to_run;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto &task : periodic_tasks_)
                {
                    if (!task.is_running)
                        continue;

                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       now - task.last_send_time)
                                       .count();

                    if (elapsed >= task.interval_ms)
                    {
                        tasks_to_run.push_back(task);
                        task.last_send_time = now;
                    }
                }
            }

            // 执行任务
            for (const auto &task : tasks_to_run)
            {
                auto data = task.data_generator();
                sendTo(task.dest_addr, task.dest_port, data.data(), data.size());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void processNextJob()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]()
                 { return !job_queue_.empty() || !running_; });

        if (!running_)
            return;

        auto job = std::move(job_queue_.front());
        job_queue_.pop();
        lock.unlock();

        processData(job.src_addr, job.src_port, job.data.data(), job.data.size());
    }

    // 实际数据处理函数
    void processData(const std::string &src_addr, int src_port,
                     const char *data, size_t size)
    {
        // 这里可以添加统一的数据处理逻辑
        // 例如日志记录、数据统计等
    }

    ProcessingMode mode_;
    std::atomic<bool> running_;

    // 周期任务相关
    std::vector<PeriodicTask> periodic_tasks_;
    std::thread task_thread_;

    // 线程池相关
    std::vector<std::thread> workers_;
    struct Job
    {
        std::string src_addr;
        int src_port;
        std::vector<char> data;
    };
    std::queue<Job> job_queue_;
    std::mutex mutex_;
    std::condition_variable cv_;

    // 地址过滤器
    std::unordered_map<std::string, std::function<void(const char *, size_t)>> addr_handlers_;
};

// UdpEnhanced成员函数实现
UdpEnhanced::UdpEnhanced(ProcessingMode mode, int thread_pool_size)
    : impl_(std::make_unique<Impl>(mode, thread_pool_size))
{
    // 设置UdpCore的回调
    setReceiveCallback([this](auto &&...args)
                       { impl_->handleReceive(std::forward<decltype(args)>(args)...); });
}

UdpEnhanced::~UdpEnhanced() = default;

bool UdpEnhanced::sendAsync(const std::string &dest_addr, int dest_port,
                            const void *data, size_t size)
{
    return impl_->asyncSend(dest_addr, dest_port, data, size);
}

int UdpEnhanced::addPeriodicTask(int interval_ms,
                                 const std::string &dest_addr,
                                 int dest_port,
                                 std::function<std::vector<char>()> data_generator)
{
    PeriodicTask task;
    task.dest_addr = dest_addr;
    task.dest_port = dest_port;
    task.data_generator = std::move(data_generator);
    task.interval_ms = interval_ms;
    return impl_->addPeriodicTask(task);
}

bool UdpEnhanced::removePeriodicTask(int task_id)
{
    return impl_->removePeriodicTask(task_id);
}

void UdpEnhanced::addAddressHandler(
    const std::string &addr,
    std::function<void(const char *data, size_t size)> handler)
{
    impl_->addAddressHandler(addr, std::move(handler));
}

void UdpEnhanced::setProcessingMode(ProcessingMode mode)
{
    impl_->setProcessingMode(mode);
}