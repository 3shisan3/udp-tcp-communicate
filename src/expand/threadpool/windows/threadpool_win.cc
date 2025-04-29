#include "threadpool_win.h"
#include <stdlib.h>

// 声明线程退出处理函数
static void __threadpool_exit_routine(void *context);
static void __threadpool_terminate(int in_pool, threadpool_t *pool);

////////// 任务队列实现 //////////

// 设置队列为非阻塞模式
void taskqueue_set_nonblock(taskqueue_t *queue)
{
    queue->nonblock = true; // 设置非阻塞标志
    EnterCriticalSection(&queue->put_mutex);
    // 唤醒可能等待的线程
    WakeConditionVariable(&queue->get_cond);
    WakeAllConditionVariable(&queue->put_cond);
    LeaveCriticalSection(&queue->put_mutex);
}

// 设置队列为阻塞模式
void taskqueue_set_block(taskqueue_t *queue)
{
    queue->nonblock = false; // 设置阻塞标志
}

// 创建任务队列
taskqueue_t *taskqueue_create(size_t maxlen, int linkoff)
{
    taskqueue_t *queue = (taskqueue_t *)malloc(sizeof(taskqueue_t));
    if (!queue)
        return NULL;

    // 初始化临界区和条件变量
    InitializeCriticalSection(&queue->get_mutex);
    InitializeCriticalSection(&queue->put_mutex);
    InitializeConditionVariable(&queue->get_cond);
    InitializeConditionVariable(&queue->put_cond);

    // 初始化队列参数
    queue->queue_maxsize = maxlen;
    queue->linkoff = linkoff;
    queue->head1 = NULL;
    queue->head2 = NULL;
    queue->get_head = &queue->head1; // 初始get_head指向head1
    queue->put_head = &queue->head2; // 初始put_head指向head2
    queue->put_tail = &queue->head2; // 初始put_tail指向head2
    queue->taskNum = 0;
    queue->nonblock = false; // 默认阻塞模式

    return queue;
}

// 销毁任务队列
void taskqueue_destroy(taskqueue_t *queue)
{
    // 销毁临界区和条件变量
    DeleteCriticalSection(&queue->put_mutex);
    DeleteCriticalSection(&queue->get_mutex);
    free(queue);
}

// 向队列尾部放入任务
void taskqueue_put(void *task, taskqueue_t *queue)
{
    // 计算任务链接指针位置
    void **link = (void **)((char *)task + queue->linkoff);
    *link = NULL; // 初始化链接指针

    EnterCriticalSection(&queue->put_mutex);
    // 如果队列满且是阻塞模式，则等待
    while (queue->taskNum > queue->queue_maxsize - 1 && !queue->nonblock)
        SleepConditionVariableCS(&queue->put_cond, &queue->put_mutex, INFINITE);

    // 将任务添加到队列尾部
    *queue->put_tail = link;
    queue->put_tail = link;
    queue->taskNum++;
    LeaveCriticalSection(&queue->put_mutex);

    // 唤醒等待获取任务的线程
    WakeConditionVariable(&queue->get_cond);
}

// 向队列头部放入任务
void taskqueue_put_head(void *task, taskqueue_t *queue)
{
    void **link = (void **)((char *)task + queue->linkoff);

    EnterCriticalSection(&queue->put_mutex);
    // 尝试直接放入get队列头部
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

    // 如果队列满且是阻塞模式，则等待
    while (queue->taskNum > queue->queue_maxsize - 1 && !queue->nonblock)
        SleepConditionVariableCS(&queue->put_cond, &queue->put_mutex, INFINITE);

    // 将任务添加到put队列头部
    *link = *queue->put_head;
    if (*link == NULL) // 如果是第一个任务，更新tail指针
        queue->put_tail = link;

    *queue->put_head = link;
    queue->taskNum++;
    LeaveCriticalSection(&queue->put_mutex);
    WakeConditionVariable(&queue->get_cond);
}

// 交换get和put队列（内部函数）
static size_t __taskqueue_swap(taskqueue_t *queue)
{
    void **get_head = queue->get_head;
    size_t cnt;

    EnterCriticalSection(&queue->put_mutex);
    // 等待队列中有任务（阻塞模式下）
    while (queue->taskNum == 0 && !queue->nonblock)
        SleepConditionVariableCS(&queue->get_cond, &queue->put_mutex, INFINITE);

    cnt = queue->taskNum;
    // 如果队列满，唤醒可能等待的put线程
    if (cnt > queue->queue_maxsize - 1)
        WakeAllConditionVariable(&queue->put_cond);

    // 交换get和put队列
    queue->get_head = queue->put_head;
    queue->put_head = get_head;
    queue->put_tail = get_head;
    queue->taskNum = 0;
    LeaveCriticalSection(&queue->put_mutex);
    return cnt;
}

