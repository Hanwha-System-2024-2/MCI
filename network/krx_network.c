#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include "krx_network.h"
#include "oms_network.h"

#include <sys/epoll.h>

#define MAX_EVENTS 2     // 최대 감시 파일 디스크립터 개수
#define BUFFER_SIZE 1024 // 버퍼 사이즈

void print_kmt_current_market_prices(kmt_current_market_prices *data)
{
    printf("Transaction ID: %d\n", data->hdr.tr_id);
    printf("Length: %d bytes\n", data->hdr.length);
    printf("=== Market Prices ===\n");

    for (int i = 0; i < 4; i++)
    {
        printf("\n[Stock %d]\n", i + 1);
        printf("Stock Code: %s\n", data->body[i].stock_code);
        printf("Stock Name: %s\n", data->body[i].stock_name);
        printf("Price: %d\n", data->body[i].price);
        printf("Volume: %ld\n", data->body[i].volume);
        printf("Change: %d\n", data->body[i].change);
        printf("Rate of Change: %s\n", data->body[i].rate_of_change);
        printf("Hoga 1 - Type: %d, Price: %d, Balance: %d\n",
               data->body[i].hoga[0].trading_type,
               data->body[i].hoga[0].price,
               data->body[i].hoga[0].balance);
        printf("Hoga 2 - Type: %d, Price: %d, Balance: %d\n",
               data->body[i].hoga[1].trading_type,
               data->body[i].hoga[1].price,
               data->body[i].hoga[1].balance);
        printf("High Price: %d\n", data->body[i].high_price);
        printf("Low Price: %d\n", data->body[i].low_price);
        printf("Market Time: %s\n", data->body[i].market_time);
    }
}

void *handle_current_market_price(void *arg)
{
    int *args = (int *)arg;
    int krx_sock = args[0];
    int pipe_write = args[1];
    kmt_current_market_prices market_price;
    mot_market_price transformed;

    while (1)
    {
        printf("[Market Price Thread] Waiting for data from KRX...\n");
        // KRX에서 시세 데이터 수신
        ssize_t bytes_received = recv(krx_sock, &market_price, sizeof(market_price), 0);
        printf("[Market Price Thread] Received market data from KRX. Total Length: %ld bytes\n", bytes_received);

        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                printf("[Market Price Thread] Connection closed by KRX server.\n");
            }
            else
            {
                perror("[Market Price Thread] Failed to receive data");
            }
            break;
        }

        printf("[KRX] Received Data:\n");
        print_kmt_current_market_prices(&market_price); // 디버깅용 출력

        // 데이터 변환: kmt_current_market_prices -> mot_current_market_price
        transformed.hdr.tr_id = MOT_CURRENT_MARKET_PRICE;
        transformed.hdr.length = market_price.hdr.length;
        for (int i = 0; i < 4; i++)
        {
            transformed.body[i] = market_price.body[i]; // body 배열 복사
        }

        printf("[Market Price Thread] Data transformed successfully.\n");

        // 변환된 데이터를 MCI 내부 다른 프로세스에 전송
        if (write(pipe_write, &transformed, sizeof(transformed)) == -1)
        {
            perror("[Market Price Thread] Failed to write to pipe");
        }
        else
        {
            printf("[Market Price Thread] Wrote transformed data to pipe.\n");
        }

        printf("[Market Price Thread] Transformed data sent to pipe successfully.\n");
    }

    close(krx_sock);
    free(args);
    pthread_exit(NULL);
}

void *handle_stock_infos(void *arg)
{
    kmt_stock_infos *data = (kmt_stock_infos *)arg;

    printf("[Thread] Processing stock infos...\n");

    // IPC 처리 등 추가 로직 작성
    // ...

    free(data);         // 동적 메모리 해제
    pthread_exit(NULL); // 쓰레드 종료
}

