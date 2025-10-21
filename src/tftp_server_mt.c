#include "../include/tftp.h"
#include <process.h>  // Windows线程支持

// 线程安全的日志记录互斥锁
static CRITICAL_SECTION log_mutex;
static int mutex_initialized = 0;

/**
 * 解析接收到的TFTP协议数据包
 * 
 * 功能说明：
 * - 解析UDP数据包中的TFTP协议内容
 * - 根据操作码解析不同类型的TFTP包
 * - 验证数据包格式的完整性和合法性
 * - 提取文件名、传输模式、数据块等信息
 * 
 * TFTP数据包类型：
 * - RRQ/WRQ: 读/写请求包
 * - DATA: 数据包
 * - ACK: 确认包
 * - ERROR: 错误包
 * 
 * 参数：
 * - buffer: 原始数据缓冲区
 * - buffer_len: 缓冲区长度
 * - packet: 解析后的TFTP包结构指针
 * 
 * 返回值：
 * - 成功：0
 * - 失败：-1（包格式错误或长度不足）
 */
int parse_tftp_packet(char* buffer, int buffer_len, tftp_packet_t* packet) {
    // 检查数据包最小长度（至少包含2字节操作码）
    if (buffer_len < 2) {
        return -1; // 包长度不足
    }
    
    // 提取操作码（网络字节序转主机字节序）
    packet->opcode = ntohs(*(unsigned short*)buffer);
    
    // 根据操作码解析不同类型的数据包
    switch (packet->opcode) {
        case TFTP_RRQ:              // 读请求（客户端下载文件）
        case TFTP_WRQ: {            // 写请求（客户端上传文件）
            // RRQ/WRQ包格式：操作码(2) + 文件名(变长) + 0 + 模式(变长) + 0
            char* ptr = buffer + 2;                      // 跳过操作码
            int remaining = buffer_len - 2;              // 剩余数据长度
            
            // 解析文件名（以null结尾的字符串）
            int filename_len = strnlen(ptr, remaining);
            if (filename_len >= remaining || filename_len >= MAX_FILENAME_LEN) {
                return -1; // 文件名长度非法
            }
            
            strcpy(packet->request.filename, ptr);       // 复制文件名
            ptr += filename_len + 1;                     // 跳过文件名和null终止符
            remaining -= filename_len + 1;
            
            // 解析传输模式（"netascii"或"octet"）
            int mode_len = strnlen(ptr, remaining);
            if (mode_len >= remaining || mode_len >= MAX_MODE_LEN) {
                return -1; // 模式字符串格式错误
            }
            
            strcpy(packet->request.mode, ptr);           // 复制传输模式
            break;
        }
        
        case TFTP_DATA: {           // 数据包
            // DATA包格式：操作码(2) + 块号(2) + 数据(0-512字节)
            if (buffer_len < 4) {
                return -1; // 数据包头部不完整
            }
            
            // 提取数据块号
            packet->data.block_num = ntohs(*(unsigned short*)(buffer + 2));
            
            // 提取数据内容
            size_t data_len = buffer_len - 4;
            if (data_len > DATA_SIZE) {
                return -1; // 数据长度超出TFTP协议限制
            }
            memcpy(packet->data.data, buffer + 4, data_len);
            break;
        }
        
        case TFTP_ACK: {            // 确认包
            // ACK包格式：操作码(2) + 块号(2)，固定4字节长度
            if (buffer_len != 4) {
                return -1; // ACK包大小必须是4字节
            }
            // 提取确认的数据块号
            packet->ack.block_num = ntohs(*(unsigned short*)(buffer + 2));
            break;
        }
        
        case TFTP_ERROR: {          // 错误包
            // ERROR包格式：操作码(2) + 错误码(2) + 错误消息(变长) + 0
            if (buffer_len < 4) {
                return -1; // 错误包头部至少4字节
            }
            
            // 提取错误码
            packet->error.error_code = ntohs(*(unsigned short*)(buffer + 2));
            
            // 提取错误消息
            int msg_len = buffer_len - 4;
            if (msg_len > 0 && msg_len < 512) {
                memcpy(packet->error.error_msg, buffer + 4, msg_len);
                packet->error.error_msg[msg_len] = '\0'; // 确保字符串正确结束
            } else {
                packet->error.error_msg[0] = '\0';       // 空错误消息
            }
            break;
        }
        
        default:
            return -1; // 未知的TFTP操作码
    }
    
    return 0;      // 解析成功
}

