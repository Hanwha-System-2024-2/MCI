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
#include "krx_network.h"
#include <mysql/mysql.h>
#include <pthread.h>
#include "../include/common.h"

#define BUFFER_SIZE 8192
#define LOGIN_SUCCESS 200
#define ID_NOT_FOUND 201
#define PASSWORD_MISMATCH 202
#define SERVER_ERROR 500
#define MAX_CLIENTS 100

// 클라이언트 소켓 관리
int oms_clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t client_list_mutex = PTHREAD_MUTEX_INITIALIZER;

char buffer[BUFFER_SIZE];
size_t buffer_offset = 0;

void handle_oms(MYSQL *conn, int oms_sock, int pipe_write, int pipe_read) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[MAX_CLIENTS + 2];
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


    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_CLIENTS + 2, -1); 
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            ssize_t len = 0;

            if (events[i].data.fd == oms_sock) { // 새 클라이언트 연결 처리
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                int client_sock = accept(oms_sock, (struct sockaddr *)&client_addr, &client_len);
                if (client_sock == -1) {
                    perror("Accept failed");
                    continue;
                }

                printf("[MCI Server] Client connected: %s:%d\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port));

                add_client(client_sock, epoll_fd);
                
            } else if (events[i].data.fd  == pipe_read) {
                len = read(pipe_read, buffer + buffer_offset, BUFFER_SIZE - buffer_offset);
            } else { // 클라이언트 데이터 수신
                len = recv(events[i].data.fd, buffer + buffer_offset, BUFFER_SIZE - buffer_offset, 0);
                if (len <= 0) {
                    remove_client(events[i].data.fd, epoll_fd);
                    continue;
                }
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
                    printf("[Connection]\n");
                } else if (events[i].data.fd == pipe_read) {
                    printf("[OMS-KRX -> OMS-MCI] event occurs\n");
                    switch (header->tr_id) {
                        case MOT_STOCK_INFOS:
                            handle_mot_stock_infos((mpt_stock_infos *)body);
                            break;
                        case MOT_CURRENT_MARKET_PRICE:
                            handle_mot_market_price((mot_market_price *)body);
                            break;
                        default:
                            printf("[ERROR] Unknown TR_ID from pipe: %d\n", header->tr_id);
                            break;
                    }
                }
                else{
                    printf("[OMS -> OMS-MCI] event occurs\n");
                    switch (header->tr_id) {
                        case OMQ_LOGIN:
                            handle_omq_login((omq_login *)body, events[i].data.fd, conn);
                            break;
                        case OMQ_TX_HISTORY:
                            handle_omq_tx_history((omq_tx_history *) body, events[i].data.fd, conn);
                            break;
                        case OMQ_STOCK_INFOS:
                            handle_omq_stock_infos((omq_stock_infos *)body, pipe_write, events[i].data.fd);
                            break;
                        case OMQ_CURRENT_MARKET_PRICE:
                            handle_omq_market_price((omq_market_price *)body, pipe_write);
                            break;
                        default:
                            printf("[ERROR] Unknown TR_ID from OMS socket: %d\n", header->tr_id);
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

    send_login_response(oms_sock, data->user_id, status_code);
}

void handle_omq_tx_history(omq_tx_history *data, int oms_sock, MYSQL *conn) {
    printf("[OMQ_TX_HISTORY] request id: %s\n", data->user_id);

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT stock_code, stock_name, transaction_code, user_id, order_type, quantity, reject_code, "
             "DATE_FORMAT(order_time, '%%Y%%m%%d%%H%%i%%s') AS datetime, price, status "
             "FROM tx_history "
             "WHERE user_id = '%s' "
             "LIMIT 50", data->user_id);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "[handle_omq_tx_history] Query failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (!result) {
        fprintf(stderr, "[handle_omq_tx_history] Failed to store result: %s\n", mysql_error(conn));
        return;
    }

    mot_tx_history response;
    memset(&response, 0, sizeof(response)); 

    response.hdr.tr_id = 13;
    
    int row_count = 0;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result)) && row_count < 50) {
        strncpy(response.tx_history[row_count].stock_code, row[0], sizeof(response.tx_history[row_count].stock_code) - 1);
        response.tx_history[row_count].stock_code[sizeof(response.tx_history[row_count].stock_code) - 1] = '\0';

        strncpy(response.tx_history[row_count].stock_name, row[1], sizeof(response.tx_history[row_count].stock_name) - 1);
        response.tx_history[row_count].stock_name[sizeof(response.tx_history[row_count].stock_name) - 1] = '\0';

        strncpy(response.tx_history[row_count].tx_code, row[2], sizeof(response.tx_history[row_count].tx_code) - 1);
        response.tx_history[row_count].tx_code[sizeof(response.tx_history[row_count].tx_code) - 1] = '\0';

        strncpy(response.tx_history[row_count].user_id, row[3], sizeof(response.tx_history[row_count].user_id) - 1);
        response.tx_history[row_count].user_id[sizeof(response.tx_history[row_count].user_id) - 1] = '\0';

        response.tx_history[row_count].order_type = row[4][0];
        response.tx_history[row_count].quantity = atoi(row[5]);
        
        strncpy(response.tx_history[row_count].reject_code, row[6], sizeof(response.tx_history[row_count].reject_code) - 1);
        response.tx_history[row_count].reject_code[sizeof(response.tx_history[row_count].reject_code) - 1] = '\0';

        strncpy(response.tx_history[row_count].datetime, row[7], sizeof(response.tx_history[row_count].datetime) - 1);
        response.tx_history[row_count].datetime[sizeof(response.tx_history[row_count].datetime) - 1] = '\0';

        response.tx_history[row_count].price = atoi(row[8]);
        response.tx_history[row_count].status = row[9][0];
        
        // printf("[TX_HISTORY] #%d Stock Code: %s, Name: %s, Tx Code: %s, User: %s, "
        //    "Order Type: %c, Quantity: %d, Reject Code: %s, Datetime: %s, Status: %c\n",
        //    row_count + 1,
        //    response.tx_history[row_count].stock_code,
        //    response.tx_history[row_count].stock_name,
        //    response.tx_history[row_count].tx_code,
        //    response.tx_history[row_count].user_id,
        //    response.tx_history[row_count].order_type,
        //    response.tx_history[row_count].quantity,
        //    response.tx_history[row_count].reject_code,
        //    response.tx_history[row_count].datetime,
        //    response.tx_history[row_count].status);
        
        row_count++;
    }

    response.hdr.length = sizeof(response);

    // Send the response to the OMS
    if (send(oms_sock, &response, sizeof(response), 0) == -1) {
        perror("[handle_omq_tx_history] Failed to send response to OMS");
    } else {
        printf("[handle_omq_tx_history]: Sent %d transaction records to OMS\n", row_count);
    }

    mysql_free_result(result);
}

