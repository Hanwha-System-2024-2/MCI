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

// #define KRX_SERVER_IP "127.0.0.1"
// #define KRX_SERVER_PORT 12345
// #define OMS_SERVER_IP "127.0.0.1"
// #define OMS_SERVER_PORT 54321

int main() {
    int krx_sock, oms_sock;
    struct sockaddr_in krx_addr, oms_addr;

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
        handle_krx(krx_sock, pipe_krx_to_oms[1], pipe_oms_to_krx[0]);
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
    oms_addr.sin_port = htons(OMS_SERVER_PORT);
    oms_addr.sin_addr.s_addr = inet_addr(OMS_SERVER_IP);

    if (connect(oms_sock, (struct sockaddr *)&oms_addr, sizeof(oms_addr)) == -1) {
        perror("OMS connection failed");
        close(oms_sock);
        exit(EXIT_FAILURE);
    }

    pid_t oms_pid = fork();
    if (oms_pid == 0) {
        close(pipe_krx_to_oms[1]); 
        close(pipe_oms_to_krx[0]); 
        handle_oms(oms_sock, pipe_oms_to_krx[1], pipe_krx_to_oms[0]);
    } else if (oms_pid < 0) {
        perror("Fork for OMS failed");
        close(oms_sock);
        exit(EXIT_FAILURE);
    }

    close(pipe_krx_to_oms[0]);
    close(pipe_krx_to_oms[1]);
    close(pipe_oms_to_krx[0]);
    close(pipe_oms_to_krx[1]);

    int status;
    while (wait(&status) > 0);

    printf("All child processes terminated. Server exiting.\n");
    return 0;
}
