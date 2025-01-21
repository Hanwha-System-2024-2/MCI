#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

void handle_krx(int krx_sock, int pipe_write, int pipe_read) {
    char buffer[1024];
    while (1) {
        ssize_t recv_len = recv(krx_sock, buffer, sizeof(buffer) - 1, 0);
        if (recv_len <= 0) {
            perror("[KRX] Connection closed or error");
            break;
        }
        buffer[recv_len] = '\0';
        printf("[KRX] Received: %s\n", buffer);

        if (write(pipe_write, buffer, recv_len) == -1) {
            perror("[KRX] Failed to write to pipe");
            break;
        }

        ssize_t read_len = read(pipe_read, buffer, sizeof(buffer) - 1);
        if (read_len <= 0) {
            perror("[KRX] Failed to read from pipe");
            break;
        }
        buffer[read_len] = '\0';
        printf("[KRX] Response from OMS: %s\n", buffer);

        if (send(krx_sock, buffer, strlen(buffer), 0) == -1) {
            perror("[KRX] Send error");
            break;
        }
    }
    close(krx_sock);
    close(pipe_write);
    close(pipe_read);
    exit(EXIT_SUCCESS);
}
