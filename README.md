# TFTP服务器实验项目

## 项目概述

这是一个基于Socket编程的TFTP（Trivial File Transfer Protocol）服务器实现，使用C语言编写，适用于Windows平台。项目提供了**单线程版本**和**多线程版本**两种实现，支持并发客户端访问和高性能文件传输。

## 版本特性

### 🔧 单线程版本 (tftp_server.exe)
- 基础TFTP协议实现
- 顺序处理客户端请求
- 完整的协议支持和错误处理
- 适合学习和理解TFTP协议

### ⚡ 多线程版本 (tftp_server_mt.exe) - **推荐**
- **并发处理**: 支持多个客户端同时访问
- **线程安全**: 独立线程处理每个客户端请求
- **自动管理**: 线程自动创建和销毁
- **增强日志**: 线程安全的日志系统
- **高性能**: 显著提升并发处理能力

## 实验要求

本项目完全满足实验要求：
- ✅ 严格按照TFTP协议与标准TFTP客户端通信
- ✅ 支持netascii和octet两种传输模式
- ✅ 支持文件上传（PUT）和下载（GET）
- ✅ 显示文件操作结果（成功/失败）
- ✅ 提示失败的具体原因
- ✅ 显示文件传输吞吐量
- ✅ 完整的日志记录功能
- ✅ 友好的命令行界面
- ✅ **并发客户端支持**（多线程版本）
- ✅ **线程安全设计**（多线程版本）
- ✅ 纯Socket编程实现，无第三方库依赖

## 项目结构

```
project/
├── src/                    # 源代码目录
│   ├── main.c             # 单线程服务器主程序
│   ├── tftp_server_mt.c   # 多线程服务器主程序
│   ├── tftp_utils.c       # TFTP工具函数
│   ├── tftp_handlers.c    # TFTP协议处理器
│   └── gui_app.c          # 图形化监控与控制面板
├── include/               # 头文件目录
│   └── tftp.h            # TFTP协议定义和函数声明
├── docs/                  # 文档目录
│   ├── experiment_record.md      # 实验记录
│   ├── 代码注释说明.md           # 代码注释详细说明
│   └── 复现实验操作指南.md       # 完整实验复现指南
├── tools/                 # 测试工具目录
│   ├── lossy_rrq.c       # 丢包测试客户端源码
│   └── lossy_rrq.exe     # 丢包测试客户端
├── tftp_root/            # TFTP服务器根目录
│   ├── config.txt        # 配置文件示例
│   └── test.txt          # 测试文件
├── logs/                 # 日志文件目录
│   ├── tftp_server.log   # 单线程版本日志
│   └── tftp_server_mt.log # 多线程版本日志
├── build/                # 编译输出目录
├── client_workspace/     # 客户端工作目录
├── Makefile              # Makefile编译脚本
├── build.bat             # 单线程版本编译脚本
├── build_mt.bat          # 多线程版本编译脚本
├── compile.bat           # 通用编译脚本
├── test.bat              # 基础测试脚本
├── test_concurrent.bat   # 并发测试脚本
├── tftp_server.exe       # 单线程服务器可执行文件
├── tftp_server_mt.exe    # 多线程服务器可执行文件
├── tftp_gui.exe          # 图形化控制面板
├── README.md             # 项目说明文档（本文件）
├── README_MT.md          # 多线程版本详细说明
└── instruction.md        # 使用说明
```

## 编译和运行

### 快速开始（推荐）

```bash
# 编译所有版本
.\compile.bat

# 运行多线程版本（推荐）
.\tftp_server_mt.exe

# 运行单线程版本
.\tftp_server.exe
```

### 分别编译

#### 单线程版本
```bash
# 使用批处理脚本
.\build.bat

# 或使用Makefile
make tftp_server
```

#### 多线程版本
```bash
# 使用批处理脚本
.\build_mt.bat

# 或使用Makefile
make tftp_server_mt
```

### 手动编译

#### 单线程版本
```bash
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/main.c -o build/main.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_utils.c -o build/tftp_utils.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_handlers.c -o build/tftp_handlers.o
gcc build/main.o build/tftp_utils.o build/tftp_handlers.o -o tftp_server.exe -lws2_32
```

