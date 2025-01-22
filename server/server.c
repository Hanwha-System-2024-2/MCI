#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "krx_network.h"
#include "oms_network.h"
#include "env.h"
#include <mysql/mysql.h>

int connect_db(MYSQL *conn);

int main() {
    int krx_sock, oms_sock;
    struct sockaddr_in krx_addr, oms_addr, client_addr;
    socklen_t client_len;

    // krx, oms 프로세스 간 통신을 위한 pipe
    int pipe_krx_to_oms[2];
    int pipe_oms_to_krx[2];
    if (pipe(pipe_krx_to_oms) == -1 || pipe(pipe_oms_to_krx) == -1) {
        perror("Failed to create pipes");
        exit(EXIT_FAILURE);
    }

    if ((krx_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("KRX socket creation failed");
        exit(EXIT_FAILURE);
    }

    krx_addr.sin_family = AF_INET;
    krx_addr.sin_port = htons(KRX_SERVER_PORT);
    krx_addr.sin_addr.s_addr = inet_addr(KRX_SERVER_IP);

    if (connect(krx_sock, (struct sockaddr *)&krx_addr, sizeof(krx_addr)) == -1) {
        perror("KRX connection failed");
        close(krx_sock);
        exit(EXIT_FAILURE);
    }

    pid_t krx_pid = fork();
    if (krx_pid == 0) {
        close(pipe_krx_to_oms[0]); 
        close(pipe_oms_to_krx[1]); 
        handle_kmt_price(krx_sock, pipe_krx_to_oms[1], pipe_oms_to_krx[0]);
    } else if (krx_pid < 0) {
        perror("Fork for KRX failed");
        close(krx_sock);
        exit(EXIT_FAILURE);
    }

    if ((oms_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("OMS socket creation failed");
        exit(EXIT_FAILURE);
    }

    oms_addr.sin_family = AF_INET;
    oms_addr.sin_port = htons(MCI_SERVER_PORT); // OMS 수신용 포트
    oms_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(oms_sock, (struct sockaddr *)&oms_addr, sizeof(oms_addr)) == -1) {
        perror("Bind failed for OMS");
        close(oms_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(oms_sock, 5) == -1) {
        perror("Listen failed for OMS");
        close(oms_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Server] Listening on port %d\n", MCI_SERVER_PORT);

    while (1) {
        client_len = sizeof(client_addr);
        int client_sock = accept(oms_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("Accept failed");
            continue;
        }

        printf("[OMS Server] Client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        pid_t oms_pid = fork();
        if (oms_pid == 0) {
            // 자식 프로세스: 클라이언트와 통신 처리
            close(pipe_krx_to_oms[1]);
            close(pipe_oms_to_krx[0]);
            close(oms_sock);

            // db 연결
            MYSQL *conn = mysql_init(NULL);
            if(connect_db(conn) == 1){ // 성공
                handle_oms(conn, client_sock, pipe_oms_to_krx[1], pipe_krx_to_oms[0]);
            }

            mysql_close(conn);
            
            close(client_sock);
            exit(0);
        } else if (oms_pid < 0) {
            perror("Fork for OMS failed");
            close(client_sock);
        }

        close(client_sock);
    }

    close(pipe_krx_to_oms[0]);
    close(pipe_krx_to_oms[1]);
    close(pipe_oms_to_krx[0]);
    close(pipe_oms_to_krx[1]);
    close(oms_sock);

    return 0;
}

int connect_db(MYSQL *conn){
    if (!mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DBNAME, 0, NULL, 0)) {
        fprintf(stderr, "MySQL connection failed: %s\n", mysql_error(conn));
        return -1;
    }

    return 1;
}