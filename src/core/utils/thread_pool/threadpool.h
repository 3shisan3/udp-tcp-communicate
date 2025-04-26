/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
All rights reserved.
File:        threadpool.h
Version:     1.0
Author:      cjx
start date: 2024-12-31
Description: 基于pthread的线程池，c实现，提供相对完整的接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1            2024-12-31       cjx         create

*****************************************************************/

#ifndef PTHRDPOOL_H_
#define PTHRDPOOL_H_

#include <stddef.h>
#include <pthread.h>

struct taskqueue_t
{
    size_t queue_maxsize;      // 最大容纳任务数
    size_t taskNum;            // 记录着的任务数
    int linkoff;               // 任务链接时的偏移量
    bool nonblock;             // 队列未阻塞标识
    void *head1;               // 第一个消息头（list1）
    void *head2;               // 第二个消息头（list2）
    void **get_head;           // 获取消息的头指针
    void **put_head;           // 放入消息的头指针
    void **put_tail;           // 放入消息的尾指针
    pthread_mutex_t get_mutex; // 获取消息的互斥锁
    pthread_mutex_t put_mutex; // 放入消息的互斥锁
    pthread_cond_t get_cond;   // 获取消息的条件变量
    pthread_cond_t put_cond;   // 放入消息的条件变量
};

struct threadpool_t
{
    size_t nthreads;            // 线程池线程数
    size_t stacksize;           // 线程使用的栈大小（需要为非系统默认大小时）
    pthread_t tid;              // 线程id
    pthread_mutex_t mutex;      // 互斥锁
    pthread_key_t key;          // 线程私有数据识别标记
    pthread_cond_t *terminate;  // 线程间同步的条件变量（终止）
    pthread_cond_t *pause;      // 线程间同步的条件变量（暂停）(目前使用可直接定义为bool)
    taskqueue_t *taskqueue;     // 任务队列
};

struct threadpool_task
{
    void (*routine)(void *);    // 入参为void *无返回值的函数指针（例程）
    void *data;                 // 上函数的入参（由用户自定义入参）
};


///////////*//////////     api    //////////*///////////
#ifdef __cplusplus
extern "C"
{
#endif

// 创建任务队列（默认为阻塞模式，最糟情况记录2 * maxlen的任务（消费队列 + 生产队列））
taskqueue_t *taskqueue_create(size_t maxlen, int linkoff);
// 获取任务
void *taskqueue_get(taskqueue_t *queue);
// 放入任务
void taskqueue_put(void *msg, taskqueue_t *queue);
// 从队列头放入任务
void taskqueue_put_head(void *msg, taskqueue_t *queue);
// 队列设置为非阻塞模式（队列无限增长）
void taskqueue_set_nonblock(taskqueue_t *queue);
// 队列设置为阻塞模式（队列限制）
void taskqueue_set_block(taskqueue_t *queue);
// 销毁任务队列 
void taskqueue_destroy(taskqueue_t *queue);

// 创建线程池（系统默认分配栈，stacksize传0）)(绑定的任务队列默认为非阻塞)
threadpool_t *threadpool_create(size_t nthreads, size_t stacksize);
// 修改线程池绑定任务队列(taskqueue传null，不做任何操作)
void threadpool_swap_taskqueue(threadpool_t *pool, taskqueue_t *taskqueue);
// 添加任务
int threadpool_schedule(const struct threadpool_task *task, threadpool_t *pool);
// 检查当前线程
int threadpool_in_pool(threadpool_t *pool);
// 增加线程池线程数
int threadpool_increase(threadpool_t *pool);
// 减少线程池线程数
int threadpool_decrease(threadpool_t *pool);
// 退出线程池
void threadpool_exit(threadpool_t *pool);
// 销毁线程池（pending函数类似遗嘱任务，保证销毁线程前的一些必要操作，可传null）
void threadpool_destroy(void (*pending)(const struct threadpool_task *),
					    threadpool_t *pool);

#ifdef __cplusplus
}
#endif

#endif  // PTHRDPOOL_H_