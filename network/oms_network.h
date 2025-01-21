#ifndef OMS_HEADER_H
#define OMS_HEADER_H

#include "common.h" // 공통 헤더 포함

typedef struct {
    hdr hdr;
    char user_id[50];
    char user_pw[50];
} omq_login;

typedef struct {
    hdr hdr;
    int status_code;
} mot_login;

typedef struct {
    hdr hdr;
} omq_stocks;

typedef struct {
    hdr hdr;
    stock_info body[4]; // kmt_stock_infos 구조체에서 가져온 stock_info[4]
} mot_stocks;

typedef struct {
    hdr hdr;
} omq_market_price;

typedef struct {
    hdr hdr;
    current_market_price body[4]; // kmt_current_market_prices 구조체에서 current_market_price[4]
} mot_market_price;

void handle_oms(int oms_sock, int pipe_write, int pipe_read);
void handle_omq_login(omq_login *data, int oms_sock);
void handle_omq_stocks(omq_stocks *data, int pipe_write);
void handle_mot_stocks(mot_stocks *data, int oms_sock);
void handle_omq_market_price(omq_market_price *data, int pipe_write);
void handle_mot_market_price(mot_market_price *data, int oms_sock);

#endif