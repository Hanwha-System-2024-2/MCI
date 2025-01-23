#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "../network/krx_network.h"
#include "env.h"

#define BUFFER_SIZE 1024

void format_current_time(char *buffer)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, 15, "%Y%m%d%H%M%S", t); // "yyyymmddhhmmss"
}

void send_data(int client_socket)
{
    kmt_current_market_prices data;

    // 데이터 초기화
    data.hdr.tr_id = 8;
    data.hdr.length = sizeof(data);

    // 데이터 예시
    for (int i = 0; i < 4; i++)
    {
        sprintf(data.body[i].stock_code, "%06d", i + 1); // 예: "000001"
        sprintf(data.body[i].stock_name, "Stock%d", i + 1);
        data.body[i].price = 1000.0 + i * 100;
        data.body[i].volume = 1000 * (i + 1);
        data.body[i].change = i * 10;
        sprintf(data.body[i].rate_of_change, "+%d.0%%", i);
        data.body[i].hoga[0].trading_type = 1;
        data.body[i].hoga[0].price = 1000 + i * 10;
        data.body[i].hoga[0].balance = 100;
        data.body[i].hoga[1].trading_type = 2;
        data.body[i].hoga[1].price = 1100 + i * 10;
        data.body[i].hoga[1].balance = 200;
        data.body[i].high_price = 1200 + i * 10;
        data.body[i].low_price = 900 + i * 10;
        format_current_time(data.body[i].market_time);
    }

    // 데이터 전송
    if (send(client_socket, &data, sizeof(data), 0) < 0)
    {
        perror("Failed to send data");
    }
}

void start_krx_server_send_version()
{
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR 설정
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(KRX_SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connections...\n");

    client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket < 0)
    {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    while (1)
    {
        send_data(client_socket);
        sleep(5); // 5초마다 데이터 전송
    }

    close(client_socket);
    close(server_fd);
}

void start_krx_server()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(KRX_SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(KRX_SERVER_IP);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) == -1)
    {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("[KRX Server] Listening on %s:%d\n", KRX_SERVER_IP, KRX_SERVER_PORT);

    client_len = sizeof(client_addr);

    while (1)
    {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1)
        {
            perror("Accept failed");
            continue;
        }

        printf("[KRX Server] Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        while (1)
        {
            ssize_t recv_len = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if (recv_len <= 0)
            {
                perror("Receive failed or connection closed");
                break;
            }

            buffer[recv_len] = '\0';
            printf("[KRX Server] Received: %s\n", buffer);

            const char *response = "Message received by KRX Server.";
            if (send(client_sock, response, strlen(response), 0) == -1)
            {
                perror("Send failed");
                break;
            }
        }

        close(client_sock);
        printf("[KRX Server] Client disconnected.\n");
    }

    close(server_sock);
}

int main()
{
    // start_krx_server();
    start_krx_server_send_version();
    return 0;
}