// 客户端请求处理的线程参数结构
typedef struct {
    SOCKET server_sock;           // 服务器套接字
    tftp_packet_t packet;         // 客户端请求包
    struct sockaddr_in client_addr; // 客户端地址
    int packet_size;              // 数据包大小
} client_request_t;

/**
 * 线程安全的日志记录函数
 * 使用临界区保护日志写入操作
 */
void thread_safe_log(const char* level, const char* message, ...) {
    if (!mutex_initialized) {
        InitializeCriticalSection(&log_mutex);
        mutex_initialized = 1;
    }
    
    EnterCriticalSection(&log_mutex);
    
    va_list args;
    va_start(args, message);
    
    // 获取当前时间
    time_t now;
    time(&now);
    struct tm* local_time = localtime(&now);
    
    // 输出到控制台
    printf("[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
           local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
           local_time->tm_hour, local_time->tm_min, local_time->tm_sec, level);
    vprintf(message, args);
    printf("\n");
    
    // 输出到日志文件
    FILE* log_file = fopen("logs/tftp_server_mt.log", "a");
    if (log_file) {
        fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
                local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
                local_time->tm_hour, local_time->tm_min, local_time->tm_sec, level);
        vfprintf(log_file, message, args);
        fprintf(log_file, "\n");
        fclose(log_file);
    }
    
    va_end(args);
    LeaveCriticalSection(&log_mutex);
}

/**
 * 处理RRQ请求的线程安全版本
 * 基于原有handle_rrq函数，添加线程安全机制
 */
