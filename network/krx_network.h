#ifndef KRX_HEADER_H
#define KRX_HEADER_H

#include "../include/common.h" // 공통 헤더 포함

// 종목 정보 요청 구조체
typedef struct {
	hdr hdr;
} mkq_stock_infos;

// 종목 정보 요청 변환 구조체
typedef struct {
    int krx_sock; // 소켓
    mkq_stock_infos *data;
} mkq_thread;

// 종목 정보 응답 구조체 
typedef struct {
	hdr hdr;
	stock_info body[4];
} kmt_stock_infos;

// 종목 정보 응답 변환 구조체 
typedef struct {
	int pipe_write;
	kmt_stock_infos *data;
} kmt_thread;

// 시세 배열 구조체
typedef struct {
	hdr hdr;
	current_market_price body[4];
} kmt_current_market_prices;


int handle_krx(int krx_sock, int pipe_write, int pipe_read);

#endif