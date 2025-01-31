// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

// 헤더 구성
typedef struct {
	int tr_id;
	int length;
} hdr;

// 호가 구조체
typedef struct  {
	int trading_type;       // 매수 OR 매도
	int price; 
	int balance;
} hoga;

// 단일 시세 구조체
typedef struct {
  char stock_code[7]; char padding1;    // 종목코드  
  char stock_name[51]; char padding2;      // 종목명
  int price;              // 시세(현재가)
  long volume;              // 거래량
  int change;               // 대비
  char rate_of_change[11]; char padding3;  // 등락률
  hoga hoga[2];             // 호가
  int high_price;           // 고가
  int low_price;            // 저가
  char market_time[19]; char padding4;    // 시세 형성 시간
} current_market_price;

// 종목 정보 구조체
typedef struct {
   char stock_code[7]; char padding1;
   char stock_name[51]; char padding2;
} stock_info;

/* tr_id */

// OMS - MCI 관련
#define OMQ_LOGIN 1
#define OMQ_STOCK_INFOS 2
#define OMQ_CURRENT_MARKET_PRICE 3
#define MOT_LOGIN 4
#define MOT_STOCK_INFOS 5
#define MOT_CURRENT_MARKET_PRICE 6
#define OMQ_TX_HISTORY 12
#define MOT_TX_HISTORY 13

// KRX - MCI 관련
#define MKQ_STOCK_INFOS 7
#define KMT_CURRENT_MARKET_PRICES 8
#define KMT_STOCK_INFOS 14

// KRX - FEP 관련
#define FKQ_ORDER 9 
#define KFT_ORDER 10
#define KFT_EXECUTION 11

#endif