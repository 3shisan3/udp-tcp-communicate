#ifndef THREADPOOL_WIN_H_
#define THREADPOOL_WIN_H_

#include <windows.h>
#include <stdbool.h>
#include <stddef.h>

struct taskqueue_t
{
    size_t queue_maxsize;
    size_t taskNum;
    int linkoff;
    bool nonblock;
    void *head1;
    void *head2;
    void **get_head;
    void **put_head;
    void **put_tail;
    CRITICAL_SECTION get_mutex;
    CRITICAL_SECTION put_mutex;
    CONDITION_VARIABLE get_cond;
    CONDITION_VARIABLE put_cond;
};

struct threadpool_task
{
    void (*routine)(void *);
    void *data;
};

struct threadpool_t
{
    size_t nthreads;
    size_t stacksize;
    HANDLE *threads;
    CRITICAL_SECTION mutex;
    DWORD key;
    CONDITION_VARIABLE *terminate;
    CONDITION_VARIABLE *pause;
    taskqueue_t *taskqueue;
};

#ifdef __cplusplus
extern "C" {
#endif

// 任务队列接口
taskqueue_t *taskqueue_create(size_t maxlen, int linkoff);
void *taskqueue_get(taskqueue_t *queue);
void taskqueue_put(void *msg, taskqueue_t *queue);
void taskqueue_put_head(void *msg, taskqueue_t *queue);
void taskqueue_set_nonblock(taskqueue_t *queue);
void taskqueue_set_block(taskqueue_t *queue);
void taskqueue_destroy(taskqueue_t *queue);

// 线程池接口
threadpool_t *threadpool_create(size_t nthreads, size_t stacksize);
void threadpool_swap_taskqueue(threadpool_t *pool, taskqueue_t *taskqueue);
int threadpool_schedule(const struct threadpool_task *task, threadpool_t *pool);
int threadpool_in_pool(threadpool_t *pool);
int threadpool_increase(threadpool_t *pool);
int threadpool_decrease(threadpool_t *pool);
void threadpool_exit(threadpool_t *pool);
void threadpool_destroy(void (*pending)(const struct threadpool_task *),
                      threadpool_t *pool);

#ifdef __cplusplus
}
#endif

#endif // THREADPOOL_WIN_H_