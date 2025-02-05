#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define TASK_QUEUE_SIZE 128

typedef struct task {
    void (*function)(void *);
    void *arg;
} task_t;

struct thread_pool {
    pthread_t *threads;
    task_t task_queue[TASK_QUEUE_SIZE]; 
    int head; 
    int tail; 
    int count;
    pthread_mutex_t lock;
    pthread_cond_t task_available;
    pthread_cond_t task_space_available;
    bool stop;
    int num_threads;
};

void *worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->lock);

        while (pool->count == 0 && !pool->stop) {
            pthread_cond_wait(&pool->task_available, &pool->lock);
        }

        if (pool->stop) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        task_t task = pool->task_queue[pool->head];
        pool->head = (pool->head + 1) % TASK_QUEUE_SIZE;
        pool->count--;

        pthread_cond_signal(&pool->task_space_available);
        pthread_mutex_unlock(&pool->lock);

        task.function(task.arg);
    }

    return NULL;
}

thread_pool_t *create_thread_pool(int num_threads) {
    thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (!pool) {
        perror("Failed to create thread pool");
        return NULL;
    }

    pool->num_threads = num_threads;
    pool->head = 0;
    pool->tail = 0;
    pool->count = 0;
    pool->stop = false;

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->task_available, NULL);
    pthread_cond_init(&pool->task_space_available, NULL);
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            perror("Failed to create thread");
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

void submit_task(thread_pool_t *pool, void (*function)(void *), void *arg) {
    pthread_mutex_lock(&pool->lock);

    while (pool->count == TASK_QUEUE_SIZE) {
        pthread_cond_wait(&pool->task_space_available, &pool->lock);
    }

    pool->task_queue[pool->tail].function = function;
    pool->task_queue[pool->tail].arg = arg;
    pool->tail = (pool->tail + 1) % TASK_QUEUE_SIZE;
    pool->count++;

    pthread_cond_signal(&pool->task_available);
    pthread_mutex_unlock(&pool->lock);
}

void destroy_thread_pool(thread_pool_t *pool) {
    pthread_mutex_lock(&pool->lock);
    pool->stop = true;
    pthread_cond_broadcast(&pool->task_available);
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    free(pool->threads);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->task_available);
    pthread_cond_destroy(&pool->task_space_available);
    free(pool);
}