// 从队列获取任务
void *taskqueue_get(taskqueue_t *queue)
{
    void *task;

    EnterCriticalSection(&queue->get_mutex);
    // 如果当前get队列有任务或交换后新队列有任务
    if (*queue->get_head || __taskqueue_swap(queue) > 0)
    {
        // 计算任务起始地址
        task = (char *)*queue->get_head - queue->linkoff;
        // 移动get_head指针
        *queue->get_head = *(void **)*queue->get_head;
    }
    else
    {
        task = NULL; // 无任务可用
    }
    LeaveCriticalSection(&queue->get_mutex);

    return task;
}

////////// 线程池实现 //////////

// 线程池任务条目结构
struct __threadpool_task_entry
{
    void *link;                  // 任务链接指针
    struct threadpool_task task; // 实际任务
};

// 空的线程句柄标识
static HANDLE __zero_handle = INVALID_HANDLE_VALUE;

// 线程工作函数
static DWORD WINAPI __threadpool_routine(LPVOID arg)
{
    threadpool_t *pool = (threadpool_t *)arg;
    struct __threadpool_task_entry *entry;
    void (*task_routine)(void *);
    void *task_context;

    // 设置线程特定数据
    TlsSetValue(pool->key, (LPVOID)pool);
    while (!pool->terminate) // 检查终止标志
    {
        // 从队列获取任务
        entry = (struct __threadpool_task_entry *)taskqueue_get(pool->taskqueue);
        if (!entry)
            break;

        // 执行任务
        task_routine = entry->task.routine;
        task_context = entry->task.data;
        free(entry);
        task_routine(task_context);

        // 检查线程数是否为0（可能在执行任务时被减少）
        if (pool->nthreads == 0)
        {
            free(pool);
            return 0;
        }
    }

    // 执行线程退出处理
    __threadpool_exit_routine(pool);
    return 0;
}

// 线程退出处理函数
static void __threadpool_exit_routine(void *context)
{
    threadpool_t *pool = (threadpool_t *)context;
    HANDLE tid = INVALID_HANDLE_VALUE;

    EnterCriticalSection(&pool->mutex);
    // 保存当前线程ID并更新为调用线程
    tid = pool->tid;
    pool->tid = GetCurrentThread();
    // 减少线程计数，如果为0且设置了终止条件，则唤醒
    if (--pool->nthreads == 0 && pool->terminate)
        WakeConditionVariable(pool->terminate);

    LeaveCriticalSection(&pool->mutex);

    // 等待前一个线程结束（链式等待）
    if (tid != INVALID_HANDLE_VALUE && tid != __zero_handle)
        WaitForSingleObject(tid, INFINITE);
}

// 终止线程池（内部函数）
static void __threadpool_terminate(int in_pool, threadpool_t *pool)
{
    CONDITION_VARIABLE term;
    InitializeConditionVariable(&term);

    EnterCriticalSection(&pool->mutex);
    // 设置非阻塞模式并标记终止
    taskqueue_set_nonblock(pool->taskqueue);
    pool->terminate = &term;

    // 如果是在池线程中调用，减少计数
    if (in_pool)
    {
        pool->nthreads--;
    }

    // 等待所有线程退出
    while (pool->nthreads > 0)
        SleepConditionVariableCS(&term, &pool->mutex, INFINITE);

    LeaveCriticalSection(&pool->mutex);

    // 等待最后一个线程结束
    if (pool->tid != INVALID_HANDLE_VALUE && pool->tid != __zero_handle)
        WaitForSingleObject(pool->tid, INFINITE);
}

// 创建线程池线程（内部函数）
static int __threadpool_create_threads(size_t nthreads, threadpool_t *pool)
{
    DWORD threadId;
    int ret = 0;

    for (size_t i = 0; i < nthreads; i++)
    {
        // 创建新线程
        HANDLE hThread = CreateThread(
            NULL,
            pool->stacksize,
            __threadpool_routine,
            pool,
            0,
            &threadId);

        if (hThread)
        {
            EnterCriticalSection(&pool->mutex);
            // 如果是第一个线程，保存句柄
            if (pool->nthreads == 0)
            {
                pool->tid = hThread;
            }
            else
            {
                CloseHandle(hThread); // 只跟踪一个线程（与原版行为一致）
            }
            pool->nthreads++;
            LeaveCriticalSection(&pool->mutex);
        }
        else
        {
            ret = -1;
            break;
        }
    }

    if (ret == 0)
        return 0;

    // 创建失败，终止线程池
    __threadpool_terminate(0, pool);
    return -1;
}

// 创建线程池
threadpool_t *threadpool_create(size_t nthreads, size_t stacksize)
{
    threadpool_t *pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    if (!pool)
        return NULL;

    // 创建任务队列
    pool->taskqueue = taskqueue_create(0, 0);
    if (!pool->taskqueue)
    {
        free(pool);
        return NULL;
    }

    // 初始化互斥锁和TLS
    InitializeCriticalSection(&pool->mutex);
    pool->key = TlsAlloc();
    if (pool->key == TLS_OUT_OF_INDEXES)
    {
        taskqueue_destroy(pool->taskqueue);
        free(pool);
        return NULL;
    }

    // 初始化线程池参数
    pool->stacksize = stacksize;
    pool->nthreads = 0;
    pool->tid = __zero_handle;
    pool->terminate = NULL;

    // 创建线程
    if (__threadpool_create_threads(nthreads, pool) < 0)
    {
        TlsFree(pool->key);
        taskqueue_destroy(pool->taskqueue);
        free(pool);
        return NULL;
    }

    return pool;
}

