#include "threadpool.h"

#include <errno.h>
#include <stdlib.h>


///////////*//////////     任务队列相关操作接口封装    //////////*///////////

// 设置非阻塞模式
void taskqueue_set_nonblock(taskqueue_t *queue)
{
	queue->nonblock = true;
	pthread_mutex_lock(&queue->put_mutex);
	pthread_cond_signal(&queue->get_cond);
	pthread_cond_broadcast(&queue->put_cond);
	pthread_mutex_unlock(&queue->put_mutex);
}
    // 设置阻塞模式
void taskqueue_set_block(taskqueue_t *queue)
{
	queue->nonblock = false;
}

// 创建任务队列
taskqueue_t *taskqueue_create(size_t maxlen, int linkoff)
{
	taskqueue_t *queue = (taskqueue_t *)malloc(sizeof (taskqueue_t));
	int ret;

	if (!queue)
		return NULL;

	ret = pthread_mutex_init(&queue->get_mutex, NULL);
	if (ret == 0)
	{
		ret = pthread_mutex_init(&queue->put_mutex, NULL);
		if (ret == 0)
		{
			ret = pthread_cond_init(&queue->get_cond, NULL);
			if (ret == 0)
			{
				ret = pthread_cond_init(&queue->put_cond, NULL);
				if (ret == 0)
				{
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

				pthread_cond_destroy(&queue->get_cond);
			}

			pthread_mutex_destroy(&queue->put_mutex);
		}

		pthread_mutex_destroy(&queue->get_mutex);
	}

	errno = ret;
	free(queue);
	return NULL;
}
// 销毁队列
void taskqueue_destroy(taskqueue_t *queue)
{
	pthread_cond_destroy(&queue->put_cond);
	pthread_cond_destroy(&queue->get_cond);
	pthread_mutex_destroy(&queue->put_mutex);
	pthread_mutex_destroy(&queue->get_mutex);
	free(queue);
}
// 放入任务
void taskqueue_put(void *task, taskqueue_t *queue)
{
	void **link = (void **)((char *)task + queue->linkoff);     // 计算任务链接指针

	*link = NULL;   // 将链接指针指向初始化为NULL，表示该任务还没有连接到任何其他任务
	pthread_mutex_lock(&queue->put_mutex);
	while (queue->taskNum > queue->queue_maxsize - 1 && !queue->nonblock)
		pthread_cond_wait(&queue->put_cond, &queue->put_mutex);	// 阻塞

	*queue->put_tail = link;    // 使put_tail指向link（即put_tail不在是队尾，而是link）
	queue->put_tail = link;     // 更新put_tail指针，指向新放入的任务（更新put_tail为队尾）
	queue->taskNum++;
	pthread_mutex_unlock(&queue->put_mutex);
	pthread_cond_signal(&queue->get_cond);
}
// 从队列头放入任务
void taskqueue_put_head(void *task, taskqueue_t *queue)
{
	void **link = (void **)((char *)task + queue->linkoff);

	pthread_mutex_lock(&queue->put_mutex);
	while (*queue->get_head)
	{
		if (pthread_mutex_trylock(&queue->get_mutex) == 0)
		{
			pthread_mutex_unlock(&queue->put_mutex);
			*link = *queue->get_head;
			*queue->get_head = link;
			pthread_mutex_unlock(&queue->get_mutex);
			return;
		}
	}

	while (queue->taskNum > queue->queue_maxsize - 1 && !queue->nonblock)
		pthread_cond_wait(&queue->put_cond, &queue->put_mutex);

	*link = *queue->put_head;
	if (*link == NULL)
		queue->put_tail = link;

	*queue->put_head = link;
	queue->taskNum++;
	pthread_mutex_unlock(&queue->put_mutex);
	pthread_cond_signal(&queue->get_cond);
}
// 交换任务队列（将head2的内容，swap到head1）
static size_t __taskqueue_swap(taskqueue_t *queue)
{
	void **get_head = queue->get_head;
	size_t cnt;

	pthread_mutex_lock(&queue->put_mutex);
	while (queue->taskNum == 0 && !queue->nonblock) // 等待任务添加
		pthread_cond_wait(&queue->get_cond, &queue->put_mutex);

	cnt = queue->taskNum;
	if (cnt > queue->queue_maxsize - 1) // 如果为阻塞模式，此时put处应该因栈满而wait
		pthread_cond_broadcast(&queue->put_cond);	// 后文开始消费，故通知可以put

	queue->get_head = queue->put_head;
	queue->put_head = get_head;
	queue->put_tail = get_head;
	queue->taskNum = 0;
	pthread_mutex_unlock(&queue->put_mutex);
	return cnt;
}
// 获取任务
void *taskqueue_get(taskqueue_t *queue)
{
	void *task;

	pthread_mutex_lock(&queue->get_mutex);
	if (*queue->get_head || __taskqueue_swap(queue) > 0)
	{   // swap调用场景，get的链表已被消费完（或初次get）
		task = (char *)*queue->get_head - queue->linkoff;
		*queue->get_head = *(void **)*queue->get_head;
	}
	else
		task = NULL;

	pthread_mutex_unlock(&queue->get_mutex);
	return task;
}


///////////*//////////      线程池相关操作接口封装     //////////*///////////

struct __threadpool_task_entry
{
	void *link;                         // 任务的链接
	struct threadpool_task task;        // 任务本身
};

static pthread_t __zero_tid;

// 线程退出时执行的函数。它会减少线程池中的线程计数，并在所有线程退出后通知主线程
static void __threadpool_exit_routine(void *context)
{
	threadpool_t *pool = (threadpool_t *)context;
	pthread_t tid;

	/* One thread joins another. Don't need to keep all thread IDs. */
	pthread_mutex_lock(&pool->mutex);
	tid = pool->tid;
	pool->tid = pthread_self();
	if (--pool->nthreads == 0 && pool->terminate)
		pthread_cond_signal(pool->terminate);

	pthread_mutex_unlock(&pool->mutex);
	if (!pthread_equal(tid, __zero_tid))
		pthread_join(tid, NULL);

	pthread_exit(NULL);
}
// 每个线程运行例程。它从消息队列中获取任务并执行，直到线程池被终止
static void *__threadpool_routine(void *arg)
{
	threadpool_t *pool = (threadpool_t *)arg;
	struct __threadpool_task_entry *entry;
	void (*task_routine)(void *);
	void *task_context;

	pthread_setspecific(pool->key, pool);
	while (!pool->terminate)
	{
		while (pool->pause && !pool->terminate)
		{
		}
			
		entry = (struct __threadpool_task_entry *)taskqueue_get(pool->taskqueue);
		if (!entry)
			break;

		task_routine = entry->task.routine;
		task_context = entry->task.data;
		free(entry);
		task_routine(task_context);		// 执行任务

		if (pool->nthreads == 0)
		{
			/* Thread pool was destroyed by the task. */
			free(pool);
			return NULL;
		}
	}

	__threadpool_exit_routine(pool);
	return NULL;
}
// 负责终止线程池的函数。它会等待所有线程结束，并确保资源得到释放。
static void __threadpool_terminate(int in_pool, threadpool_t *pool)
{
	pthread_cond_t term = PTHREAD_COND_INITIALIZER;

	pthread_mutex_lock(&pool->mutex);
	taskqueue_set_nonblock(pool->taskqueue);
	pool->terminate = &term;

	if (in_pool)
	{
		/* Thread pool destroyed in a pool thread is legal. */
		pthread_detach(pthread_self());
		pool->nthreads--;
	}

	while (pool->nthreads > 0)
		pthread_cond_wait(&term, &pool->mutex);

	pthread_mutex_unlock(&pool->mutex);
	if (!pthread_equal(pool->tid, __zero_tid))
		pthread_join(pool->tid, NULL);
}
// 根据指定的线程数量创建线程并初始化它们
static int __threadpool_create_threads(size_t nthreads, threadpool_t *pool)
{
	pthread_attr_t attr;
	pthread_t tid;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret == 0)
	{
		if (pool->stacksize)
			pthread_attr_setstacksize(&attr, pool->stacksize);

		while (pool->nthreads < nthreads)
		{
			ret = pthread_create(&tid, &attr, __threadpool_routine, pool);
			if (ret == 0)
				pool->nthreads++;
			else
				break;
		}

		pthread_attr_destroy(&attr);
		if (pool->nthreads == nthreads)
			return 0;

		__threadpool_terminate(0, pool);
	}

	errno = ret;
	return -1;
}

