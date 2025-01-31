#ifndef ENV_H
#define ENV_H

/****************************************************
    ec2 ip 사용 시, 보안을 위해 파일 .gitignore 추가하기
****************************************************/
// #define OMS_SERVER_IP "106.101.9.158"
// #define OMS_SERVER_PORT 8082

#define OMS_SERVER_IP "127.0.0.1"
#define OMS_SERVER_PORT 54321
// IP : "43.201.73.157", PORT : 8080
// #define KRX_SERVER_IP "43.201.73.157"
// #define KRX_SERVER_PORT 8080

#define KRX_SERVER_IP "127.0.0.1"
#define KRX_SERVER_PORT 12345

#define MCI_SERVER_IP "127.0.0.1"
#define MCI_SERVER_PORT 8081

#define MYSQL_HOST "127.0.0.1"
#define MYSQL_USER "root"
#define MYSQL_PASSWORD "1234"
#define MYSQL_DBNAME "mydb"

#endif
