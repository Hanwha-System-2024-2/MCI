#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "batch.h"

// 오늘 날짜 기반 파일 경로 생성
void get_today_filepath(char *filepath, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    snprintf(filepath, size, "%s%s%04d_%02d_%02d%s",
             FILE_DIRECTORY, FILE_PREFIX, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, FILE_EXTENSION);
}

// 데이터를 오늘 날짜 파일로 저장
void save_data_to_today_file(const void *data, size_t size) {
    char today_filepath[256];
    get_today_filepath(today_filepath, sizeof(today_filepath));

    FILE *file = fopen(today_filepath, "wb");  // 바이너리 모드로 저장
    if (file == NULL) {
        perror("[Batch-Save] Failed to open file for writing");
        return;
    }

    size_t written = fwrite(data, 1, size, file);
    fclose(file);

    if (written == size) {
        printf("[Batch-Save] Data saved successfully: %s\n", today_filepath);
    } else {
        perror("[Batch-Save] Failed to write full data to file");
    }
}

// 서버로 `mkq_stock_info` 요청 전송 (가상 데이터)
void request_mkq_stock_info() {
    printf("[Batch-Request] Sending mkq_stock_info request to server...\n");
    // 여기에 실제 서버 요청 코드 추가 (소켓, HTTP 등)
}

// `kmt_stock_info` 응답 수신 및 파일 저장 (가상 데이터 사용)
void receive_kmt_stock_info() {
    printf("[Batch-Response] Receiving kmt_stock_info from server...\n");

    // 예제 데이터 (실제 서버에서 받은 데이터를 대체해야 함)
    char mock_data[] = "Mock kmt_stock_info response data";
    size_t data_size = sizeof(mock_data);

    // 파일로 저장
    save_data_to_today_file(mock_data, data_size);
}

int main() {
    // 서버 요청
    request_mkq_stock_info();

    // 응답 수신 및 데이터 저장
    receive_kmt_stock_info();

    return 0;
}
