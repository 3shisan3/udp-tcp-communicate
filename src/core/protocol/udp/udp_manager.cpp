#include "udp_manager.h"
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>

UdpManager::UdpManager(Mode mode, int threadCount)
    : m_mode_(mode), m_threadCount_(threadCount), m_running_(false) {}

UdpManager::~UdpManager()
{
    stop();
}

void UdpManager::addTask(const std::string &ip, int port)
{
    std::lock_guard<std::mutex> lock(m_queueMutex_);
    m_taskQueue_.push({ip, port});
    m_condition_.notify_one();
}

void UdpManager::start()
{
    m_running_ = true;

    if (m_mode_ == Mode::THREAD_POOL)
    {
        for (int i = 0; i < m_threadCount_; ++i)
        {
            m_threads_.emplace_back(&UdpManager::threadWorker, this);
        }
    }
    else if (m_mode_ == Mode::EVENT_DRIVEN)
    {
        int epollFd = epoll_create1(0);
        if (epollFd == -1)
        {
            std::cerr << "Failed to create epoll instance: " << strerror(errno) << std::endl;
            return;
        }

        while (m_running_)
        {
            std::vector<epoll_event> events(10);
            int eventCount = epoll_wait(epollFd, events.data(), events.size(), -1);

            if (eventCount == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                std::cerr << "epoll_wait failed: " << strerror(errno) << std::endl;
                break;
            }

            for (int i = 0; i < eventCount; ++i)
            {
                if (events[i].events & EPOLLIN)
                {
                    UdpCommunicate *udp = static_cast<UdpCommunicate *>(events[i].data.ptr);
                    std::string data = udp->receiveData();
                    std::cout << "Received data: " << data << std::endl;
                }
            }
        }

        close(epollFd);
    }
}

void UdpManager::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex_);
        m_running_ = false;
    }
    m_condition_.notify_all();

    for (std::thread &thread : m_threads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    m_threads_.clear();
}

void UdpManager::threadWorker()
{
    while (m_running_)
    {
        Task task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex_);
            m_condition_.wait(lock, [this]
                              { return !m_running_ || !m_taskQueue_.empty(); });

            if (!m_running_ && m_taskQueue_.empty())
            {
                return;
            }

            task = m_taskQueue_.front();
            m_taskQueue_.pop();
        }

        // 处理任务
        std::cout << "处理任务: IP=" << task.ip << ", Port=" << task.port << std::endl;

        // 示例：为每个任务创建一个UdpCommunicate实例
        UdpCommunicate udp;
        if (udp.initializeSocket(task.port))
        {
            std::cout << "成功监听端口: " << task.port << std::endl;
        }
        else
        {
            std::cerr << "监听端口失败: " << task.port << std::endl;
        }
    }
}