// 交换线程池的任务队列
void threadpool_swap_taskqueue(threadpool_t *pool, taskqueue_t *taskqueue)
{
    if (!taskqueue || !pool)
        return;

    EnterCriticalSection(&pool->mutex);

    // 保存原始参数
    size_t nthreads = pool->nthreads;
    size_t stacksize = pool->stacksize;
    HANDLE tid = pool->tid;

    // 创建新线程池结构
    threadpool_t *new_pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    if (!new_pool)
    {
        LeaveCriticalSection(&pool->mutex);
        return;
    }

    // 初始化新池
    new_pool->taskqueue = taskqueue;
    new_pool->stacksize = stacksize;
    new_pool->nthreads = 0;
    new_pool->tid = __zero_handle;
    new_pool->terminate = NULL;

    InitializeCriticalSection(&new_pool->mutex);
    new_pool->key = TlsAlloc();
    if (new_pool->key == TLS_OUT_OF_INDEXES)
    {
        free(new_pool);
        LeaveCriticalSection(&pool->mutex);
        return;
    }

    // 转移状态
    new_pool->tid = tid;

    // 标记旧池为终止状态
    pool->terminate = (CONDITION_VARIABLE *)1; // 特殊标记表示立即终止
    taskqueue_set_nonblock(pool->taskqueue);

    LeaveCriticalSection(&pool->mutex);

    // 销毁旧池（不处理pending任务）
    threadpool_destroy(NULL, pool);

    // 替换指针（需要调用方配合）
    *pool = *new_pool;
    free(new_pool);
}

// 调度任务到线程池
int threadpool_schedule(const struct threadpool_task *task, threadpool_t *pool)
{
    // 分配任务条目内存
    struct __threadpool_task_entry *entry = (struct __threadpool_task_entry *)malloc(sizeof(struct __threadpool_task_entry));
    if (!entry)
        return -1;

    // 复制任务数据并放入队列
    entry->task = *task;
    taskqueue_put(entry, pool->taskqueue);
    return 0;
}

// 检查当前线程是否在线程池中
int threadpool_in_pool(threadpool_t *pool)
{
    return TlsGetValue(pool->key) == (LPVOID)pool;
}

// 增加线程池线程数量
int threadpool_increase(threadpool_t *pool)
{
    DWORD threadId;
    HANDLE hThread;

    EnterCriticalSection(&pool->mutex);
    // 创建新线程
    hThread = CreateThread(
        NULL,
        pool->stacksize,
        __threadpool_routine,
        pool,
        0,
        &threadId);

    if (hThread)
    {
        // 如果是第一个线程，保存句柄
        if (pool->nthreads == 0)
            pool->tid = hThread;
        else
            CloseHandle(hThread); // 只跟踪一个线程

        pool->nthreads++;
        LeaveCriticalSection(&pool->mutex);
        return 0;
    }

    LeaveCriticalSection(&pool->mutex);
    return -1;
}

// 减少线程池线程数量
int threadpool_decrease(threadpool_t *pool)
{
    // 创建一个退出任务
    struct __threadpool_task_entry *entry = (struct __threadpool_task_entry *)malloc(sizeof(struct __threadpool_task_entry));
    if (!entry)
        return -1;

    // 设置退出任务参数
    entry->task.routine = __threadpool_exit_routine;
    entry->task.data = pool;
    // 将退出任务放入队列头部（优先执行）
    taskqueue_put_head(entry, pool->taskqueue);
    return 0;
}

// 退出线程池
void threadpool_exit(threadpool_t *pool)
{
    // 如果当前线程在线程池中，执行退出处理
    if (threadpool_in_pool(pool))
        __threadpool_exit_routine(pool);
}

// 销毁线程池
void threadpool_destroy(void (*pending)(const struct threadpool_task *), threadpool_t *pool)
{
    // 检查是否在池线程中调用
    int in_pool = threadpool_in_pool(pool);
    struct __threadpool_task_entry *entry;

    // 终止线程池
    __threadpool_terminate(in_pool, pool);

    // 处理队列中剩余的任务
    while (1)
    {
        entry = (struct __threadpool_task_entry *)taskqueue_get(pool->taskqueue);
        if (!entry)
            break;

        // 如果提供了pending回调且不是退出任务，则调用
        if (pending && entry->task.routine != __threadpool_exit_routine)
            pending(&entry->task);

        free(entry);
    }

    // 清理资源
    TlsFree(pool->key);
    DeleteCriticalSection(&pool->mutex);
    taskqueue_destroy(pool->taskqueue);

    // 如果不是在池线程中调用，释放池内存
    if (!in_pool)
        free(pool);
}