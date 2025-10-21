#ifndef TFTP_H
#define TFTP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>

// TFTP协议常量定义
#define TFTP_PORT 69            // TFTP默认端口
#define BUFFER_SIZE 516         // TFTP数据包最大大小（512字节数据 + 4字节头部）
#define DATA_SIZE 512           // TFTP数据块大小
#define MAX_FILENAME_LEN 255    // 最大文件名长度
#define MAX_MODE_LEN 10         // 最大模式名长度
#define MAX_RETRIES 5           // 最大重传次数
#define TIMEOUT_SECONDS 5       // 超时时间（秒）

// TFTP操作码定义
typedef enum {
    TFTP_RRQ = 1,      // 读请求（下载）
    TFTP_WRQ = 2,      // 写请求（上传）
    TFTP_DATA = 3,     // 数据包
    TFTP_ACK = 4,      // 确认包
    TFTP_ERROR = 5     // 错误包
} tftp_opcode_t;

// TFTP错误码定义
typedef enum {
    TFTP_ERROR_NOT_DEFINED = 0,         // 未定义错误
    TFTP_ERROR_FILE_NOT_FOUND = 1,      // 文件未找到
    TFTP_ERROR_ACCESS_VIOLATION = 2,     // 访问违规
    TFTP_ERROR_DISK_FULL = 3,           // 磁盘已满
    TFTP_ERROR_ILLEGAL_OPERATION = 4,    // 非法操作
    TFTP_ERROR_UNKNOWN_TID = 5,         // 未知传输ID
    TFTP_ERROR_FILE_EXISTS = 6,         // 文件已存在
    TFTP_ERROR_NO_SUCH_USER = 7         // 用户不存在
} tftp_error_code_t;

// TFTP传输模式
typedef enum {
    MODE_NETASCII = 0,  // ASCII模式
    MODE_OCTET = 1      // 二进制模式
} tftp_mode_t;

// TFTP数据包结构
typedef struct {
    unsigned short opcode;              // 操作码
    union {
        struct {                        // RRQ/WRQ请求包
            char filename[MAX_FILENAME_LEN];
            char mode[MAX_MODE_LEN];
        } request;
        
        struct {                        // 数据包
            unsigned short block_num;
            char data[DATA_SIZE];
        } data;
        
        struct {                        // ACK包
            unsigned short block_num;
        } ack;
        
        struct {                        // 错误包
            unsigned short error_code;
            char error_msg[512];
        } error;
    };
} tftp_packet_t;

// 客户端会话信息
typedef struct {
    struct sockaddr_in client_addr;     // 客户端地址
    int client_addr_len;                // 客户端地址长度
    FILE* file_handle;                  // 文件句柄
    tftp_mode_t transfer_mode;          // 传输模式
    unsigned short current_block;       // 当前块号
    char filename[MAX_FILENAME_LEN];    // 文件名
    int is_upload;                      // 是否为上传操作
    time_t last_activity;               // 最后活动时间
} tftp_session_t;

// 传输统计信息
typedef struct {
    size_t bytes_transferred;           // 传输字节数
    time_t start_time;                  // 开始时间
    time_t end_time;                    // 结束时间
    int blocks_sent;                    // 发送的数据块数
    int retransmissions;                // 重传次数
} tftp_stats_t;

// 函数声明
void init_winsock(void);
void cleanup_winsock(void);
int create_tftp_socket(void);
void log_message(const char* level, const char* message, ...);
int send_error_packet(SOCKET sock, struct sockaddr_in* client_addr, 
                     tftp_error_code_t error_code, const char* error_msg);
int send_ack_packet(SOCKET sock, struct sockaddr_in* client_addr, 
                   unsigned short block_num);
int send_data_packet(SOCKET sock, struct sockaddr_in* client_addr, 
                    unsigned short block_num, char* data, int data_len);
void handle_rrq(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr);
void handle_wrq(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr);
void handle_data(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr);
void handle_ack(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr);
tftp_mode_t parse_mode(const char* mode_str);
const char* get_error_message(tftp_error_code_t error_code);
void print_throughput(tftp_stats_t* stats);

#endif // TFTP_H