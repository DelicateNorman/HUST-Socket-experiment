#define _WIN32_WINNT 0x0600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/*
 * 该示例程序用于向本地 TFTP 服务器发送 RRQ 请求，并故意丢弃第一个数据块的 ACK。
 * 实现思路：
 *   1. 先发送 RRQ 请求，等待服务器返回 DATA block #1。
 *   2. 第一次收到 block #1 时不发送 ACK，模拟网络丢包。
 *   3. 服务器检测超时后会自动重传 block #1。
 *   4. 客户端在第二次收到相同的数据块时正常发送 ACK，并继续后续传输。
 * 这样就能触发服务器的重传机制，方便验证异常环境下的可靠传输实现。
 */

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 516
#define DATA_SIZE 512

static void die(const char* msg) {
    fprintf(stderr, "%s (error=%d)\n", msg, WSAGetLastError());
    WSACleanup();
    exit(EXIT_FAILURE);
}

int main(void) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return EXIT_FAILURE;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        die("socket failed");
    }

    // 构造RRQ请求: opcode(01) + filename + 0 + mode + 0
    char rrq[BUFFER_SIZE];
    const char* filename = "test.txt";
    const char* mode = "octet";
    size_t pos = 0;
    unsigned short opcode = htons(1); // RRQ
    memcpy(rrq + pos, &opcode, 2); pos += 2;
    strcpy(rrq + pos, filename); pos += strlen(filename) + 1;
    strcpy(rrq + pos, mode); pos += strlen(mode) + 1;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(69);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) != 1) {
        die("inet_pton failed");
    }

    printf("Sending RRQ for %s...\n", filename);
    if (sendto(sock, rrq, (int)pos, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        die("sendto failed");
    }

    // 设置接收超时，方便等待重传
    int timeout_ms = 8000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        die("setsockopt failed");
    }

    int drop_first_ack = 1;
    unsigned short expected_block = 1;
    char buffer[BUFFER_SIZE];

    while (1) {
        struct sockaddr_in data_sender;
        int sender_len = sizeof(data_sender);
        int received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                (struct sockaddr*)&data_sender, &sender_len);
        if (received == SOCKET_ERROR) {
            die("recvfrom failed");
        }

        unsigned short pkt_opcode = ntohs(*(unsigned short*)buffer);
        if (pkt_opcode != 3) { // expect DATA
            printf("Unexpected opcode: %u\n", pkt_opcode);
            break;
        }

        unsigned short block = ntohs(*(unsigned short*)(buffer + 2));
        int data_len = received - 4;
        printf("Received DATA block %u (%d bytes)\n", block, data_len);

        if (drop_first_ack && block == 1) {
              // ===================== 丢包模拟核心 =====================
              // 第一次收到 block 1 的数据包时，故意不发送 ACK，直接丢弃。
              // 这样服务器会在超时后自动重传 block 1。
              // 客户端第二次收到 block 1 后才发送 ACK，后续流程恢复正常。
              // 通过这种方式，可以验证服务器的重传机制和异常处理能力。
              printf("Simulating packet loss: dropping ACK for block 1\n"); 
              // ===================== 丢包模拟核心 =====================
            drop_first_ack = 0;
            continue; // 不发送ACK，等待服务器超时重传
        }

        unsigned short ack_opcode = htons(4);
        unsigned short ack_block = htons(block);
        char ack[4];
        memcpy(ack, &ack_opcode, 2);
        memcpy(ack + 2, &ack_block, 2);

        if (sendto(sock, ack, sizeof(ack), 0,
                   (struct sockaddr*)&data_sender, sender_len) == SOCKET_ERROR) {
            die("sendto ACK failed");
        }
        printf("Sent ACK for block %u\n", block);

        if (block == expected_block) {
            ++expected_block;
        }

        if (data_len < DATA_SIZE) {
            printf("Transfer complete.\n");
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