#### 多线程版本
```bash
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_server_mt.c -o build/tftp_server_mt.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_utils.c -o build/tftp_utils.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_handlers.c -o build/tftp_handlers.o
gcc build/tftp_server_mt.o build/tftp_utils.o build/tftp_handlers.o -o tftp_server_mt.exe -lws2_32
```

## 使用说明

### 启动服务器

#### 多线程版本（推荐）
```bash
# 启动多线程TFTP服务器
.\tftp_server_mt.exe

# 指定端口（可选，默认69）
.\tftp_server_mt.exe 2069
```

**多线程版本特性：**
- 支持多个客户端同时连接
- 每个客户端请求在独立线程中处理
- 线程安全的日志记录
- 自动线程管理和资源清理

#### 单线程版本
```bash
# 启动单线程TFTP服务器
.\tftp_server.exe
```

### 并发测试

项目提供了专门的并发测试脚本：

```bash
# 运行并发测试（需要先启动多线程服务器）
.\test_concurrent.bat
```

测试脚本会：
1. 创建多个测试文件
2. 同时启动多个客户端进行上传/下载
3. 验证并发处理能力
4. 显示测试结果和性能统计

### 启动图形化监控面板

```bash
.\tftp_gui.exe
```

该面板提供：
- 一键启动/停止服务器
- 服务器文件与客户端缓存列表
- 上传、下载操作入口（支持多线程并发，后台管道显示执行结果）
- 实时日志、吞吐量与错误提示
- 客户端访问行为列表

GUI 会在工程目录下自动创建 `client_workspace/` 作为客户端缓存目录，所有下载文件默认保存到该目录。

### 使用客户端测试

#### 下载文件

```bash
# 从服务器下载test.txt文件
tftp -i 127.0.0.1 get test.txt

# 下载并重命名
tftp -i 127.0.0.1 get test.txt downloaded_file.txt
```

#### 上传文件

```bash
# 上传文件到服务器
tftp -i 127.0.0.1 put local_file.txt remote_file.txt
```

### 目录说明

- `tftp_root/`: 服务器文件根目录，所有上传的文件保存在这里，下载的文件也从这里读取
- `logs/`: 日志文件目录，服务器运行日志保存在 `tftp_server.log`

## 功能特性

### TFTP协议支持

- **RRQ（读请求）**: 客户端下载文件
- **WRQ（写请求）**: 客户端上传文件
- **DATA（数据包）**: 文件数据传输
- **ACK（确认包）**: 数据包确认
- **ERROR（错误包）**: 错误信息传输

### 传输模式

- **netascii**: ASCII文本模式
- **octet**: 二进制模式（推荐）

### 错误处理

- 文件未找到
- 访问违规
- 磁盘已满
- 非法操作
- 未知传输ID
- 文件已存在

### 高级功能

#### 单线程版本
- **超时重传机制**: 支持数据包丢失后的自动重传
- **传输统计**: 显示传输字节数、耗时和吞吐量
- **详细日志**: 记录所有操作、错误和传输统计
- **多客户端支持**: 每个传输使用独立的socket端口

#### 多线程版本（额外特性）
- **并发处理**: 多个客户端可同时进行文件传输
- **线程安全**: 所有共享资源都有适当的同步机制
- **自动线程管理**: 线程自动创建、执行和清理
- **增强日志**: 线程安全的日志系统，支持并发写入
- **性能优化**: 显著提升多客户端场景下的响应速度

#### 性能对比

| 特性 | 单线程版本 | 多线程版本 |
|------|------------|------------|
| 并发客户端 | 顺序处理 | 同时处理 |
| 响应时间 | 较慢（排队等待） | 快速（并行处理） |
| 资源利用率 | 低 | 高 |
| 适用场景 | 学习、单客户端 | 生产、多客户端 |

## 代码结构说明

### 核心文件