void handle_omq_stock_infos(omq_stock_infos *data, int pipe_write, int oms_sock) {
    printf("[OMQ_STOCK_INFOS] Forwarding request to KRX process from oms_sock: %d\n", oms_sock);
    
    mpq_stock_infos stockInfo;
    stockInfo.oms_sock = oms_sock;
    stockInfo.omq_stock_infos = *data; // 요청 데이터 복사

    printf("[omq_stock_infos] OMS_SOCK = %d\n", oms_sock);

    if (write(pipe_write, &stockInfo, sizeof(stockInfo)) == -1) {
        perror("[OMQ_STOCK_INFOS] Failed to write to pipe");
    }
}

void handle_mot_stock_infos(mpt_stock_infos *data) {
    printf("[MOT_STOCK_INFOS] Sending stock info to requesting OMS client: %d\n", data->oms_sock);
    
    int oms_sock = data->oms_sock;
    mot_stock_infos stockInfo = data->mot_stock_infos;
    
    if (send(oms_sock, &stockInfo, sizeof(stockInfo), 0) == -1) {
        perror("[MOT_STOCK_INFOS] Failed to send stock info to OMS client");
    }
}

void handle_omq_market_price(omq_market_price *data, int pipe_write) {
    printf("[OMQ_MARKET_PRICE] Forwarding request to KRX process\n");

    if (write(pipe_write, data, sizeof(omq_market_price)) == -1) {
        perror("[OMQ_MARKET_PRICE] Failed to write to pipe");
    }
}

void handle_mot_market_price(mot_market_price *data) {
    printf("[MOT_MARKET_PRICE] Broadcasting market price to all OMS clients\n");

    broadcast_to_clients(data, sizeof(mot_market_price));
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

void send_login_response(int oms_sock,char *user_id, int status_code) {
    mot_login response;
    response.hdr.tr_id = MOT_LOGIN;
    response.hdr.length = sizeof(mot_login); // // 62 + 2(패딩)
    strncpy(response.user_id, user_id, sizeof(response.user_id) - 1);
    response.status_code = status_code;

    if (send(oms_sock, &response, sizeof(response), 0) == -1) {
        perror("[send_login_response] Failed to send response to OMS");
    }
    
    printf("[send_login_response]: status code:%d\n", status_code);

}

void add_client(int client_sock, int epoll_fd) {
    pthread_mutex_lock(&client_list_mutex);
    if (client_count < MAX_CLIENTS) {
        oms_clients[client_count++] = client_sock;

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = client_sock;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev) == -1) {
            perror("[add_client] epoll_ctl add failed");
        } else {
            printf("[add_client] Added client socket %d\n", client_sock);
            printf("[add_client] Current OMS Client = %d\n",client_count);
        }
    } else {
        printf("[add_client Failed] Max clients reached. Closing socket %d\n", client_sock);
        close(client_sock);
    }
    pthread_mutex_unlock(&client_list_mutex);
}

void remove_client(int client_sock, int epoll_fd) {
    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < client_count; i++) {
        if (oms_clients[i] == client_sock) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_sock, NULL);
            close(client_sock);

            oms_clients[i] = oms_clients[--client_count];
            printf("[remove_client] Removed client socket %d\n", client_sock);
            printf("[remove_client] Current OMS Client = %d\n",client_count);
            break;
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}

void broadcast_to_clients(void *data, size_t data_size) {
    pthread_mutex_lock(&client_list_mutex);
    for (int i = 0; i < client_count; i++) {
        if (send(oms_clients[i], data, data_size, 0) == -1) {
            perror("[broadcast_to_clients] Failed to send to client");
            remove_client(oms_clients[i], 0);
        }
    }
    pthread_mutex_unlock(&client_list_mutex);
}