void handle_rrq_mt(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr) {
    char* ptr = (char*)packet + 2;
    char filename[MAX_FILENAME_LEN];
    char mode[MAX_MODE_LEN];
    char filepath[512];
    
    // 解析文件名和模式
    strcpy(filename, ptr);
    ptr += strlen(filename) + 1;
    strcpy(mode, ptr);
    
    thread_safe_log("INFO", "Thread %lu: Client %s:%d requests download file: %s, mode: %s", 
                   GetCurrentThreadId(), inet_ntoa(client_addr->sin_addr), 
                   ntohs(client_addr->sin_port), filename, mode);
    
    // 构造文件路径
    snprintf(filepath, sizeof(filepath), "tftp_root/%s", filename);
    
    // 打开文件
    FILE* file = fopen(filepath, (parse_mode(mode) == MODE_NETASCII) ? "r" : "rb");
    if (file == NULL) {
        thread_safe_log("ERROR", "Thread %lu: Cannot open file: %s", GetCurrentThreadId(), filepath);
        send_error_packet(sock, client_addr, TFTP_ERROR_FILE_NOT_FOUND, "File not found");
        return;
    }
    
    // 创建数据传输套接字
    SOCKET data_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (data_sock == INVALID_SOCKET) {
        thread_safe_log("ERROR", "Thread %lu: Failed to create data transfer socket", GetCurrentThreadId());
        fclose(file);
        send_error_packet(sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Server internal error");
        return;
    }
    
    // 绑定到动态端口
    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = INADDR_ANY;
    data_addr.sin_port = 0;
    
    if (bind(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) == SOCKET_ERROR) {
        thread_safe_log("ERROR", "Thread %lu: Failed to bind data transfer socket", GetCurrentThreadId());
        closesocket(data_sock);
        fclose(file);
        send_error_packet(sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Server internal error");
        return;
    }
    
    // 设置超时
    int timeout = TIMEOUT_SECONDS * 1000;
    setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    // 传输统计
    tftp_stats_t stats = {0};
    time(&stats.start_time);
    
    unsigned short block_num = 1;
    char data_buffer[DATA_SIZE];
    int bytes_read;
    
    // 文件传输循环
    while ((bytes_read = fread(data_buffer, 1, DATA_SIZE, file)) > 0) {
        int ack_received = 0;
        int retries = 0;
        
        while (!ack_received && retries < MAX_RETRIES) {
            // 发送数据包
            if (send_data_packet(data_sock, client_addr, block_num, data_buffer, bytes_read) < 0) {
                thread_safe_log("ERROR", "Thread %lu: Failed to send data packet %d", GetCurrentThreadId(), block_num);
                break;
            }
            
            stats.blocks_sent++;
            
            // 等待ACK
            char ack_buffer[4];
            struct sockaddr_in ack_addr;
            int ack_addr_len = sizeof(ack_addr);
            
            int recv_result = recvfrom(data_sock, ack_buffer, sizeof(ack_buffer), 0,
                                     (struct sockaddr*)&ack_addr, &ack_addr_len);
            
            if (recv_result == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAETIMEDOUT) {
                    thread_safe_log("WARNING", "Thread %lu: Waiting for ACK timed out, retransmitting data packet %d", 
                                   GetCurrentThreadId(), block_num);
                    retries++;
                    stats.retransmissions++;
                    continue;
                }
                thread_safe_log("ERROR", "Thread %lu: Failed to receive ACK: %d", GetCurrentThreadId(), error);
                break;
            }
            
            // 验证ACK包
            if (recv_result == 4) {
                unsigned short ack_opcode = ntohs(*(unsigned short*)ack_buffer);
                unsigned short ack_block = ntohs(*(unsigned short*)(ack_buffer + 2));
                
                if (ack_opcode == TFTP_ACK && ack_block == block_num) {
                    ack_received = 1;
                    stats.bytes_transferred += bytes_read;
                }
            }
        }
        
        if (!ack_received) {
            thread_safe_log("ERROR", "Thread %lu: Failed to receive ACK after %d retries", GetCurrentThreadId(), MAX_RETRIES);
            break;
        }
        
        block_num++;
        
        // 如果读取的数据少于512字节，说明文件传输完成
        if (bytes_read < DATA_SIZE) {
            break;
        }
    }
    
    // 记录传输完成
    time(&stats.end_time);
    thread_safe_log("INFO", "Thread %lu: File transfer completed for %s", GetCurrentThreadId(), filename);
    
    // 打印传输统计
    if (stats.end_time > stats.start_time) {
        double duration = difftime(stats.end_time, stats.start_time);
        double throughput = stats.bytes_transferred / duration;
        thread_safe_log("INFO", "Thread %lu: Transfer statistics - Bytes: %zu, Duration: %.2fs, Throughput: %.2f bytes/s", 
                       GetCurrentThreadId(), stats.bytes_transferred, duration, throughput);
    }
    
    // 清理资源
    closesocket(data_sock);
    fclose(file);
}

/**
 * 处理WRQ请求的线程安全版本
 * 基于原有handle_wrq函数，添加线程安全机制
 */
void handle_wrq_mt(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr) {
    char* ptr = (char*)packet + 2;
    char filename[MAX_FILENAME_LEN];
    char mode[MAX_MODE_LEN];
    char filepath[512];
    
    // 解析文件名和模式
    strcpy(filename, ptr);
    ptr += strlen(filename) + 1;
    strcpy(mode, ptr);
    
    thread_safe_log("INFO", "Thread %lu: Client %s:%d requests upload file: %s, mode: %s", 
                   GetCurrentThreadId(), inet_ntoa(client_addr->sin_addr), 
                   ntohs(client_addr->sin_port), filename, mode);
    
    // 构造文件路径
    snprintf(filepath, sizeof(filepath), "tftp_root/%s", filename);
    
    // 检查文件是否已存在
    FILE* existing_file = fopen(filepath, "r");
    if (existing_file != NULL) {
        fclose(existing_file);
        thread_safe_log("ERROR", "Thread %lu: File already exists: %s", GetCurrentThreadId(), filepath);
        send_error_packet(sock, client_addr, TFTP_ERROR_FILE_EXISTS, "File already exists");
        return;
    }
    
    // 创建新文件
    FILE* file = fopen(filepath, (parse_mode(mode) == MODE_NETASCII) ? "w" : "wb");
    if (file == NULL) {
        thread_safe_log("ERROR", "Thread %lu: Cannot create file: %s", GetCurrentThreadId(), filepath);
        send_error_packet(sock, client_addr, TFTP_ERROR_ACCESS_VIOLATION, "Cannot create file");
        return;
    }
    
    // 发送初始ACK (block 0)
    if (send_ack_packet(sock, client_addr, 0) < 0) {
        thread_safe_log("ERROR", "Thread %lu: Failed to send initial ACK", GetCurrentThreadId());
        fclose(file);
        return;
    }
    
    // 传输统计
    tftp_stats_t stats = {0};
    time(&stats.start_time);
    
    unsigned short expected_block = 1;
    
    // 文件接收循环
    while (1) {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in data_addr;
        int data_addr_len = sizeof(data_addr);
        
        // 接收数据包
        int recv_result = recvfrom(sock, buffer, sizeof(buffer), 0,
                                 (struct sockaddr*)&data_addr, &data_addr_len);
        
        if (recv_result == SOCKET_ERROR) {
            thread_safe_log("ERROR", "Thread %lu: Failed to receive data packet", GetCurrentThreadId());
            break;
        }
        
        // 解析数据包
        tftp_packet_t data_packet;
        if (parse_tftp_packet(buffer, recv_result, &data_packet) < 0) {
            thread_safe_log("WARNING", "Thread %lu: Received invalid packet", GetCurrentThreadId());
            continue;
        }
        
        if (data_packet.opcode == TFTP_DATA) {
            if (data_packet.data.block_num == expected_block) {
                // 写入数据到文件
                size_t data_len = recv_result - 4; // 减去4字节头部
                if (fwrite(data_packet.data.data, 1, data_len, file) != data_len) {
                    thread_safe_log("ERROR", "Thread %lu: Failed to write data to file", GetCurrentThreadId());
                    send_error_packet(sock, client_addr, TFTP_ERROR_DISK_FULL, "Disk full or write error");
                    break;
                }
                
                stats.bytes_transferred += data_len;
                
                // 发送ACK
                if (send_ack_packet(sock, client_addr, expected_block) < 0) {
                    thread_safe_log("ERROR", "Thread %lu: Failed to send ACK", GetCurrentThreadId());
                    break;
                }
                
                expected_block++;
                
                // 如果数据长度小于512字节，传输完成
                if (data_len < DATA_SIZE) {
                    time(&stats.end_time);
                    thread_safe_log("INFO", "Thread %lu: File upload completed for %s", GetCurrentThreadId(), filename);
                    
                    // 打印传输统计
                    if (stats.end_time > stats.start_time) {
                        double duration = difftime(stats.end_time, stats.start_time);
                        double throughput = stats.bytes_transferred / duration;
                        thread_safe_log("INFO", "Thread %lu: Upload statistics - Bytes: %zu, Duration: %.2fs, Throughput: %.2f bytes/s", 
                                       GetCurrentThreadId(), stats.bytes_transferred, duration, throughput);
                    }
                    break;
                }
            } else {
                // 重复或乱序的数据包，重新发送ACK
                thread_safe_log("WARNING", "Thread %lu: Received duplicate or out-of-order packet, block %d (expected %d)", 
                               GetCurrentThreadId(), data_packet.data.block_num, expected_block);
                if (data_packet.data.block_num == expected_block - 1) {
                    send_ack_packet(sock, client_addr, data_packet.data.block_num);
                }
            }
        } else if (data_packet.opcode == TFTP_ERROR) {
            thread_safe_log("INFO", "Thread %lu: Client reported error: %s", GetCurrentThreadId(), data_packet.error.error_msg);
            break;
        }
    }
    
    // 清理资源
    fclose(file);
}

/**
 * 客户端请求处理线程函数
 * 每个客户端请求在独立线程中处理
 */
unsigned __stdcall client_handler_thread(void* param) {
    client_request_t* request = (client_request_t*)param;
    
    thread_safe_log("INFO", "Thread %lu: Started handling client request, opcode: %d", 
                   GetCurrentThreadId(), request->packet.opcode);
    
    // 根据请求类型分发处理
    switch (request->packet.opcode) {
        case TFTP_RRQ:
            handle_rrq_mt(request->server_sock, &request->packet, &request->client_addr);
            break;
            
        case TFTP_WRQ:
            handle_wrq_mt(request->server_sock, &request->packet, &request->client_addr);
            break;
            
        default:
            thread_safe_log("WARNING", "Thread %lu: Unsupported opcode: %d", GetCurrentThreadId(), request->packet.opcode);
            send_error_packet(request->server_sock, &request->client_addr, 
                            TFTP_ERROR_ILLEGAL_OPERATION, "Unsupported operation");
            break;
    }
    
    thread_safe_log("INFO", "Thread %lu: Finished handling client request", GetCurrentThreadId());
    
    // 释放请求参数内存
    free(request);
    
    return 0;
}

/**
 * 显示帮助信息
 */
void show_help_mt(void) {
    printf("\n");
    printf("=================================================================\n");
    printf("                Multi-threaded TFTP Server v1.0                 \n");
    printf("=================================================================\n");
    printf("Features:\n");
    printf("  ✓ Support multiple concurrent client access\n");
    printf("  ✓ Support file upload (PUT) and download (GET)\n");
    printf("  ✓ Support netascii and octet transfer modes\n");
    printf("  ✓ Automatic retransmission and error recovery\n");
    printf("  ✓ Thread-safe logging\n");
    printf("  ✓ Transfer speed statistics\n");
    printf("\n");
    printf("Server Configuration:\n");
    printf("  Listen Port: %d\n", TFTP_PORT);
    printf("  File Root Directory: tftp_root/\n");
    printf("  Log File: logs/tftp_server_mt.log\n");
    printf("  Max Retries: %d\n", MAX_RETRIES);
    printf("  Timeout: %d seconds\n", TIMEOUT_SECONDS);
    printf("\n");
    printf("Client Usage Examples:\n");
    printf("  Download file: tftp -i 127.0.0.1 get test.txt local_test.txt\n");
    printf("  Upload file: tftp -i 127.0.0.1 put local_file.txt remote_file.txt\n");
    printf("\n");
    printf("Press Ctrl+C to stop the server\n");
    printf("=================================================================\n");
    printf("\n");
}

/**
 * 控制台信号处理器
 */
BOOL WINAPI console_handler_mt(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        thread_safe_log("INFO", "Received Ctrl+C, shutting down server...");
        
        // 清理临界区
        if (mutex_initialized) {
            DeleteCriticalSection(&log_mutex);
        }
        
        cleanup_winsock();
        exit(0);
    }
    return TRUE;
}

