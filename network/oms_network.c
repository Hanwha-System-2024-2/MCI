#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

void handle_oms(int oms_sock, int pipe_write, int pipe_read) {
    char buffer[1024];
    while (1) {
        ssize_t read_len = read(pipe_read, buffer, sizeof(buffer) - 1);
        if (read_len <= 0) {
            perror("[OMS] Failed to read from pipe");
            break;
        }
        buffer[read_len] = '\0';
        printf("[OMS] Received from KRX: %s\n", buffer);

        if (send(oms_sock, buffer, strlen(buffer), 0) == -1) {
            perror("[OMS] Send error");
            break;
        }

        ssize_t recv_len = recv(oms_sock, buffer, sizeof(buffer) - 1, 0);
        if (recv_len <= 0) {
            perror("[OMS] Connection closed or error");
            break;
        }
        buffer[recv_len] = '\0';
        printf("[OMS] Response from server: %s\n", buffer);

        if (write(pipe_write, buffer, recv_len) == -1) {
            perror("[OMS] Failed to write to pipe");
            break;
        }
    }
    close(oms_sock);
    close(pipe_write);
    close(pipe_read);
    exit(EXIT_SUCCESS);
}