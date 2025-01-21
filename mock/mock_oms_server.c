#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "env.h"

#define BUFFER_SIZE 1024

void start_oms_server() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(OMS_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(OMS_SERVER_IP);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) == -1) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Server] Listening on %s:%d\n", OMS_SERVER_IP, OMS_SERVER_PORT);

    client_len = sizeof(client_addr);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("Accept failed");
            continue;
        }

        printf("[OMS Server] Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        while (1) {
            ssize_t recv_len = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if (recv_len <= 0) {
                perror("Receive failed or connection closed");
                break;
            }

            buffer[recv_len] = '\0';
            printf("[OMS Server] Received: %s\n", buffer);

            const char *response = "Message received by OMS Server.";
            if (send(client_sock, response, strlen(response), 0) == -1) {
                perror("Send failed");
                break;
            }
        }

        close(client_sock);
        printf("[OMS Server] Client disconnected.\n");
    }

    close(server_sock);
}

int main() {
    start_oms_server();
    return 0;
}
