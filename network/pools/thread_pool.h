#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

typedef struct thread_pool thread_pool_t;

thread_pool_t *create_thread_pool(int num_threads);

void submit_task(thread_pool_t *pool, void (*function)(void *), void *arg);

void destroy_thread_pool(thread_pool_t *pool);

#endif
