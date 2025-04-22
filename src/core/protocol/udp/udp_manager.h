#ifndef UDP_MANAGER_H
#define UDP_MANAGER_H

#include "udp_communicate.h"
#include <unordered_map>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>

class UdpManager
{
public:
    enum class Mode
    {
        THREAD_POOL, // 使用线程池模式
        EVENT_DRIVEN // 使用事件驱动模式
    };

    UdpManager(Mode mode, int threadCount = 4);
    ~UdpManager();

    // 添加一个监听任务
    void addTask(const std::string &ip, int port);

    // 启动管理器
    void start();

    // 停止管理器
    void stop();

private:
    struct Task
    {
        std::string ip;
        int port;
    };

    // 线程池相关
    void threadWorker();

    Mode m_mode_;                         // 当前模式
    int m_threadCount_;                   // 线程池线程数量
    std::vector<std::thread> m_threads_;  // 线程池
    std::queue<Task> m_taskQueue_;        // 任务队列
    std::mutex m_queueMutex_;             // 队列互斥锁
    std::condition_variable m_condition_; // 条件变量
    bool m_running_;                      // 管理器运行状态

    // IP到线程的映射
    std::unordered_map<std::string, int> m_ipToThreadMap_;
};

#endif // UDP_MANAGER_H