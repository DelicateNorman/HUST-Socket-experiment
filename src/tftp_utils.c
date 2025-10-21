#include "../include/tftp.h"

// 全局变量：日志文件指针，用于记录服务器运行日志
static FILE* log_file = NULL;

/**
 * 初始化Windows Socket库
 * 
 * 功能说明：
 * - 初始化Winsock 2.2版本
 * - 检查初始化是否成功
 * - 如果失败则退出程序
 */
void init_winsock(void) {
    WSADATA wsa_data;                                    // Winsock数据结构
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);  // 初始化Winsock 2.2
    
    if (result != 0) {
        printf("WSAStartup failed, error code: %d\n", result);
        exit(1);                                         // 初始化失败，退出程序
    }
    printf("Winsock initialized successfully\n");
}

/**
 * 清理Windows Socket库资源
 * 
 * 功能说明：
 * - 释放Winsock库资源
 * - 关闭日志文件
 * - 在程序退出时调用
 */
void cleanup_winsock(void) {
    WSACleanup();                    // 清理Winsock库
    if (log_file) {
        fclose(log_file);            // 关闭日志文件
        log_file = NULL;             // 重置文件指针
    }
}

/**
 * 创建TFTP服务器套接字
 * 
 * 功能说明：
 * - 创建UDP套接字用于TFTP通信
 * - 设置套接字选项允许地址重用
 * - 绑定到TFTP默认端口69
 * - 处理端口占用等错误情况
 * 
 * 返回值：
 * - 成功：返回套接字描述符
 * - 失败：返回-1
 */
int create_tftp_socket(void) {
    // 创建UDP套接字，TFTP协议基于UDP
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        printf("Failed to create socket, error code: %d\n", WSAGetLastError());
        return -1;
    }

    // 设置套接字选项，允许地址重用（避免TIME_WAIT状态影响）
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        printf("Warning: Failed to set SO_REUSEADDR\n");
    }

    // 设置服务器地址结构
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;           // IPv4地址族
    server_addr.sin_addr.s_addr = INADDR_ANY;   // 绑定到所有可用网络接口
    server_addr.sin_port = htons(TFTP_PORT);    // 绑定到TFTP标准端口69

    // 将套接字绑定到指定地址和端口
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        printf("Failed to bind socket, error code: %d\n", error);
        
        // 特殊处理端口占用错误（错误码10048）
        if (error == 10048) {
            printf("Error: Port %d is already in use.\n", TFTP_PORT);
            printf("Please make sure no other TFTP server is running, or\n");
            printf("close any application using port %d.\n", TFTP_PORT);
        }
        closesocket(sock);
        return -1;
    }

    printf("TFTP server started successfully, listening on port: %d\n", TFTP_PORT);
    return sock;
}

/**
 * 记录日志信息到文件和控制台
 * 
 * 功能说明：
 * - 支持格式化字符串输出（类似printf）
 * - 同时输出到控制台和日志文件
 * - 自动添加时间戳和日志级别
 * - 如果日志文件不存在会自动创建
 * 
 * 参数：
 * - level: 日志级别（如"INFO", "ERROR", "DEBUG"等）
 * - message: 格式化字符串
 * - ...: 可变参数列表
 */
void log_message(const char* level, const char* message, ...) {
    // 如果日志文件还未打开，则以追加模式打开
    if (log_file == NULL) {
        log_file = fopen("logs/tftp_server.log", "a");
        if (log_file == NULL) {
            printf("Can't open Log File!\n");
            return;
        }
    }

    // 获取当前系统时间
    time_t now;
    time(&now);
    char time_str[64];
    // 格式化时间字符串：YYYY-MM-DD HH:MM:SS
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // 处理可变参数列表
    va_list args;
    va_start(args, message);
    
    // 输出到控制台（带时间戳和级别）
    printf("[%s] [%s] ", time_str, level);
    vprintf(message, args);      // 使用vprintf处理可变参数
    printf("\n");
    
    // 输出到日志文件（格式相同）
    fprintf(log_file, "[%s] [%s] ", time_str, level);
    vfprintf(log_file, message, args);   // 使用vfprintf处理可变参数
    fprintf(log_file, "\n");
    fflush(log_file);                    // 立即刷新缓冲区，确保日志及时写入
    
    va_end(args);                        // 清理可变参数列表
}

