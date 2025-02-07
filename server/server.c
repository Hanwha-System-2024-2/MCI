#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <mysql/mysql.h>
#include <mqueue.h>

#include "krx_network.h"
#include "oms_network.h"
#include "env.h"

#define MAX_CLIENTS 100

int connect_db(MYSQL *conn);
int connect_to_krx();
void run_krx_client(int pipe_krx_to_oms[2], int pipe_oms_to_krx[2]);
void run_oms_server(int pipe_krx_to_oms[2], int pipe_oms_to_krx[2]);

int main() {
    int pipe_krx_to_oms[2];
    int pipe_oms_to_krx[2];

    if (pipe(pipe_krx_to_oms) == -1 || pipe(pipe_oms_to_krx) == -1) {
        perror("Failed to create pipes");
        exit(EXIT_FAILURE);
    }

    pid_t krx_pid = fork();
    if (krx_pid == 0) {
        // KRX 클라이언트 실행
        run_krx_client(pipe_krx_to_oms, pipe_oms_to_krx);
        exit(EXIT_SUCCESS);
    } else if (krx_pid > 0) {
        // OMS 서버 실행
        run_oms_server(pipe_krx_to_oms, pipe_oms_to_krx);
    } else {
        perror("Failed to fork KRX process");
        exit(EXIT_FAILURE);
    }

    close(pipe_krx_to_oms[0]);
    close(pipe_krx_to_oms[1]);
    close(pipe_oms_to_krx[0]);
    close(pipe_oms_to_krx[1]);

    return 0;
}
void run_oms_server(int pipe_krx_to_oms[2], int pipe_oms_to_krx[2]) {
    int oms_sock;
    struct sockaddr_in oms_addr;

    if ((oms_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("OMS socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    oms_addr.sin_family = AF_INET;
    oms_addr.sin_port = htons(MCI_SERVER_PORT);
    oms_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(oms_sock, (struct sockaddr *)&oms_addr, sizeof(oms_addr)) == -1) {
        perror("Bind failed for OMS");
        close(oms_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(oms_sock, MAX_CLIENTS) == -1) {
        perror("Listen failed for OMS");
        close(oms_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Server] Listening on port %d\n", MCI_SERVER_PORT);

    handle_oms(oms_sock, pipe_oms_to_krx[1], pipe_krx_to_oms[0]);

    close(oms_sock);
}

void cleanup_mq(int signo) {
    // 신호 발생 시 MQ 자원을 unlink하여 제거
    printf("[Cleanup] Signal %d received. Cleaning up message queue '%s'.\n", signo, MQ_NAME);
    mq_unlink(MQ_NAME);
    exit(EXIT_SUCCESS);
}

void run_krx_client(int pipe_krx_to_oms[2], int pipe_oms_to_krx[2]) {
    /* KRX 클라이언트 프로세스에서 signal handler 등록 */
    signal(SIGINT,  cleanup_mq);
    signal(SIGTERM, cleanup_mq);

    while (1) {
        int krx_sock = connect_to_krx();
        if (krx_sock != -1) {
            printf("[KRX Process] Connected to KRX server.\n");

            close(pipe_krx_to_oms[0]);
            close(pipe_oms_to_krx[1]);
            int status = handle_krx(krx_sock, pipe_krx_to_oms[1], pipe_oms_to_krx[0]);
            close(krx_sock);

            if (status == EXIT_FAILURE) {
                printf("[KRX Process] Connection lost. Attempting to reconnect...\n");
            }
        }
        sleep(5); // 5초마다 재연결
    }
}

int connect_db(MYSQL *conn) {
    if (!mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DBNAME, 0, NULL, 0)) {
        fprintf(stderr, "MySQL connection failed: %s\n", mysql_error(conn));
        return -1;
    }

    return 1;
}

int connect_to_krx() {
    int krx_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (krx_sock == -1) {
        perror("KRX socket creation failed");
        return -1;
    }

    struct sockaddr_in krx_addr;
    krx_addr.sin_family = AF_INET;
    krx_addr.sin_port = htons(KRX_SERVER_PORT);
    krx_addr.sin_addr.s_addr = inet_addr(KRX_SERVER_IP);

    if (connect(krx_sock, (struct sockaddr *)&krx_addr, sizeof(krx_addr)) == -1) {
        perror("KRX connection failed");
        close(krx_sock);
        return -1;
    }

    return krx_sock;
}