// 用户调用的函数，创建并初始化一个新的线程池，包括消息队列和互斥锁
threadpool_t *threadpool_create(size_t nthreads, size_t stacksize)
{
	threadpool_t *pool;
	int ret;

	pool = (threadpool_t *)malloc(sizeof (threadpool_t));
	if (!pool)
		return NULL;

	pool->taskqueue = taskqueue_create(0, 0);
	if (pool->taskqueue)
	{
		ret = pthread_mutex_init(&pool->mutex, NULL);
		if (ret == 0)
		{
			ret = pthread_key_create(&pool->key, NULL);
			if (ret == 0)
			{
				pool->stacksize = stacksize;
				pool->nthreads = 0;
				pool->tid = __zero_tid;
				pool->terminate = NULL;
				pool->pause = NULL;
				if (__threadpool_create_threads(nthreads, pool) >= 0)
					return pool;

				pthread_key_delete(pool->key);
			}

			pthread_mutex_destroy(&pool->mutex);
		}

		errno = ret;
		taskqueue_destroy(pool->taskqueue);
	}

	free(pool);
	return NULL;
}
// 重新绑定新的任务队列
void threadpool_swap_taskqueue(threadpool_t *pool, taskqueue_t *taskqueue)
{
	if (!taskqueue)
		return;

	// 未使用互斥锁阻塞其他位置操作，消费部分多个线程进行时捕获相同资源难梳理
	pthread_mutex_lock(&pool->mutex);

	pthread_cond_t term = PTHREAD_COND_INITIALIZER;
	pool->pause = &term;				// 暂停消费与生产
	// 使用pause的条件特性及锁的阻塞，而非while和下文的操作（代码美观），待后续todo
	taskqueue_t *old = pool->taskqueue;
	taskqueue_put(NULL, old);
	taskqueue_get(old);					// 通过taskqueue内部锁，确保其他线程暂停前执行任务完成
	
	pool->taskqueue = taskqueue;		// 替换任务队列
	taskqueue_destroy(old);
	pool->pause = NULL;

	pthread_mutex_unlock(&pool->mutex);
}
// 用户调用的函数，分配任务到线程池
int threadpool_schedule(const struct threadpool_task *task, threadpool_t *pool)
{
	void *buf = malloc(sizeof (struct __threadpool_task_entry));

	if (buf)
	{
		((struct __threadpool_task_entry *)buf)->task = *task;
		while (pool->pause)
		{
		}
		taskqueue_put(buf, pool->taskqueue);
		return 0;
	}

	return -1;
}
// 检查当前线程是否在线程池中
int threadpool_in_pool(threadpool_t *pool)
{
	return pthread_getspecific(pool->key) == pool;
}
// 增加线程池中的线程数量
int threadpool_increase(threadpool_t *pool)
{
	pthread_attr_t attr;
	pthread_t tid;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret == 0)
	{
		if (pool->stacksize)
			pthread_attr_setstacksize(&attr, pool->stacksize);

		pthread_mutex_lock(&pool->mutex);
		ret = pthread_create(&tid, &attr, __threadpool_routine, pool);
		if (ret == 0)
			pool->nthreads++;

		pthread_mutex_unlock(&pool->mutex);
		pthread_attr_destroy(&attr);
		if (ret == 0)
			return 0;
	}

	errno = ret;
	return -1;
}
// 减少线程池中的线程数量(通过添加一个停止当前线程的任务到线程池中，并减少线程池计数)
int threadpool_decrease(threadpool_t *pool)
{
	void *buf = malloc(sizeof (struct __threadpool_task_entry));
	struct __threadpool_task_entry *entry;

	if (buf)
	{
		entry = (struct __threadpool_task_entry *)buf;
		entry->task.routine = __threadpool_exit_routine;
		entry->task.data = pool;
		while (pool->pause)
		{
		}
		taskqueue_put_head(entry, pool->taskqueue);
		return 0;
	}

	return -1;
}
// 退出线程池
void threadpool_exit(threadpool_t *pool)
{
	if (threadpool_in_pool(pool))
		__threadpool_exit_routine(pool);
}
// 销毁线程池，处理挂起的任务并释放资源
void threadpool_destroy(void (*pending)(const threadpool_task *), threadpool_t *pool)
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

	pthread_key_delete(pool->key);
	pthread_mutex_destroy(&pool->mutex);
	taskqueue_destroy(pool->taskqueue);
	if (!in_pool)
		free(pool);
}