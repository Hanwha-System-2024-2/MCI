#ifndef DB_POOL_H
#define DB_POOL_H

#include <mysql/mysql.h>
#include <pthread.h>

#define DB_POOL_SIZE 10

typedef struct {
    MYSQL *connections[DB_POOL_SIZE];
    int available[DB_POOL_SIZE];
    pthread_mutex_t lock;
    pthread_cond_t cond;
} db_pool_t;

db_pool_t *init_db_pool(const char *host, const char *user, const char *password, const char *database);
void destroy_db_pool(db_pool_t *pool);

MYSQL *get_db_connection(db_pool_t *pool);
void release_db_connection(db_pool_t *pool, MYSQL *conn);

#endif 
