#pragma once

#include "threadpool.h"

#include <functional>
#include <memory>

class ThreadPoolWrapper
{
public:
    // 封装任务类型为 std::function
    using Task = std::function<void()>;

    explicit ThreadPoolWrapper(size_t threads)
    {
        pool_ = threadpool_create(threads, 0); // 默认栈大小
        if (!pool_)
            throw std::runtime_error("Failed to create threadpool");
    }

    ~ThreadPoolWrapper()
    {
        threadpool_destroy(nullptr, pool_); // 无 pending 回调
    }

    void enqueue(Task task)
    {
        // 将 std::function 转换为 C 风格任务
        auto *c_task = new Task(std::move(task));
        threadpool_task t{
            [](void *arg)
            {
                auto *f = static_cast<Task *>(arg);
                (*f)();   // 执行任务
                delete f; // 释放内存
            },
            c_task};
        if (threadpool_schedule(&t, pool_) != 0)
        {
            delete c_task;
            throw std::runtime_error("Failed to enqueue task");
        }
    }

private:
    threadpool_t *pool_;
};