// krx_sock 기반으로 데이터 판단해서 처리 진행
void handle_krx(int krx_sock, int pipe_write, int pipe_read)
{
    pthread_t market_thread;
    static int market_thread_initialized = 0;

    // 고정된 시세 처리 쓰레드 초기화
    if (!market_thread_initialized)
    {
        int *args = malloc(2 * sizeof(int));
        args[0] = krx_sock;
        args[1] = pipe_write;

        if (pthread_create(&market_thread, NULL, handle_current_market_price, args) != 0)
        {
            perror("Failed to create market price thread");
            free(args);
            exit(EXIT_FAILURE);
        }

        pthread_detach(market_thread); // 쓰레드 자동 해제
        market_thread_initialized = 1;
        printf("[KRX] Fixed market price thread initialized.\n"); // ?
    }

    // epoll 관측 구간
    int epoll_fd, nfds;
    struct epoll_event ev, events[MAX_EVENTS];
    char krx_buffer[BUFFER_SIZE];
    size_t krx_buffer_offset = 0;
    char pipe_buffer[BUFFER_SIZE];
    size_t pipe_buffer_offset = 0;

    // epoll 파일 디스크립터 생성
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    // KRX 소켓을 epoll에 추가
    ev.events = EPOLLIN;
    ev.data.fd = krx_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, krx_sock, &ev) == -1)
    {
        perror("epoll_ctl: krx_sock");
        exit(EXIT_FAILURE);
    }

    // 파이프 읽기 끝을 epoll에 추가
    ev.events = EPOLLIN;
    ev.data.fd = pipe_read;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_read, &ev) == -1)
    {
        perror("epoll_ctl: pipe_read");
        exit(EXIT_FAILURE);
    }

    printf("[KRX] epoll initialized and monitoring started.\n");

    while (1)
    {
        // epoll 대기
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("epoll_wait failed");
            break;
        }

        // 이벤트 처리
        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;

            // KRX 소켓에서 데이터 수신
            if (fd == krx_sock)
            {
                // 소켓에서 데이터 수신
                ssize_t bytes_received = recv(krx_sock, krx_buffer + krx_buffer_offset, BUFFER_SIZE - krx_buffer_offset, 0);
                if (bytes_received <= 0)
                {
                    perror("[KRX-Socket] Failed to receive data");
                    break;
                }

                krx_buffer_offset += bytes_received;

                // 버퍼 내 데이터 처리
                while (krx_buffer_offset >= sizeof(hdr))
                {
                    // 헤더 파싱
                    hdr *header = (hdr *)krx_buffer;
                    size_t total_length = sizeof(hdr) + header->length;

                    if (krx_buffer_offset < total_length)
                    {
                        // 데이터가 부족하면 다음 수신을 기다림
                        break;
                    }

                    // 요청 처리
                    int tr_id = header->tr_id;
                    printf("[KRX-Socket] Received request with tr_id: %d, length: %d\n", tr_id, header->length);

                    switch (tr_id)
                    {
                    case KMT_CURRENT_MARKET_PRICES:
                    {
                        printf("[KRX-Socket] Market price request handled by fixed thread.\n");
                        break;
                    }

                    case KMT_STOCK_INFOS:
                    {
                        void *request_data = malloc(header->length);
                        memcpy(request_data, krx_buffer + sizeof(hdr), header->length);

                        pthread_t request_thread;
                        if (pthread_create(&request_thread, NULL, handle_stock_infos, request_data) != 0)
                        {
                            perror("[KRX-Socket] Failed to create dynamic thread for socket");
                            free(request_data);
                            continue;
                        }
                        pthread_detach(request_thread);
                        break;
                    }

                    default:
                        printf("[KRX-Socket] Unknown tr_id: %d\n", tr_id);
                        break;
                    }

                    // 처리된 데이터 제거
                    memmove(krx_buffer, krx_buffer + total_length, krx_buffer_offset - total_length);
                    krx_buffer_offset -= total_length;
                }
            }

            // 파이프에서 데이터 수신
            else if (fd == pipe_read)
            {
                // 파이프에서 데이터 수신
                ssize_t bytes_received = read(pipe_read, pipe_buffer + pipe_buffer_offset, BUFFER_SIZE - pipe_buffer_offset);
                if (bytes_received <= 0)
                {
                    if (bytes_received == 0)
                        printf("[KRX-Pipe] Pipe closed by writer (EOF).\n");
                    else
                        perror("[KRX-Pipe] Failed to read data from pipe");
                    break;
                }

                pipe_buffer_offset += bytes_received;

                // 버퍼 내 데이터 처리
                while (pipe_buffer_offset >= sizeof(hdr))
                {
                    // 헤더 파싱
                    hdr *header = (hdr *)pipe_buffer;
                    size_t total_length = sizeof(hdr) + header->length;

                    if (pipe_buffer_offset < total_length)
                    {
                        // 데이터가 부족하면 다음 수신을 기다림
                        break;
                    }

                    // 요청 처리
                    int tr_id = header->tr_id;
                    printf("[KRX-Pipe] Received request with tr_id: %d, length: %d\n", tr_id, header->length);

                    switch (tr_id)
                    {
                    case KMT_STOCK_INFOS:
                        printf("[KRX-Pipe] Wait. Not ready to take data.\n");
                        break;

                    default:
                        printf("[KRX-Pipe] Unknown tr_id: %d\n", tr_id);
                        break;
                    }

                    // 처리된 데이터 제거
                    memmove(pipe_buffer, pipe_buffer + total_length, pipe_buffer_offset - total_length);
                    pipe_buffer_offset -= total_length;
                }
            }
        }
    }

    close(epoll_fd);
    close(krx_sock);
    close(pipe_read);
}