1. **tftp.h**: 定义了TFTP协议的所有数据结构、常量和函数声明
2. **main.c**: 单线程服务器主程序，包含服务器主循环和数据包解析
3. **tftp_server_mt.c**: 多线程服务器主程序，实现并发客户端处理
4. **tftp_utils.c**: 工具函数，包含网络初始化、日志记录、数据包发送等
5. **tftp_handlers.c**: 协议处理器，实现RRQ和WRQ的具体逻辑
6. **gui_app.c**: 图形化监控与控制面板

### 多线程实现要点

- **线程模型**: 主线程监听连接，为每个客户端创建独立工作线程
- **线程安全**: 使用Windows Critical Section保护共享资源
- **资源管理**: 线程自动清理socket和文件资源
- **日志同步**: 线程安全的日志记录机制

### 关键设计

- **状态机设计**: 每个传输会话维护独立的状态
- **异步处理**: 使用UDP socket实现非阻塞通信
- **资源管理**: 自动清理文件句柄和socket资源
- **安全检查**: 防止路径遍历和文件覆盖

## 测试方法

### 基础测试

1. 启动服务器：
   ```bash
   # 多线程版本（推荐）
   .\tftp_server_mt.exe
   
   # 或单线程版本
   .\tftp_server.exe
   ```

2. 将测试文件放入 `tftp_root/` 目录

3. 使用Windows自带的TFTP客户端测试：
   ```bash
   # 下载文件
   tftp -i 127.0.0.1 get test.txt
   
   # 上传文件
   tftp -i 127.0.0.1 put local_file.txt remote_file.txt
   ```

4. 查看日志了解详细运行情况：
   ```bash
   # 多线程版本日志
   type logs\tftp_server_mt.log
   
   # 单线程版本日志
   type logs\tftp_server.log
   ```

### 并发测试

使用提供的并发测试脚本：

```bash
# 1. 启动多线程服务器
.\tftp_server_mt.exe

# 2. 在另一个终端运行并发测试
.\test_concurrent.bat
```

测试包括：
- 同时下载多个文件
- 同时上传多个文件
- 混合并发操作
- 错误处理测试

### 丢包测试

使用专门的丢包测试工具：

```bash
# 编译丢包测试客户端
gcc tools\lossy_rrq.c -o tools\lossy_rrq.exe -lws2_32

# 启动服务器后运行丢包测试
.\tools\lossy_rrq.exe
```

### 完整实验复现

参考 `docs/复现实验操作指南.md` 进行完整的实验验证，包括：
- 正常上传/下载测试
- 异常环境可靠传输测试
- 吞吐量测试
- 日志记录验证

## 常见问题

### 编译错误

- 确保安装了MinGW或其他GCC编译器
- 检查include路径是否正确
- 确保链接了ws2_32库（Windows Socket库）

### 运行错误

- 确保69端口未被占用（或以管理员权限运行）
- 检查防火墙设置
- 确保 `tftp_root/` 目录有读写权限

### 连接问题

- 检查客户端是否使用正确的IP地址
- 确认服务器正在监听69端口
- 查看日志文件排查具体问题

## 实验报告要素

本项目包含实验报告所需的所有要素：

1. **程序流程图**: 可根据代码结构绘制
2. **详细注释**: 所有函数都有完整的中文注释（参见 `docs/代码注释说明.md`）
3. **编译说明**: 提供多种编译方式
4. **源代码**: 结构清晰，模块化设计
5. **测试说明**: 详细的使用和测试方法
6. **实验记录**: 完整的实验过程记录（参见 `docs/experiment_record.md`）
7. **操作指南**: 详细的复现步骤（参见 `docs/复现实验操作指南.md`）
8. **多线程实现**: 高级并发处理能力展示
9. **性能对比**: 单线程vs多线程性能分析

## 文档说明

- **README.md**: 项目总体说明（本文件）
- **README_MT.md**: 多线程版本详细技术说明
- **docs/experiment_record.md**: 实验过程记录
- **docs/代码注释说明.md**: 代码注释详细说明
- **docs/复现实验操作指南.md**: 完整实验复现指南
- **instruction.md**: 基础使用说明

## 作者

计算机网络实验项目 - TFTP服务器实现（单线程 + 多线程版本）

