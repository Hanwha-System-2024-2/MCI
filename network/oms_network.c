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
#include <mysql/mysql.h>
#include "../include/common.h"

#define BUFFER_SIZE 1024
#define LOGIN_SUCCESS 200
#define ID_NOT_FOUND 201
#define PASSWORD_MISMATCH 202
#define SERVER_ERROR 500

void handle_oms(MYSQL *conn, int oms_sock, int pipe_write, int pipe_read) {
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
    size_t buffer_offset = 0;

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, 2, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            ssize_t len = 0;

            if (events[i].data.fd == oms_sock) {
                len = recv(oms_sock, buffer + buffer_offset, BUFFER_SIZE - buffer_offset, 0);
                if (len == 0) {  // Client disconnected
                    printf("[MCI Server] Client disconnected\n");
                    close(oms_sock);
                    oms_sock = -1;
                    continue;
                }
            } else if (events[i].data.fd == pipe_read) {
                len = read(pipe_read, buffer + buffer_offset, BUFFER_SIZE - buffer_offset);
            } else {
                printf("[MCI Server] Unknown event source: fd = %d\n", events[i].data.fd);
                continue;
            }

            if (len < 0) {
                perror("Read error");
                continue;
            }

            buffer_offset += len;
            
            // Process all complete messages in the buffer
            size_t processed_bytes = 0;
            while (processed_bytes + sizeof(hdr) <= buffer_offset) {
                hdr *header = (hdr *)(buffer + processed_bytes);

                if (processed_bytes + header->length > buffer_offset) {
                    break;  // Wait for more data
                }

                void *body = buffer + processed_bytes;

                if (events[i].data.fd == oms_sock) {
                    printf("[OMS -> OMS-MCI] event occurs\n");
                    switch (header->tr_id) {
                        case OMQ_LOGIN:
                            handle_omq_login((omq_login *)body, oms_sock, conn);
                            break;
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
                processed_bytes += header->length;
            }

            // Shift unprocessed bytes to the beginning of the buffer
            if (processed_bytes < buffer_offset) {
                memmove(buffer, buffer + processed_bytes, buffer_offset - processed_bytes);
            }
            buffer_offset -= processed_bytes;
        }
    }

    close(epoll_fd);
    close(oms_sock);
    close(pipe_write);
    close(pipe_read);
    exit(EXIT_SUCCESS);
}


void handle_omq_login(omq_login *data, int oms_sock, MYSQL *conn) {
    printf("[OMQ_LOGIN] User ID: %s, Password: %s\n", data->user_id, data->user_pw);

    int status_code = validate_user_credentials(conn, data->user_id, data->user_pw);

    send_login_response(oms_sock, status_code);
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

int validate_user_credentials(MYSQL *conn, const char *user_id, const char *user_pw) {
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT user_pw FROM users WHERE user_id = '%s'", user_id);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "[validate_user_credentials] Query failed: %s\n", mysql_error(conn));
        return SERVER_ERROR;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (!result) {
        fprintf(stderr, "[validate_user_credentials] Failed to store result: %s\n", mysql_error(conn));
        return SERVER_ERROR;
    }

    int status_code;
    if (mysql_num_rows(result) == 0) { // no id
        status_code = ID_NOT_FOUND;
    } else {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row && strcmp(row[0], user_pw) == 0) { // success
            status_code = LOGIN_SUCCESS;
        } else { // not correct pw
            status_code = PASSWORD_MISMATCH;
        }
    }

    mysql_free_result(result);
    return status_code;
}

void send_login_response(int oms_sock, int status_code) {
    mot_login response;
    response.hdr.tr_id = MOT_LOGIN;
    response.hdr.length = sizeof(mot_login);
    response.status_code = status_code;

    if (send(oms_sock, &response, sizeof(response), 0) == -1) {
        perror("[send_login_response] Failed to send response to OMS");
    }
    
    printf("[send_login_response]: status code:%d\n", status_code);

}