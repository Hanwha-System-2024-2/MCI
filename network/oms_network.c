#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "oms_network.h"
#include "../include/common.h"

#define BUFFER_SIZE 1024

void handle_oms(int oms_sock, int pipe_write, int pipe_read) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[2];
    ev.events = EPOLLIN;

    ev.data.fd = oms_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, oms_sock, &ev) == -1) {
        perror("epoll_ctl: oms_sock");
        exit(EXIT_FAILURE);
    }

    ev.data.fd = pipe_read;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_read, &ev) == -1) {
        perror("epoll_ctl: pipe_read");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, 2, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            ssize_t len = 0;
            
            if (events[i].data.fd == oms_sock) {
                printf("event from oms\n");
                len = recv(oms_sock, buffer, BUFFER_SIZE, 0);

                if (len == 0) {  // 클라이언트 연결 종료: 리소스 정리
                    printf("[OMS Server] Client disconnected\n");
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, oms_sock, NULL) == -1) {
                        perror("epoll_ctl: EPOLL_CTL_DEL failed");
                    }
                    close(oms_sock);
                    oms_sock = -1;  
                    continue;
                }
            } else if (events[i].data.fd == pipe_read) {
                printf("event from pipe\n");
                len = read(pipe_read, buffer, BUFFER_SIZE);
            } else {
                printf("event from ?\n");
            }
            
            if (len < 0) {  // 에러 처리
                perror("Read error");
                continue;
            }

            hdr *header = (hdr *)buffer;
            void *body = buffer;

            if (events[i].data.fd == oms_sock) {
                printf("[OMS -> OMS-MCI] event occurs\n");
                switch (header->tr_id) {
                    case OMQ_LOGIN: {
                        handle_omq_login((omq_login *)body, oms_sock);
                        break;
                    }
                    case OMQ_STOCK_INFOS:
                        handle_omq_stocks((omq_stocks *)body, pipe_write);
                        break;
                    case OMQ_CURRENT_MARKET_PRICE:
                        handle_omq_market_price((omq_market_price *)body, pipe_write);
                        break;
                    default:
                        printf("[ERROR] Unknown TR_ID from OMS socket: %d\n", header->tr_id);
                        break;
                }
            } else if (events[i].data.fd == pipe_read) {
                printf("[OMS-KRX -> OMS-MCI] event occurs\n");
                switch (header->tr_id) {
                    case MOT_STOCK_INFOS:
                        handle_mot_stocks((mot_stocks *)body, oms_sock);
                        break;
                    case MOT_CURRENT_MARKET_PRICE:
                        handle_mot_market_price((mot_market_price *)body, oms_sock);
                        break;
                    default:
                        printf("[ERROR] Unknown TR_ID from pipe: %d\n", header->tr_id);
                        break;
                }
            }
        }
    }

    close(epoll_fd);
    close(oms_sock);
    close(pipe_write);
    close(pipe_read);
    exit(EXIT_SUCCESS);
}

void handle_omq_login(omq_login *data, int oms_sock) {
    printf("[OMQ_LOGIN] User ID: %s, Password: %s\n", data->user_id, data->user_pw);

    /*
        TODO: DB 처리 하기 - Validate user credentials
    */

    mot_login response;
    response.hdr.tr_id = MOT_LOGIN;
    response.hdr.length = sizeof(mot_login);
    response.status_code = 200;

    if (send(oms_sock, &response, sizeof(response), 0) == -1) {
        perror("[OMQ_LOGIN] Failed to send response to OMS");
    }
}

void handle_omq_stocks(omq_stocks *data, int pipe_write) {
    printf("[OMQ_STOCKS] Forwarding request to KRX process\n");

    if (write(pipe_write, data, sizeof(omq_stocks)) == -1) {
        perror("[OMQ_STOCKS] Failed to write to pipe");
    }
}

void handle_mot_stocks(mot_stocks *data, int oms_sock) {
    printf("[MOT_STOCKS] Forwarding stock info to OMS socket\n");

    if (send(oms_sock, data, sizeof(mot_stocks), 0) == -1) {
        perror("[MOT_STOCKS] Failed to send to OMS socket");
    }
}

void handle_omq_market_price(omq_market_price *data, int pipe_write) {
    printf("[OMQ_MARKET_PRICE] Forwarding request to KRX process\n");

    if (write(pipe_write, data, sizeof(omq_market_price)) == -1) {
        perror("[OMQ_MARKET_PRICE] Failed to write to pipe");
    }
}

void handle_mot_market_price(mot_market_price *data, int oms_sock) {
    printf("[MOT_MARKET_PRICE] Forwarding market price to OMS socket\n");

    if (send(oms_sock, data, sizeof(mot_market_price), 0) == -1) {
        perror("[MOT_MARKET_PRICE] Failed to send to OMS socket");
    }
}