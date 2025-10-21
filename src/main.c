#include "../include/tftp.h"

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
            int data_len = buffer_len - 4;
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

/**
 * 显示TFTP服务器使用帮助和配置信息
 * 
 * 功能说明：
 * - 打印服务器功能特性介绍
 * - 显示服务器配置参数
 * - 提供客户端使用示例
 * - 说明操作方法
 */
void show_help(void) {
    printf("\n=== TFTP Server ===\n");
    printf("A simple TFTP server implementation supporting file upload and download.\n\n");
    
    printf("Supported features:\n");
    printf("  - File download (RRQ)\n");                   // 支持文件下载
    printf("  - File upload (WRQ)\n");                     // 支持文件上传
    printf("  - netascii and octet transfer modes\n");     // 支持两种传输模式
    printf("  - Error handling and retransmission\n");     // 错误处理和重传机制
    printf("  - Transfer statistics and logging\n\n");     // 传输统计和日志功能
    
    printf("Server configuration:\n");
    printf("  - Listen port: %d\n", TFTP_PORT);            // 监听端口
    printf("  - File root directory: tftp_root/\n");       // 文件根目录
    printf("  - Log file: logs/tftp_server.log\n\n");      // 日志文件位置
    
    printf("Connect using standard TFTP client, for example:\n");
    printf("  tftp -i 127.0.0.1 get filename.txt\n");      // 下载文件示例
    printf("  tftp -i 127.0.0.1 put filename.txt\n\n");    // 上传文件示例
    printf("Press Ctrl+C to stop server\n");                // 停止服务器的方法
    printf("==================\n\n");
}

/**
 * Windows控制台信号处理函数
 * 
 * 功能说明：
 * - 处理Ctrl+C等控制台中断信号
 * - 优雅地关闭服务器和清理资源
 * - 防止程序异常退出导致资源泄露
 * 
 * 参数：
 * - signal: 接收到的信号类型
 * 
 * 返回值：
 * - TRUE: 信号已处理
 * - FALSE: 信号未处理，传递给默认处理器
 */
BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        printf("\nReceived stop signal, shutting down server...\n");
        cleanup_winsock();                               // 清理网络资源
        exit(0);                                         // 正常退出程序
    }
    return TRUE;                                         // 信号已处理
}

/**
 * 程序主入口函数
 * 
 * 功能说明：
 * - 初始化服务器环境和网络库
 * - 创建并配置TFTP服务器套接字
 * - 进入主服务循环，处理客户端请求
 * - 根据TFTP协议分发不同类型的请求
 * 
 * TFTP服务器工作流程：
 * 1. 初始化Winsock库
 * 2. 创建UDP套接字并绑定到端口69
 * 3. 循环接收客户端请求
 * 4. 解析TFTP协议包
 * 5. 根据操作码调用相应处理函数
 * 6. 记录操作日志
 * 
 * 参数：
 * - argc: 命令行参数个数
 * - argv: 命令行参数数组
 * 
 * 返回值：
 * - 0: 正常退出
 * - 1: 初始化失败退出
 */
