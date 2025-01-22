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

void send_login_request(int client_sock, const char *user_id, const char *user_pw) {
    omq_login login;
    memset(&login, 0, sizeof(login));

    login.hdr.tr_id = 1; // omq_login
    login.hdr.length = 108; // 길이
    strncpy(login.user_id, user_id, sizeof(login.user_id) - 1);
    strncpy(login.user_pw, user_pw, sizeof(login.user_pw) - 1);

    if (send(client_sock, (void *)&login, sizeof(login), 0) == -1) {
        perror("[OMS Client] Send failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Client] Sent login request: ID='%s', PW='%s'\n", user_id, user_pw);
}
void handle_server_response(int server_sock) {
    char buffer[BUFFER_SIZE];
    size_t buffer_offset = 0;

    while (1) {
        ssize_t recv_len = recv(server_sock, buffer + buffer_offset, BUFFER_SIZE - buffer_offset, 0);
        if (recv_len <= 0) {
            perror("Receive failed or connection closed");
            break;
        }

        buffer_offset += recv_len;

        size_t processed_bytes = 0;
        while (processed_bytes + sizeof(hdr) <= buffer_offset) {
            hdr *header = (hdr *)(buffer + processed_bytes);

            if (processed_bytes + header->length > buffer_offset) {
                break;
            }

            if (header->tr_id == MOT_LOGIN) {
                mot_login *login = (mot_login *)(buffer + processed_bytes);
                printf("[OMS Client] Login Response: tr_id: %d, status code: %d\n", login->hdr.tr_id, login->status_code);
            } else {
                printf("[OMS Client] Received TR_ID: %d\n", header->tr_id);
            }

            processed_bytes += header->length;
        }

        if (processed_bytes < buffer_offset) {
            memmove(buffer, buffer + processed_bytes, buffer_offset - processed_bytes);
        }
        buffer_offset -= processed_bytes;
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
    server_addr.sin_port = htons(MCI_SERVER_PORT); 
    server_addr.sin_addr.s_addr = inet_addr(MCI_SERVER_IP);

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection to MCI Server failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Client] Connected to MCI Server at %s:%d\n", MCI_SERVER_IP, MCI_SERVER_PORT);

    // TEST 성공
    for(int i=0;i<100;i++){
        send_login_request(client_sock, "hj", "1234"); // fail: 201
        send_login_request(client_sock, "jina", "123"); // fail: 202
        send_login_request(client_sock, "jina", "1234"); // success: 200
        if(i% 10 == 0) usleep(500000);
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
