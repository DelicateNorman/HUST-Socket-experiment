#include "../include/tftp.h"

/**
 * 处理TFTP读请求（RRQ）- 客户端下载文件
 * 
 * 功能说明：
 * - 解析客户端的文件下载请求
 * - 打开请求的文件并验证访问权限
 * - 创建新的套接字用于数据传输
 * - 分块发送文件数据给客户端
 * - 处理ACK确认和超时重传
 * - 记录传输统计信息
 * 
 * TFTP读请求流程：
 * 1. 客户端发送RRQ包（包含文件名和传输模式）
 * 2. 服务器打开文件并验证
 * 3. 服务器创建新端口用于数据传输
 * 4. 服务器发送数据包，等待ACK确认
 * 5. 重复步骤4直到文件传输完成
 * 
 * 参数：
 * - sock: 服务器主监听套接字
 * - packet: 解析后的RRQ请求包
 * - client_addr: 客户端地址信息
 */
void handle_rrq(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr) {
    char* ptr = (char*)packet + 2;                       // 跳过2字节操作码
    char filename[MAX_FILENAME_LEN];                     // 存储文件名
    char mode[MAX_MODE_LEN];                             // 存储传输模式
    char filepath[512];                                  // 存储完整文件路径
    
    // 解析文件名（以null结尾的字符串）
    strcpy(filename, ptr);
    ptr += strlen(filename) + 1;                         // 移动指针到下一个字段
    
    // 解析传输模式（netascii或octet）
    strcpy(mode, ptr);
    
    // 记录客户端请求信息
    log_message("INFO", "Client %s:%d requests download file: %s, mode: %s", 
               inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), 
               filename, mode);
    
    // 构造完整的文件路径（在tftp_root目录下）
    snprintf(filepath, sizeof(filepath), "tftp_root/%s", filename);
    
    // 根据传输模式选择文件打开方式
    // netascii模式用文本方式，octet模式用二进制方式
    FILE* file = fopen(filepath, (parse_mode(mode) == MODE_NETASCII) ? "r" : "rb");
    if (file == NULL) {
        log_message("ERROR", "Cannot open file: %s", filepath);
        send_error_packet(sock, client_addr, TFTP_ERROR_FILE_NOT_FOUND, "File not found");
        return;
    }
    
    // 初始化文件传输统计信息
    tftp_stats_t stats = {0};
    time(&stats.start_time);                             // 记录传输开始时间
    
    unsigned short block_num = 1;                        // 数据块号（从1开始）
    char data_buffer[DATA_SIZE];                         // 数据缓冲区（512字节）
    int bytes_read;                                      // 实际读取的字节数
    int retries = 0;                                     // 重试次数
    
    // 创建新的UDP套接字用于数据传输
    // TFTP协议要求：数据传输使用服务器动态分配的新端口
    SOCKET data_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (data_sock == INVALID_SOCKET) {
        log_message("ERROR", "Failed to create data transfer socket");
        fclose(file);
        send_error_packet(sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Server internal error");
        return;
    }
    
    // 配置数据传输套接字
    // 绑定到系统动态分配的端口（TFTP协议要求）
    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;                      // IPv4地址族
    data_addr.sin_addr.s_addr = INADDR_ANY;              // 绑定到所有网络接口
    data_addr.sin_port = 0;                              // 端口号为0，让系统自动分配
    
    if (bind(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) == SOCKET_ERROR) {
        log_message("ERROR", "Failed to bind data transfer socket");
        closesocket(data_sock);
        fclose(file);
        send_error_packet(sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Server internal error");
        return;
    }
    
    // 设置套接字接收超时时间
    // 用于处理网络延迟和丢包情况
    int timeout = TIMEOUT_SECONDS * 1000;               // 转换为毫秒
    setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    // 开始文件数据传输循环
    // 每次读取最多512字节数据，分块发送
    while ((bytes_read = fread(data_buffer, 1, DATA_SIZE, file)) > 0) {
        int ack_received = 0;                            // ACK接收标志
        retries = 0;                                     // 重置重试计数器
        
        // 可靠传输机制：发送数据包并等待ACK确认
        // 如果超时或收到错误ACK，则重传，最多重试MAX_RETRIES次
        while (!ack_received && retries < MAX_RETRIES) {
            // 发送当前数据块
            if (send_data_packet(data_sock, client_addr, block_num, data_buffer, bytes_read) < 0) {
                log_message("ERROR", "Failed to send data packet, block number: %d");
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
                    log_message("WARNING", "Waiting for ACK timed out, retransmitting data packet, block number: %d", block_num);
                    retries++;
                    stats.retransmissions++;
                    continue;
                } else {
                    log_message("ERROR", "Failed to receive ACK: %d", error);
                    break;
                }
            }
            
            // 验证ACK包
            unsigned short ack_opcode = ntohs(*(unsigned short*)ack_buffer);
            unsigned short ack_block = ntohs(*(unsigned short*)(ack_buffer + 2));
            
            if (ack_opcode == TFTP_ACK && ack_block == block_num) {
                log_message("DEBUG", "Received ACK, block number: %d", block_num);
                ack_received = 1;
                stats.bytes_transferred += bytes_read;
            } else {
                log_message("WARNING", "Received invalid ACK, expected block number: %d, received block number: %d",
                           block_num, ack_block);
                retries++;
            }
        }
        
        if (!ack_received) {
            log_message("ERROR", "Number %d transmission failed, reached maximum retry count", block_num);
            send_error_packet(data_sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Transfer timed out");
            break;
        }
        
        block_num++;
        
        // 如果读取的字节数小于512，说明这是最后一个包
        if (bytes_read < DATA_SIZE) {
            log_message("INFO", "File transfer complete: %s", filename);
            break;
        }
    }
    
    time(&stats.end_time);
    print_throughput(&stats);
    
    fclose(file);
    closesocket(data_sock);
}

/**
 * 处理写请求（WRQ）- 客户端要上传文件
 */
void handle_wrq(SOCKET sock, tftp_packet_t* packet, struct sockaddr_in* client_addr) {
    char* ptr = (char*)packet + 2; // 跳过操作码
    char filename[MAX_FILENAME_LEN];
    char mode[MAX_MODE_LEN];
    char filepath[512];
    
    // 解析文件名
    strcpy(filename, ptr);
    ptr += strlen(filename) + 1;
    
    // 解析传输模式
    strcpy(mode, ptr);

    log_message("INFO", "Client %s:%d requests to upload file: %s, mode: %s",
               inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port),
               filename, mode);
    
    // 构造完整文件路径
    snprintf(filepath, sizeof(filepath), "tftp_root/%s", filename);
    
    // 检查文件是否已存在
    FILE* check_file = fopen(filepath, "r");
    if (check_file != NULL) {
        fclose(check_file);
        log_message("ERROR", "File already exists: %s", filepath);
        send_error_packet(sock, client_addr, TFTP_ERROR_FILE_EXISTS, "File already exists");
        return;
    }
    
    // 尝试创建文件
    FILE* file = fopen(filepath, (parse_mode(mode) == MODE_NETASCII) ? "w" : "wb");
    if (file == NULL) {
        log_message("ERROR", "Failed to create file: %s", filepath);
        send_error_packet(sock, client_addr, TFTP_ERROR_ACCESS_VIOLATION, "Failed to create file");
        return;
    }
    
    // 创建新的socket用于数据传输
    SOCKET data_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (data_sock == INVALID_SOCKET) {
        log_message("ERROR", "Failed to create data socket");
        fclose(file);
        remove(filepath); // 删除刚创建的文件
        send_error_packet(sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Server internal error");
        return;
    }
    
    // 绑定到动态端口
    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = INADDR_ANY;
    data_addr.sin_port = 0;
    
    if (bind(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) == SOCKET_ERROR) {
        log_message("ERROR", "Failed to bind data socket");
        closesocket(data_sock);
        fclose(file);
        remove(filepath);
        send_error_packet(sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Server internal error");
        return;
    }
    
    // 设置socket超时
    int timeout = TIMEOUT_SECONDS * 1000;
    setsockopt(data_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    // 发送初始ACK（块号0）表示准备接收数据
    if (send_ack_packet(data_sock, client_addr, 0) < 0) {
        log_message("ERROR", "Failed to send initial ACK");
        closesocket(data_sock);
        fclose(file);
        remove(filepath);
        return;
    }
    
    // 开始接收文件数据
    tftp_stats_t stats = {0};
    time(&stats.start_time);
    
    unsigned short expected_block = 1;
    char recv_buffer[BUFFER_SIZE];
    int transfer_complete = 0;
    
    while (!transfer_complete) {
        struct sockaddr_in recv_addr;
        int recv_addr_len = sizeof(recv_addr);
        
        int recv_result = recvfrom(data_sock, recv_buffer, sizeof(recv_buffer), 0,
                                 (struct sockaddr*)&recv_addr, &recv_addr_len);
        
        if (recv_result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                log_message("ERROR", "Failed to receive data packet: Timeout");
                send_error_packet(data_sock, client_addr, TFTP_ERROR_NOT_DEFINED, "Timeout");
                break;
            } else {
                log_message("ERROR", "Failed to receive data packet: %d", error);
                break;
            }
        }
        
        // 解析数据包
        unsigned short opcode = ntohs(*(unsigned short*)recv_buffer);
        
        if (opcode == TFTP_DATA) {
            unsigned short block_num = ntohs(*(unsigned short*)(recv_buffer + 2));
            char* data = recv_buffer + 4;
            int data_len = recv_result - 4;
            
            if (block_num == expected_block) {
                // 写入数据到文件
                if (fwrite(data, 1, (size_t)data_len, file) != (size_t)data_len) {
                    log_message("ERROR", "Failed to write to file");
                    send_error_packet(data_sock, client_addr, TFTP_ERROR_DISK_FULL, "磁盘已满");
                    break;
                }
                
                stats.bytes_transferred += data_len;
                
                // 发送ACK
                send_ack_packet(data_sock, client_addr, block_num);

                log_message("DEBUG", "Received data packet, block number: %d, size: %d bytes", block_num, data_len);

                expected_block++;
                
                // 如果数据长度小于512字节，说明传输完成
                if (data_len < DATA_SIZE) {
                    transfer_complete = 1;
                    log_message("INFO", "File upload complete: %s", filename);
                }
            } else {
                log_message("WARNING", "Received duplicate or out-of-order packet, block number: %d, expected: %d",
                           block_num, expected_block);
                // 重新发送上一个ACK
                if (block_num == expected_block - 1) {
                    send_ack_packet(data_sock, client_addr, block_num);
                    stats.retransmissions++;
                }
            }
        } else if (opcode == TFTP_ERROR) {
            unsigned short error_code = ntohs(*(unsigned short*)(recv_buffer + 2));
            char* error_msg = recv_buffer + 4;
            log_message("ERROR", "Client send error (code:%d): %s", error_code, error_msg);
            break;
        } else {
            log_message("WARNING", "Received unknown opcode: %d", opcode);
        }
    }
    
    time(&stats.end_time);
    print_throughput(&stats);
    
    fclose(file);
    closesocket(data_sock);
    
    // 如果传输失败，删除部分传输的文件
    if (!transfer_complete) {
        remove(filepath);
        log_message("INFO", "Deleted incomplete file: %s", filepath);
    }
}