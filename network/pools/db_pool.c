#include "db_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql/mysql.h>

db_pool_t *init_db_pool(const char *host, const char *user, const char *password, const char *database) {
    db_pool_t *pool = (db_pool_t *)malloc(sizeof(db_pool_t));
    if (!pool) {
        perror("Failed to allocate memory for DB pool");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);

    for (int i = 0; i < DB_POOL_SIZE; i++) {
    pool->connections[i] = mysql_init(NULL);
    if (!pool->connections[i]) {
        fprintf(stderr, "MySQL initialization failed\n");
        continue;
    }

    if (!mysql_real_connect(pool->connections[i], host, user, password, database, 0, NULL, 0)) {
        fprintf(stderr, "MySQL connection failed (Error Code: %d): %s\n", 
                mysql_errno(pool->connections[i]), mysql_error(pool->connections[i]));
        mysql_close(pool->connections[i]); 
        pool->connections[i] = NULL; 
        continue;
    }

    pool->available[i] = 1;
}


    return pool;
}

MYSQL *get_db_connection(db_pool_t *pool) {
    pthread_mutex_lock(&pool->lock);

    while (1) {
        for (int i = 0; i < DB_POOL_SIZE; i++) {
            if (pool->available[i]) {
                pool->available[i] = 0;
                pthread_mutex_unlock(&pool->lock);
                return pool->connections[i];
            }
        }
        pthread_cond_wait(&pool->cond, &pool->lock);
    }
}

void release_db_connection(db_pool_t *pool, MYSQL *conn) {
    pthread_mutex_lock(&pool->lock);

    for (int i = 0; i < DB_POOL_SIZE; i++) {
        if (pool->connections[i] == conn) {
            pool->available[i] = 1;
            break;
        }
    }

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
}

void destroy_db_pool(db_pool_t *pool) {
    for (int i = 0; i < DB_POOL_SIZE; i++) {
        if (pool->connections[i]) {
            mysql_close(pool->connections[i]);
        }
    }

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    free(pool);
}
