#ifndef OMS_HEADER_H
#define OMS_HEADER_H

#include "common.h" // 공통 헤더 포함

struct omq_login {
	hdr hdr;
	char user_id[50];
	char user_pw[50];
};

struct mot_login {
	hdr hdr;
	int status_code;
};

struct omq_stocks {
	hdr hdr;
};

struct mot_stocks {
	hdr hdr;
	stock_info body[4]; // kmt_stock_infos 구조체에서 가져온 stock_info[4]
};

struct omq_market_price {
	hdr hdr;
};

struct mot_market_price {
	hdr hdr;
	current_market_price body[4]; // kmt_current_market_prices 구조체에서 가져온 current_market_price[4]
};

#endif