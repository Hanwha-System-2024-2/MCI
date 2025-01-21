#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "env.h"
#include "./network/oms_network.h"

#define BUFFER_SIZE 1024

void handle_server_response(int server_sock) {
    char buffer[BUFFER_SIZE];

    while (1) {
        ssize_t recv_len = recv(server_sock, buffer, BUFFER_SIZE, 0);
        if (recv_len <= 0) {
            perror("Receive failed or connection closed");
            break;
        }

        mot_stocks *data = (mot_stocks *)buffer;

        if (recv_len == sizeof(mot_login)) {
            mot_login *login = (mot_login *) buffer;
            printf("[OMS Client] Login Response: tr_id: %d, status code: %d\n", login->hdr.tr_id, login->status_code);
            continue;
        }

        printf("[OMS Client] Received TR_ID: %d from MCI Server\n", data->hdr.tr_id);
    }
}

void start_oms_client() {
    int client_sock;
    struct sockaddr_in server_addr;

    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MCI_SERVER_PORT); // Connect to MCI Server (Port 8081)
    server_addr.sin_addr.s_addr = inet_addr(MCI_SERVER_IP); // Replace with MCI Server IP

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection to MCI Server failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Client] Connected to MCI Server at %s:%d\n", MCI_SERVER_IP, MCI_SERVER_PORT);

    // Send a login request to MCI Server
    omq_login login;
    memset(&login, 0, sizeof(login));
    login.hdr.tr_id = 1; // Transaction ID for login
    strncpy(login.user_id, "example_user", sizeof(login.user_id) - 1);
    strncpy(login.user_pw, "example_password", sizeof(login.user_pw) - 1);

    if (send(client_sock, (void *)&login, sizeof(login), 0) == -1) {
        perror("[OMS Client] Send failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Client] Login request sent to MCI Server\n");

    // Handle server responses
    handle_server_response(client_sock);

    close(client_sock);
    printf("[OMS Client] Disconnected from MCI Server\n");
}

int main() {
    start_oms_client();
    return 0;
}