/**
 * 解析TFTP传输模式字符串
 * 
 * 功能说明：
 * - 将字符串形式的传输模式转换为枚举值
 * - 支持不区分大小写的比较
 * - TFTP标准定义的两种模式：netascii（文本）和octet（二进制）
 * 
 * 参数：
 * - mode_str: 模式字符串（"netascii" 或 "octet"）
 * 
 * 返回值：
 * - MODE_NETASCII: ASCII文本模式
 * - MODE_OCTET: 二进制模式（默认）
 */
tftp_mode_t parse_mode(const char* mode_str) {
    if (strcasecmp(mode_str, "netascii") == 0) {
        return MODE_NETASCII;        // ASCII文本模式
    } else if (strcasecmp(mode_str, "octet") == 0) {
        return MODE_OCTET;           // 二进制模式
    }
    return MODE_OCTET;               // 默认使用二进制模式，兼容性更好
}

/**
 * 获取TFTP错误码对应的错误消息
 * 
 * 功能说明：
 * - 将TFTP协议定义的错误码转换为可读的错误消息
 * - 用于构造错误包发送给客户端
 * - 符合RFC 1350标准定义的错误类型
 * 
 * 参数：
 * - error_code: TFTP错误码枚举值
 * 
 * 返回值：
 * - 对应的英文错误消息字符串
 */
const char* get_error_message(tftp_error_code_t error_code) {
    switch (error_code) {
        case TFTP_ERROR_NOT_DEFINED:
            return "Undefined Error";           // 未定义错误
        case TFTP_ERROR_FILE_NOT_FOUND:
            return "File Not Found";            // 文件未找到
        case TFTP_ERROR_ACCESS_VIOLATION:
            return "Error Access Violation";    // 访问违规
        case TFTP_ERROR_DISK_FULL:
            return "FUll DISK";                 // 磁盘空间不足
        case TFTP_ERROR_ILLEGAL_OPERATION:
            return "illegal operation";         // 非法操作
        case TFTP_ERROR_UNKNOWN_TID:
            return "unknown TID";               // 未知传输标识符
        case TFTP_ERROR_FILE_EXISTS:
            return "FILE EXISTS";               // 文件已存在
        case TFTP_ERROR_NO_SUCH_USER:
            return "User Not Found";            // 用户不存在
        default:
            return "Unknown Error";             // 未知错误类型
    }
}

/**
 * 发送TFTP错误包给客户端
 * 
 * 功能说明：
 * - 构造TFTP错误包格式（操作码 + 错误码 + 错误消息）
 * - 通过UDP发送给指定客户端
 * - 记录发送结果到日志
 * 
 * TFTP错误包格式：
 * |操作码(2字节)|错误码(2字节)|错误消息(变长)|终止符(1字节)|
 * 
 * 参数：
 * - sock: 发送套接字
 * - client_addr: 客户端地址结构
 * - error_code: TFTP错误码
 * - error_msg: 错误消息字符串
 * 
 * 返回值：
 * - 成功：0
 * - 失败：-1
 */
int send_error_packet(SOCKET sock, struct sockaddr_in* client_addr, 
                     tftp_error_code_t error_code, const char* error_msg) {
    char buffer[BUFFER_SIZE];
    // 网络字节序的TFTP错误操作码
    unsigned short opcode = htons(TFTP_ERROR);
    // 网络字节序的错误码
    unsigned short err_code = htons((unsigned short)error_code);
    
    // 构造错误包：操作码(2) + 错误码(2) + 错误消息 + 终止符
    memcpy(buffer, &opcode, 2);                          // 复制操作码
    memcpy(buffer + 2, &err_code, 2);                    // 复制错误码
    strcpy(buffer + 4, error_msg);                       // 复制错误消息（包含终止符）
    
    int packet_size = 4 + strlen(error_msg) + 1;         // 计算包总长度
    
    // 通过UDP发送错误包
    int result = sendto(sock, buffer, packet_size, 0, 
                       (struct sockaddr*)client_addr, sizeof(*client_addr));
    
    if (result == SOCKET_ERROR) {
        log_message("ERROR", "Failed to send error packet: %d", WSAGetLastError());
        return -1;
    }

    log_message("INFO", "Sent error packet: %s", error_msg);
    return 0;
}

