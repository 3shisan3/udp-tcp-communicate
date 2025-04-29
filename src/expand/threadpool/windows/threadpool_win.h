#ifndef THREADPOOL_WIN_H_
#define THREADPOOL_WIN_H_

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>

// 任务队列结构体
struct taskqueue_t
{
    size_t queue_maxsize;      // 队列最大容量
    size_t taskNum;            // 当前任务数量
    int linkoff;               // 任务链接偏移量
    bool nonblock;             // 非阻塞模式标志
    void *head1;               // 第一个链表头
    void *head2;               // 第二个链表头
    void **get_head;           // 获取任务的链表头指针
    void **put_head;           // 放入任务的链表头指针
    void **put_tail;           // 放入任务的链表尾指针
    CRITICAL_SECTION get_mutex;// 获取任务的互斥锁
    CRITICAL_SECTION put_mutex;// 放入任务的互斥锁
    CONDITION_VARIABLE get_cond;// 获取任务的条件变量
    CONDITION_VARIABLE put_cond;// 放入任务的条件变量
};

// 线程池任务结构体
struct threadpool_task
{
    void (*routine)(void *);   // 任务函数指针
    void *data;                // 任务函数参数
};

// 线程池结构体
struct threadpool_t
{
    size_t nthreads;           // 线程数量
    size_t stacksize;          // 线程栈大小
    HANDLE tid;                // 线程句柄(类似于原版的pthread_t)
    CRITICAL_SECTION mutex;    // 互斥锁
    DWORD key;                 // 线程本地存储键
    CONDITION_VARIABLE *terminate; // 终止条件变量
    taskqueue_t *taskqueue;    // 任务队列指针
};

#ifdef __cplusplus
extern "C" {
#endif

/* 任务队列接口 */

// 创建任务队列
taskqueue_t *taskqueue_create(size_t maxlen, int linkoff);
// 从队列获取任务
void *taskqueue_get(taskqueue_t *queue);
// 向队列放入任务
void taskqueue_put(void *msg, taskqueue_t *queue);
// 向队列头部放入任务
void taskqueue_put_head(void *msg, taskqueue_t *queue);
// 设置队列为非阻塞模式
void taskqueue_set_nonblock(taskqueue_t *queue);
// 设置队列为阻塞模式
void taskqueue_set_block(taskqueue_t *queue);
// 销毁任务队列
void taskqueue_destroy(taskqueue_t *queue);

/* 线程池接口 */

// 创建线程池
threadpool_t *threadpool_create(size_t nthreads, size_t stacksize);
// 交换线程池的任务队列
void threadpool_swap_taskqueue(threadpool_t *pool, taskqueue_t *taskqueue);
// 调度任务到线程池
int threadpool_schedule(const struct threadpool_task *task, threadpool_t *pool);
// 检查当前线程是否在线程池中
int threadpool_in_pool(threadpool_t *pool);
// 增加线程池线程数量
int threadpool_increase(threadpool_t *pool);
// 减少线程池线程数量
int threadpool_decrease(threadpool_t *pool);
// 退出线程池
void threadpool_exit(threadpool_t *pool);
// 销毁线程池
void threadpool_destroy(void (*pending)(const struct threadpool_task *),
                      threadpool_t *pool);

#ifdef __cplusplus
}
#endif

#endif // THREADPOOL_WIN_H_