int main(int argc, char* argv[]) {
    // 避免编译器对未使用参数的警告
    (void)argc;
    (void)argv;
    
    printf("TFTP Server starting...\n");
    
    // 设置Windows控制台信号处理器
    // 用于捕获Ctrl+C等中断信号，实现优雅退出
    if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
        printf("Warning: Unable to set signal handler\n");
    }
    
    // 显示服务器帮助信息和配置
    show_help();
    
    // 初始化Windows Socket网络库
    init_winsock();
    
    // 创建并配置TFTP服务器套接字
    SOCKET server_sock = create_tftp_socket();
    if (server_sock == INVALID_SOCKET) {
        cleanup_winsock();                               // 清理资源
        return 1;                                        // 返回错误码
    }
    
    // 确保必要的目录结构存在
    CreateDirectory("tftp_root", NULL);                  // 创建文件根目录
    CreateDirectory("logs", NULL);                       // 创建日志目录
    
    log_message("INFO", "TFTP server started successfully, waiting for client connections...");
    
    // 创建测试文件（如果不存在）
    // 为客户端提供一个可以下载的示例文件
    FILE* test_file = fopen("tftp_root/test.txt", "r");
    if (test_file == NULL) {
        test_file = fopen("tftp_root/test.txt", "w");
        if (test_file != NULL) {
            fprintf(test_file, "Hello, this is a test file for TFTP server!\n");
            fprintf(test_file, "You can download this file using TFTP client.\n");
            fclose(test_file);
            log_message("INFO", "Created test file: tftp_root/test.txt");
        }
    } else {
        fclose(test_file);
    }
    
    // 准备主服务循环的变量
    char buffer[BUFFER_SIZE];                            // UDP数据接收缓冲区
    struct sockaddr_in client_addr;                      // 客户端地址信息
    int client_addr_len = sizeof(client_addr);           // 地址结构长度
    
    // 主服务循环：持续监听和处理客户端请求
    while (1) {
        // 从UDP套接字接收客户端数据包
        // recvfrom函数会阻塞等待，直到收到数据或发生错误
        int recv_result = recvfrom(server_sock, buffer, sizeof(buffer), 0,
                                 (struct sockaddr*)&client_addr, &client_addr_len);
        
        // 检查接收是否成功
        if (recv_result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEINTR) {                     // 忽略中断信号错误
                log_message("ERROR", "Failed to receive data: %d", error);
            }
            continue;                                    // 继续监听下一个请求
        }
        
        // 检查是否收到空数据包
        if (recv_result == 0) {
            log_message("WARNING", "Received empty packet");
            continue;
        }
        
        // 解析接收到的TFTP协议数据包
        tftp_packet_t packet;
        if (parse_tftp_packet(buffer, recv_result, &packet) < 0) {
            log_message("WARNING", "Received invalid TFTP packet");
            // 发送错误包通知客户端数据包格式有误
            send_error_packet(server_sock, &client_addr, 
                            TFTP_ERROR_ILLEGAL_OPERATION, "Invalid packet format");
            continue;
        }
        
        // 记录客户端连接和请求信息
        log_message("INFO", "Client connection: %s:%d, opcode: %d", 
                   inet_ntoa(client_addr.sin_addr), 
                   ntohs(client_addr.sin_port), 
                   packet.opcode);
        
        // 根据TFTP操作码分发请求到相应的处理函数
        switch (packet.opcode) {
            case TFTP_RRQ:
                // 处理读请求（客户端下载文件）
                handle_rrq(server_sock, &packet, &client_addr);
                break;
                
            case TFTP_WRQ:
                // 处理写请求（客户端上传文件）
                handle_wrq(server_sock, &packet, &client_addr);
                break;
                
            case TFTP_DATA:
            case TFTP_ACK:
                // DATA和ACK包不应该发送到主服务器端口（69端口）
                // 这些包应该发送到数据传输端口
                log_message("WARNING", "Received unexpected %s packet", 
                           (packet.opcode == TFTP_DATA) ? "DATA" : "ACK");
                send_error_packet(server_sock, &client_addr, 
                                TFTP_ERROR_UNKNOWN_TID, "Unknown transfer ID");
                break;
                
            case TFTP_ERROR:
                // 记录客户端发送的错误信息
                log_message("INFO", "Client reported error: %s", packet.error.error_msg);
                break;
                
            default:
                // 处理未知的操作码
                log_message("WARNING", "Received unknown opcode: %d", packet.opcode);
                send_error_packet(server_sock, &client_addr, 
                                TFTP_ERROR_ILLEGAL_OPERATION, "Unsupported operation");
                break;
        }
    }
    
    // 程序结束时清理资源（实际上由于无限循环，这部分代码不会执行）
    closesocket(server_sock);                            // 关闭服务器套接字
    cleanup_winsock();                                   // 清理Winsock库
    
    return 0;                                            // 正常退出
}