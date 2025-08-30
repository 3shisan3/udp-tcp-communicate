#pragma once

#ifdef _WIN32
#include "threadpool_win.h"
#else
#include "threadpool.h"
#endif

#include <functional>
#include <memory>
#include <stdexcept>
class ThreadPoolWrapper
{
public:
// 封装任务类型为 std::function
    using Task = std::function<void()>;

    explicit ThreadPoolWrapper(size_t threads, size_t task_queue_size = 1024)
    {
        // 创建任务队列，linkoff设置为0因为我们使用独立的任务结构
        taskqueue_ = taskqueue_create(task_queue_size, 0);
        if (!taskqueue_)
        {
            throw std::runtime_error("Failed to create task queue");
        }

        // 创建线程池
        pool_ = threadpool_create(threads, 0);
        if (!pool_)
        {
            taskqueue_destroy(taskqueue_);
            throw std::runtime_error("Failed to create threadpool");
        }

        // 关联任务队列和线程池
        threadpool_swap_taskqueue(pool_, taskqueue_);
    }

    ~ThreadPoolWrapper()
    {
        // 先停止接收新任务
        taskqueue_set_nonblock(taskqueue_);

        // 销毁线程池，不处理pending任务
        threadpool_destroy(nullptr, pool_);

        // 销毁任务队列
        taskqueue_destroy(taskqueue_);
    }

    void enqueue(Task task)
    {
        // 分配任务内存
        auto *entry = new TaskEntry{std::move(task)};

        threadpool_task t{
            [](void *arg)
            {
                auto *entry = static_cast<TaskEntry *>(arg);
                try
                {
                    entry->task(); // 执行任务
                }
                catch (...)
                {
                    // 捕获所有异常，防止线程因异常退出
                }
                delete entry; // 释放任务内存
            },
            entry};

        if (threadpool_schedule(&t, pool_) != 0)
        {
            delete entry; // 入队失败时释放内存
            throw std::runtime_error("Failed to enqueue task");
        }
    }

    size_t threadCount() const
    {
        return pool_->nthreads;
    }

    // 禁用拷贝和移动
    ThreadPoolWrapper(const ThreadPoolWrapper &) = delete;
    ThreadPoolWrapper &operator=(const ThreadPoolWrapper &) = delete;

private:
    struct TaskEntry
    {
        Task task;
    };

    threadpool_t *pool_ = nullptr;
    taskqueue_t *taskqueue_ = nullptr;
};