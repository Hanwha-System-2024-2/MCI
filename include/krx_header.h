#ifndef KRX_HEADER_H
#define KRX_HEADER_H

#include "common.h" // 공통 헤더 포함

// 종목 정보 배열 구조체 
typedef struct {
		hdr header;
		stock_info body[4];
} kmt_stock_infos;

// 시세 배열 구조체
typedef struct {
	hdr header;
	current_market_price body[4];
} kmt_current_market_prices;

#endif