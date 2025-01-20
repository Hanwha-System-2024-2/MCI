// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

// 헤더 구성
typedef struct {
	char tr_id[4];
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
  char stock_code[6];       // 종목코드
  char stock_name[50];      // 종목명
  float price;              // 시세(현재가)
  long volume;              // 거래량
  int change;               // 대비
  char rate_of_change[10];  // 등락률
  hoga hoga[2];             // 호가
  int high_price;           // 고가
  int low_price;            // 저가
  char market_time[18];     // 시세 형성 시간
} current_market_price;

// 종목 정보 구조체
typedef struct {
   char stock_code[6];
   char stock_name[50];
} stock_info;

#endif