/**
 * 发送TFTP确认（ACK）包给客户端
 * 
 * 功能说明：
 * - 构造TFTP ACK包格式（操作码 + 块号）
 * - 用于确认收到的数据包或响应写请求
 * - ACK包总是4字节固定长度
 * 
 * TFTP ACK包格式：
 * |操作码(2字节)|块号(2字节)|
 * 
 * 参数：
 * - sock: 发送套接字
 * - client_addr: 客户端地址结构
 * - block_num: 要确认的数据块号
 * 
 * 返回值：
 * - 成功：0
 * - 失败：-1
 */
int send_ack_packet(SOCKET sock, struct sockaddr_in* client_addr, 
                   unsigned short block_num) {
    char buffer[4];                                       // ACK包固定4字节
    // 网络字节序的ACK操作码
    unsigned short opcode = htons(TFTP_ACK);
    // 网络字节序的块号
    unsigned short block = htons(block_num);
    
    // 构造ACK包：操作码(2) + 块号(2)
    memcpy(buffer, &opcode, 2);                          // 复制操作码
    memcpy(buffer + 2, &block, 2);                       // 复制块号
    
    // 通过UDP发送ACK包
    int result = sendto(sock, buffer, 4, 0, 
                       (struct sockaddr*)client_addr, sizeof(*client_addr));
    
    if (result == SOCKET_ERROR) {
        log_message("ERROR", "Failed to send ACK packet: %d", WSAGetLastError());
        return -1;
    }

    log_message("DEBUG", "Sent ACK packet, block number: %d", block_num);
    return 0;
}

/**
 * 发送TFTP数据包给客户端
 * 
 * 功能说明：
 * - 构造TFTP数据包格式（操作码 + 块号 + 数据）
 * - 用于向客户端发送文件数据
 * - 支持可变长度数据（最大512字节）
 * 
 * TFTP数据包格式：
 * |操作码(2字节)|块号(2字节)|数据(0-512字节)|
 * 
 * 参数：
 * - sock: 发送套接字
 * - client_addr: 客户端地址结构
 * - block_num: 数据块号（从1开始）
 * - data: 要发送的数据缓冲区
 * - data_len: 数据长度（0-512字节）
 * 
 * 返回值：
 * - 成功：0
 * - 失败：-1
 */
int send_data_packet(SOCKET sock, struct sockaddr_in* client_addr, 
                    unsigned short block_num, char* data, int data_len) {
    char buffer[BUFFER_SIZE];                            // 最大516字节缓冲区
    // 网络字节序的数据操作码
    unsigned short opcode = htons(TFTP_DATA);
    // 网络字节序的块号
    unsigned short block = htons(block_num);
    
    // 构造数据包：操作码(2) + 块号(2) + 数据(变长)
    memcpy(buffer, &opcode, 2);                          // 复制操作码
    memcpy(buffer + 2, &block, 2);                       // 复制块号
    memcpy(buffer + 4, data, data_len);                  // 复制实际数据
    
    int packet_size = 4 + data_len;                      // 计算包总长度
    
    // 通过UDP发送数据包
    int result = sendto(sock, buffer, packet_size, 0, 
                       (struct sockaddr*)client_addr, sizeof(*client_addr));
    
    if (result == SOCKET_ERROR) {
        log_message("ERROR", "Failed to send data packet: %d", WSAGetLastError());
        return -1;
    }

    log_message("DEBUG", "Sent data packet, block number: %d, size: %d bytes", block_num, data_len);
    return 0;
}

/**
 * 计算并显示文件传输吞吐量统计信息
 * 
 * 功能说明：
 * - 计算文件传输的总耗时
 * - 计算平均传输速度（字节/秒）
 * - 显示重传次数（如果有）
 * - 输出详细的传输统计报告
 * 
 * 参数：
 * - stats: 传输统计信息结构指针，包含：
 *   - bytes_transferred: 总传输字节数
 *   - start_time: 传输开始时间
 *   - end_time: 传输结束时间
 *   - retransmissions: 重传次数
 */
void print_throughput(tftp_stats_t* stats) {
    // 计算传输总耗时（秒）
    double duration = difftime(stats->end_time, stats->start_time);
    
    if (duration > 0) {
        // 计算平均吞吐量（字节/秒）
        double throughput = (double)stats->bytes_transferred / duration;
        
        // 输出传输统计信息
        log_message("INFO", "Transfer statistics: %zu bytes, duration: %.2f seconds, throughput: %.2f bytes/second", 
                   stats->bytes_transferred, duration, throughput);
        
        // 如果有重传，单独显示重传次数
        if (stats->retransmissions > 0) {
            log_message("INFO", "Retransmissions: %d", stats->retransmissions);
        }
    }
}