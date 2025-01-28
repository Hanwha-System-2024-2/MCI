#ifndef OMS_HEADER_H
#define OMS_HEADER_H

#include <mysql/mysql.h>
#include "common.h" // 공통 헤더 포함

typedef struct {
    hdr hdr;
    char user_id[50];
    char user_pw[50];
} omq_login;

typedef struct {
    hdr hdr;
    char user_id[50];
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

typedef struct { // 거래 내역
	char stock_code[6];
	char stock_name[50];
	char tx_code[6];
	char user_id[50];
	char order_type;
	int quantity;
	char datetime[15];	// 'YYYYMMDDHHMMSS'
	int price;
	char status; // 체결 여부
} transaction;

typedef struct {
	hdr hdr;
    char user_id[50];
} omq_tx_history;

typedef struct {
	hdr hdr;
	transaction tx_history[50]; // 오래된 tx부터 최대 50개까지 리턴
} mot_tx_history;

void handle_oms(MYSQL *conn, int oms_sock, int pipe_write, int pipe_read);
void handle_omq_login(omq_login *data, int oms_sock, MYSQL *conn);
void handle_omq_stocks(omq_stocks *data, int pipe_write);
void handle_mot_stocks(mot_stocks *data, int oms_sock);
void handle_omq_market_price(omq_market_price *data, int pipe_write);
void handle_mot_market_price(mot_market_price *data, int oms_sock);
void send_login_response(int oms_sock,char *user_id, int status_code);
int validate_user_credentials(MYSQL *conn, const char *user_id, const char *user_pw);
void handle_omq_tx_history(omq_tx_history *data, int oms_sock, MYSQL *conn);

#endif