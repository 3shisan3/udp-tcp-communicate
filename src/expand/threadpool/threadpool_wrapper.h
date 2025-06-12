#pragma once

#ifdef _WIN32
#include "threadpool_win.h"
#else
#include "threadpool.h"
#endif

#include <functional>
#include <memory>
#include <stdexcept>

class TaskMemoryPool
{
public:
    explicit TaskMemoryPool(size_t block_size, size_t block_count)
        : block_size_(std::max(block_size, sizeof(void*)))  // 确保块大小至少能存放指针
        , block_count_(block_count)
    {
        memory_ = ::operator new(block_size * block_count);

        initialize_free_list();
    }

    ~TaskMemoryPool() {
        ::operator delete(memory_);
    }

    void* allocate() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!free_list_) {
            return nullptr;
        }
        void* block = free_list_;
        free_list_ = *static_cast<void**>(block);
        return block;
    }

    void deallocate(void* block) {
        if (!block) return;
        std::lock_guard<std::mutex> lock(mtx_);
        *static_cast<void**>(block) = free_list_;
        free_list_ = block;
    }

    size_t available() const {
        std::lock_guard<std::mutex> lock(mtx_);
        size_t count = 0;
        for (void* p = free_list_; p; p = *static_cast<void**>(p)) {
            ++count;
        }
        return count;
    }

    // 禁用拷贝和移动
    TaskMemoryPool(const TaskMemoryPool&) = delete;
    TaskMemoryPool& operator=(const TaskMemoryPool&) = delete;

private:
    void initialize_free_list() {
        free_list_ = memory_;
        char* p = static_cast<char*>(memory_);
        for (size_t i = 0; i < block_count_ - 1; ++i) {
            *reinterpret_cast<void**>(p) = p + block_size_;
            p += block_size_;
        }
        *reinterpret_cast<void**>(p) = nullptr;
    }

    void* memory_ = nullptr;
    void* free_list_ = nullptr;
    size_t block_size_;
    size_t block_count_;
    mutable std::mutex mtx_;    // 允许const方法加锁
};

class ThreadPoolWrapper
{
public:
    // 封装任务类型为 std::function
    using Task = std::function<void()>;

    explicit ThreadPoolWrapper(size_t threads, size_t task_pool_size = 1024)
        : task_pool_(sizeof(Task), task_pool_size)
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
        // 从内存池分配任务内存
        void *mem = task_pool_.allocate();
        if (!mem)
        {
            throw std::runtime_error("Task memory pool exhausted");
        }

        // 将 std::function 转换为 C 风格任务
        // 使用 placement new 在预分配内存上构造 Task
        Task *c_task = new (mem) Task(std::move(task));

        threadpool_task t{
            [](void *arg)
            {
                auto *f = static_cast<Task *>(arg);
                (*f)();     // 执行任务
                f->~Task(); // 显式调用析构函数
                // 内存由内存池管理，不直接 delete
            },
            c_task};
        if (threadpool_schedule(&t, pool_) != 0)
        {
            c_task->~Task();
            task_pool_.deallocate(mem);
            throw std::runtime_error("Failed to enqueue task");
        }
    }

    // 添加一个方法用于回收任务内存
    void recycle_task_memory(void *mem)
    {
        task_pool_.deallocate(mem);
    }

    /* 可实现添加异步返回任务 */

private:
    threadpool_t *pool_;
    TaskMemoryPool task_pool_;
};