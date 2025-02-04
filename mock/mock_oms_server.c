#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "env.h"
#include "../network/oms_network.h"
#include "../network/krx_network.h"
#include "../include/common.h"

#define BUFFER_SIZE 8192

void send_login_request(int client_sock, const char *user_id, const char *user_pw) {
    omq_login login;
    memset(&login, 0, sizeof(login));

    login.hdr.tr_id = 1; // omq_login
    login.hdr.length = 108; // 길이 : 108
    strncpy(login.user_id, user_id, sizeof(login.user_id) - 1);
    strncpy(login.user_pw, user_pw, sizeof(login.user_pw) - 1);

    if (send(client_sock, (void *)&login, sizeof(login), 0) == -1) {
        perror("[OMS Client] Send login request failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf("[OMS Client] Sent login request: ID='%s', PW='%s'\n", user_id, user_pw);
}


void send_stock_info_request(int client_sock){
    omq_stock_infos req;

    req.hdr.tr_id = 2;
    req.hdr.length = sizeof(omq_stock_infos);

    if(send(client_sock, (void *) &req, sizeof(omq_stock_infos), 0) == -1){
        perror("[OMS Client] Send Stock infos request failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }
}

void send_hisotry_request(int client_sock, const char *user_id){
    omq_tx_history history;

    history.hdr.tr_id = 12;
    history.hdr.length = sizeof(omq_tx_history);
    strncpy(history.user_id, user_id, sizeof(history.user_id) - 1);

    if (send(client_sock, (void *)&history, sizeof(history), 0) == -1) {
        perror("[OMS Client] Send history request failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }
}

void print_tx(transaction *tx, int i){
    printf("Transaction %d: ", i + 1);
    // printf("  Stock Code: %s\n", tx->stock_code);
    // printf("  Stock Name: %s\n", tx->stock_name);
    // printf("  Transaction Code: %s\n", tx->tx_code);
    printf("  User ID: %s\n", tx->user_id);
    // printf("  Order Type: %c\n", tx->order_type);
    // printf("  Quantity: %d\n", tx->quantity);
    printf("  Date/Time: %s\n", tx->datetime);
    // printf("  Price: %d\n", tx->price);
    // printf("  Status: %c\n", tx->status);
    // printf("  Reject code: %s\n", tx->reject_code);
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
            printf("header->length: %d\n",header->length);
            if (processed_bytes + header->length > buffer_offset) {
                break;
            }

            if (header->tr_id == MOT_LOGIN) {
                mot_login *login = (mot_login *)(buffer + processed_bytes);
                printf("[OMS Client] Login Response: tr_id: %d, status code: %d, user_id: %s\n", login->hdr.tr_id, login->status_code, login->user_id);
            }
            else if(header->tr_id == MOT_TX_HISTORY){
                mot_tx_history *history = (mot_tx_history *)(buffer + processed_bytes);
                printf("[OMS Client] TX_history Response\n");

                for (int i = 0; i < 50; i++) {
                    transaction *tx = &history->tx_history[i];

                    if (tx->stock_code[0] == '\0') {
                        continue;
                    }

                    print_tx(tx, i);
                }
            }
            else if (header->tr_id == MOT_STOCK_INFOS) {
                mot_stock_infos *stockInfo = (mot_stock_infos *)(buffer + processed_bytes);
                printf("[OMS Client] stock_info Response\n");

                for (int i = 0; i < 4; i++) {
                    printf("Stock %d: code=%.6s, name=%s\n", i + 1,
                        stockInfo->body[i].stock_code,
                        stockInfo->body[i].stock_name
                    );
                }
            }
            else printf("[OMS Client] Received TR_ID: %d\n", header->tr_id);

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
    for(int i=0;i<1;i++){
        // send_login_request(client_sock, "hj", "1234"); // fail: 201
        // send_login_request(client_sock, "jina", "123"); // fail: 202
        // send_login_request(client_sock, "jina", "1234"); // success: 200
        // send_hisotry_request(client_sock, "jina");
        send_stock_info_request(client_sock);
        if(i% 10 == 0) usleep(500000);
    }

    send_stock_info_request(client_sock);
    
    printf("[OMS Client] Request sent to MCI Server\n");

    // Handle server responses
    handle_server_response(client_sock);

    close(client_sock);
    printf("[OMS Client] Disconnected from MCI Server\n");
}

int main() {
    start_oms_client();
    return 0;
}
