#include "threadpool_win.h"
#include <stdlib.h>

// 线程退出处理函数声明
static void __threadpool_exit_routine(void *context);
static void __threadpool_terminate(int in_pool, threadpool_t *pool);

///////////*//////////     任务队列相关操作接口封装    //////////*///////////

void taskqueue_set_nonblock(taskqueue_t *queue)
{
    queue->nonblock = true;
    EnterCriticalSection(&queue->put_mutex);
    WakeConditionVariable(&queue->get_cond);
    WakeAllConditionVariable(&queue->put_cond);
    LeaveCriticalSection(&queue->put_mutex);
}

void taskqueue_set_block(taskqueue_t *queue)
{
    queue->nonblock = false;
}

taskqueue_t *taskqueue_create(size_t maxlen, int linkoff)
{
    taskqueue_t *queue = (taskqueue_t *)malloc(sizeof(taskqueue_t));
    if (!queue)
        return NULL;

    InitializeCriticalSection(&queue->get_mutex);
    InitializeCriticalSection(&queue->put_mutex);
    InitializeConditionVariable(&queue->get_cond);
    InitializeConditionVariable(&queue->put_cond);

    queue->queue_maxsize = maxlen;
    queue->linkoff = linkoff;
    queue->head1 = NULL;
    queue->head2 = NULL;
    queue->get_head = &queue->head1;
    queue->put_head = &queue->head2;
    queue->put_tail = &queue->head2;
    queue->taskNum = 0;
    queue->nonblock = false;
    
    return queue;
}

void taskqueue_destroy(taskqueue_t *queue)
{
    DeleteCriticalSection(&queue->put_mutex);
    DeleteCriticalSection(&queue->get_mutex);
    free(queue);
}

void taskqueue_put(void *task, taskqueue_t *queue)
{
    void **link = (void **)((char *)task + queue->linkoff);
    *link = NULL;

    EnterCriticalSection(&queue->put_mutex);
    while (queue->taskNum > queue->queue_maxsize - 1 && !queue->nonblock)
        SleepConditionVariableCS(&queue->put_cond, &queue->put_mutex, INFINITE);

    *queue->put_tail = link;
    queue->put_tail = link;
    queue->taskNum++;
    LeaveCriticalSection(&queue->put_mutex);
    WakeConditionVariable(&queue->get_cond);
}

void taskqueue_put_head(void *task, taskqueue_t *queue)
{
    void **link = (void **)((char *)task + queue->linkoff);

    EnterCriticalSection(&queue->put_mutex);
    while (*queue->get_head)
    {
        if (TryEnterCriticalSection(&queue->get_mutex))
        {
            LeaveCriticalSection(&queue->put_mutex);
            *link = *queue->get_head;
            *queue->get_head = link;
            LeaveCriticalSection(&queue->get_mutex);
            return;
        }
    }

    while (queue->taskNum > queue->queue_maxsize - 1 && !queue->nonblock)
        SleepConditionVariableCS(&queue->put_cond, &queue->put_mutex, INFINITE);

    *link = *queue->put_head;
    if (*link == NULL)
        queue->put_tail = link;

    *queue->put_head = link;
    queue->taskNum++;
    LeaveCriticalSection(&queue->put_mutex);
    WakeConditionVariable(&queue->get_cond);
}

static size_t __taskqueue_swap(taskqueue_t *queue)
{
    void **get_head = queue->get_head;
    size_t cnt;

    EnterCriticalSection(&queue->put_mutex);
    while (queue->taskNum == 0 && !queue->nonblock)
        SleepConditionVariableCS(&queue->get_cond, &queue->put_mutex, INFINITE);

    cnt = queue->taskNum;
    if (cnt > queue->queue_maxsize - 1)
        WakeAllConditionVariable(&queue->put_cond);

    queue->get_head = queue->put_head;
    queue->put_head = get_head;
    queue->put_tail = get_head;
    queue->taskNum = 0;
    LeaveCriticalSection(&queue->put_mutex);
    return cnt;
}

void *taskqueue_get(taskqueue_t *queue)
{
    void *task;

    EnterCriticalSection(&queue->get_mutex);
    if (*queue->get_head || __taskqueue_swap(queue) > 0)
    {
        task = (char *)*queue->get_head - queue->linkoff;
        *queue->get_head = *(void **)*queue->get_head;
    }
    else
    {
        task = NULL;
    }
    LeaveCriticalSection(&queue->get_mutex);
    
    return task;
}

///////////*//////////      线程池相关操作接口封装     //////////*///////////

struct __threadpool_task_entry
{
    void *link;
    struct threadpool_task task;
};

static DWORD WINAPI __threadpool_routine(LPVOID arg)
{
    threadpool_t *pool = (threadpool_t *)arg;
    struct __threadpool_task_entry *entry;
    void (*task_routine)(void *);
    void *task_context;

    TlsSetValue(pool->key, (LPVOID)pool);
    while (pool->terminate == NULL)
    {
        while (pool->pause && pool->terminate == NULL)
        {
            Sleep(1);
        }
            
        entry = (struct __threadpool_task_entry *)taskqueue_get(pool->taskqueue);
        if (!entry)
            break;

        task_routine = entry->task.routine;
        task_context = entry->task.data;
        free(entry);
        task_routine(task_context);

        if (pool->nthreads == 0)
        {
            free(pool);
            return 0;
        }
    }

    // 线程退出处理
    __threadpool_exit_routine(pool);
    return 0;
}