/**
 * 主函数 - 多线程TFTP服务器
 */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Multi-threaded TFTP Server starting...\n");
    
    // 设置控制台信号处理器
    if (!SetConsoleCtrlHandler(console_handler_mt, TRUE)) {
        printf("Warning: Unable to set signal handler\n");
    }
    
    // 显示帮助信息
    show_help_mt();
    
    // 初始化网络
    init_winsock();
    
    // 创建服务器套接字
    SOCKET server_sock = create_tftp_socket();
    if (server_sock == INVALID_SOCKET) {
        cleanup_winsock();
        return 1;
    }
    
    // 创建必要目录
    CreateDirectory("tftp_root", NULL);
    CreateDirectory("logs", NULL);
    
    thread_safe_log("INFO", "Multi-threaded TFTP server started successfully, waiting for client connections...");
    
    // 创建测试文件
    FILE* test_file = fopen("tftp_root/test.txt", "r");
    if (test_file == NULL) {
        test_file = fopen("tftp_root/test.txt", "w");
        if (test_file != NULL) {
            fprintf(test_file, "Hello, this is a test file for multi-threaded TFTP server!\n");
            fprintf(test_file, "This server can handle multiple clients concurrently.\n");
            fclose(test_file);
            thread_safe_log("INFO", "Created test file: tftp_root/test.txt");
        }
    } else {
        fclose(test_file);
    }
    
    // 主服务循环
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    
    while (1) {
        // 接收客户端请求
        int recv_result = recvfrom(server_sock, buffer, sizeof(buffer), 0,
                                 (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (recv_result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEINTR) {
                thread_safe_log("ERROR", "Failed to receive data: %d", error);
            }
            continue;
        }
        
        if (recv_result == 0) {
            thread_safe_log("WARNING", "Received empty packet");
            continue;
        }
        
        // 解析TFTP数据包
        tftp_packet_t packet;
        if (parse_tftp_packet(buffer, recv_result, &packet) < 0) {
            thread_safe_log("WARNING", "Received invalid TFTP packet from %s:%d", 
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            send_error_packet(server_sock, &client_addr, 
                            TFTP_ERROR_ILLEGAL_OPERATION, "Invalid packet format");
            continue;
        }
        
        // 只处理RRQ和WRQ请求
        if (packet.opcode == TFTP_RRQ || packet.opcode == TFTP_WRQ) {
            // 为每个客户端请求创建新线程
            client_request_t* request = (client_request_t*)malloc(sizeof(client_request_t));
            if (request == NULL) {
                thread_safe_log("ERROR", "Failed to allocate memory for client request");
                send_error_packet(server_sock, &client_addr, 
                                TFTP_ERROR_NOT_DEFINED, "Server internal error");
                continue;
            }
            
            // 复制请求信息
            request->server_sock = server_sock;
            request->packet = packet;
            request->client_addr = client_addr;
            request->packet_size = recv_result;
            
            // 创建处理线程
            HANDLE thread_handle = (HANDLE)_beginthreadex(NULL, 0, client_handler_thread, 
                                                         request, 0, NULL);
            if (thread_handle == 0) {
                thread_safe_log("ERROR", "Failed to create client handler thread");
                free(request);
                send_error_packet(server_sock, &client_addr, 
                                TFTP_ERROR_NOT_DEFINED, "Server internal error");
                continue;
            }
            
            // 分离线程，让其自动清理
            CloseHandle(thread_handle);
            
            thread_safe_log("INFO", "Created new thread for client %s:%d, opcode: %d", 
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), 
                           packet.opcode);
        } else {
            // 处理其他类型的包
            thread_safe_log("WARNING", "Received unexpected packet type %d from %s:%d", 
                           packet.opcode, inet_ntoa(client_addr.sin_addr), 
                           ntohs(client_addr.sin_port));
            
            if (packet.opcode == TFTP_DATA || packet.opcode == TFTP_ACK) {
                send_error_packet(server_sock, &client_addr, 
                                TFTP_ERROR_UNKNOWN_TID, "Unknown transfer ID");
            } else if (packet.opcode == TFTP_ERROR) {
                thread_safe_log("INFO", "Client %s:%d reported error: %s", 
                               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                               packet.error.error_msg);
            } else {
                send_error_packet(server_sock, &client_addr, 
                                TFTP_ERROR_ILLEGAL_OPERATION, "Unsupported operation");
            }
        }
    }
    
    // 清理资源（实际不会执行到这里）
    closesocket(server_sock);
    cleanup_winsock();
    
    if (mutex_initialized) {
        DeleteCriticalSection(&log_mutex);
    }
    
    return 0;
}