static void __threadpool_exit_routine(void *context)
{
    threadpool_t *pool = (threadpool_t *)context;
    HANDLE thread = INVALID_HANDLE_VALUE;

    EnterCriticalSection(&pool->mutex);
    if (pool->nthreads > 0)
    {
        thread = pool->threads[pool->nthreads - 1];
        pool->nthreads--;
        
        if (pool->nthreads == 0 && pool->terminate != NULL)
            WakeConditionVariable(pool->terminate);
    }
    LeaveCriticalSection(&pool->mutex);

    if (thread != INVALID_HANDLE_VALUE && thread != GetCurrentThread())
        WaitForSingleObject(thread, INFINITE);
}

static void __threadpool_terminate(int in_pool, threadpool_t *pool)
{
    CONDITION_VARIABLE term;
    InitializeConditionVariable(&term);

    EnterCriticalSection(&pool->mutex);
    taskqueue_set_nonblock(pool->taskqueue);
    pool->terminate = &term;

    if (in_pool)
    {
        pool->nthreads--;
    }

    while (pool->nthreads > 0)
        SleepConditionVariableCS(&term, &pool->mutex, INFINITE);

    LeaveCriticalSection(&pool->mutex);
}

static int __threadpool_create_threads(size_t nthreads, threadpool_t *pool)
{
    pool->threads = (HANDLE *)malloc(sizeof(HANDLE) * nthreads);
    if (!pool->threads)
        return -1;

    for (size_t i = 0; i < nthreads; i++)
    {
        pool->threads[i] = CreateThread(
            NULL,
            pool->stacksize,
            __threadpool_routine,
            pool,
            0,
            NULL);
        
        if (pool->threads[i] == NULL)
        {
            __threadpool_terminate(0, pool);
            return -1;
        }
        pool->nthreads++;
    }
    
    return 0;
}

threadpool_t *threadpool_create(size_t nthreads, size_t stacksize)
{
    threadpool_t *pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    if (!pool)
        return NULL;

    pool->taskqueue = taskqueue_create(0, 0);
    if (!pool->taskqueue)
    {
        free(pool);
        return NULL;
    }

    InitializeCriticalSection(&pool->mutex);
    pool->key = TlsAlloc();
    if (pool->key == TLS_OUT_OF_INDEXES)
    {
        taskqueue_destroy(pool->taskqueue);
        free(pool);
        return NULL;
    }

    pool->stacksize = stacksize;
    pool->nthreads = 0;
    pool->threads = NULL;
    pool->terminate = NULL;
    pool->pause = NULL;

    if (__threadpool_create_threads(nthreads, pool) < 0)
    {
        TlsFree(pool->key);
        taskqueue_destroy(pool->taskqueue);
        free(pool);
        return NULL;
    }

    return pool;
}

void threadpool_swap_taskqueue(threadpool_t *pool, taskqueue_t *taskqueue)
{
    if (!taskqueue)
        return;

    EnterCriticalSection(&pool->mutex);

    CONDITION_VARIABLE term;
    InitializeConditionVariable(&term);
    pool->pause = &term;

    taskqueue_t *old = pool->taskqueue;
    taskqueue_put(NULL, old);
    taskqueue_get(old);
    
    pool->taskqueue = taskqueue;
    taskqueue_destroy(old);
    pool->pause = NULL;

    LeaveCriticalSection(&pool->mutex);
}

int threadpool_schedule(const struct threadpool_task *task, threadpool_t *pool)
{
    struct __threadpool_task_entry *entry = (struct __threadpool_task_entry *)malloc(sizeof(struct __threadpool_task_entry));
    if (!entry)
        return -1;

    entry->task = *task;
    while (pool->pause)
    {
        Sleep(1);
    }
    taskqueue_put(entry, pool->taskqueue);
    return 0;
}

int threadpool_in_pool(threadpool_t *pool)
{
    return TlsGetValue(pool->key) == (LPVOID)pool;
}

int threadpool_increase(threadpool_t *pool)
{
    HANDLE *new_threads = (HANDLE *)realloc(pool->threads, sizeof(HANDLE) * (pool->nthreads + 1));
    if (!new_threads)
        return -1;

    pool->threads = new_threads;
    EnterCriticalSection(&pool->mutex);
    
    pool->threads[pool->nthreads] = CreateThread(
        NULL,
        pool->stacksize,
        __threadpool_routine,
        pool,
        0,
        NULL);
    
    if (pool->threads[pool->nthreads] == NULL)
    {
        LeaveCriticalSection(&pool->mutex);
        return -1;
    }

    pool->nthreads++;
    LeaveCriticalSection(&pool->mutex);
    return 0;
}

int threadpool_decrease(threadpool_t *pool)
{
    struct __threadpool_task_entry *entry = (struct __threadpool_task_entry *)malloc(sizeof(struct __threadpool_task_entry));
    if (!entry)
        return -1;

    entry->task.routine = __threadpool_exit_routine;
    entry->task.data = pool;
    
    while (pool->pause)
    {
        Sleep(1);
    }
    
    taskqueue_put_head(entry, pool->taskqueue);
    return 0;
}

void threadpool_exit(threadpool_t *pool)
{
    if (threadpool_in_pool(pool))
    {
        __threadpool_exit_routine(pool);
    }
}

void threadpool_destroy(void (*pending)(const struct threadpool_task *), threadpool_t *pool)
{
    int in_pool = threadpool_in_pool(pool);
    struct __threadpool_task_entry *entry;

    __threadpool_terminate(in_pool, pool);
    while (1)
    {
        entry = (struct __threadpool_task_entry *)taskqueue_get(pool->taskqueue);
        if (!entry)
            break;

        if (pending && entry->task.routine != __threadpool_exit_routine)
            pending(&entry->task);

        free(entry);
    }

    TlsFree(pool->key);
    DeleteCriticalSection(&pool->mutex);
    taskqueue_destroy(pool->taskqueue);
    free(pool->threads);
    if (!in_pool)
        free(pool);
}