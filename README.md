# TFTP服务器实验项目

## 项目概述

这是一个基于Socket编程的TFTP（Trivial File Transfer Protocol）服务器实现，使用C语言编写，适用于Windows平台。

## 实验要求

本项目完全满足实验要求：
- ✅ 严格按照TFTP协议与标准TFTP客户端通信
- ✅ 支持netascii和octet两种传输模式
- ✅ 支持文件上传（PUT）
- ✅ 支持文件下载（GET）
- ✅ 显示文件操作结果（成功/失败）
- ✅ 提示失败的具体原因
- ✅ 显示文件传输吞吐量
- ✅ 完整的日志记录功能
- ✅ 友好的命令行界面
- ✅ 纯Socket编程实现，无第三方库依赖

## 项目结构

```
project/
├── src/                    # 源代码目录
│   ├── main.c             # 主程序入口
│   ├── tftp_utils.c       # TFTP工具函数
│   ├── tftp_handlers.c    # TFTP协议处理器
│   └── gui_app.c          # 图形化监控与控制面板
├── include/               # 头文件目录
│   └── tftp.h            # TFTP协议定义和函数声明
├── docs/                  # 文档目录
│   └── experiment_record.md # 实验记录
├── tftp_root/            # TFTP服务器根目录
├── logs/                 # 日志文件目录
├── build/                # 编译输出目录
├── Makefile              # Makefile编译脚本
├── build.bat             # Windows批处理编译脚本
└── README.md             # 项目说明文档
```

## 编译和运行

### 方法1：使用批处理脚本（推荐）

```bash
# 双击运行或在命令行执行
build.bat
```

脚本会在项目根目录生成 `tftp_server.exe`（命令行服务器）和 `tftp_gui.exe`（图形化控制面板）。

### 方法2：使用Makefile

```bash
# 如果安装了make工具
make all
```

### 方法3：手动编译

```bash
# 创建构建目录
mkdir build

# 编译源文件
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/main.c -o build/main.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_utils.c -o build/tftp_utils.o
gcc -Wall -Wextra -std=c99 -g -Iinclude -c src/tftp_handlers.c -o build/tftp_handlers.o

# 链接生成可执行文件
gcc build/main.o build/tftp_utils.o build/tftp_handlers.o -o tftp_server.exe -lws2_32
```

## 使用说明

### 启动服务器

```bash
tftp_server.exe
```

服务器将监听69端口，等待客户端连接。

### 启动图形化监控面板

```bash
tftp_gui.exe
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

- **超时重传机制**: 支持数据包丢失后的自动重传
- **传输统计**: 显示传输字节数、耗时和吞吐量
- **详细日志**: 记录所有操作、错误和传输统计
- **多客户端支持**: 每个传输使用独立的socket端口

## 代码结构说明

### 核心文件

1. **tftp.h**: 定义了TFTP协议的所有数据结构、常量和函数声明
2. **main.c**: 主程序，包含服务器主循环和数据包解析
3. **tftp_utils.c**: 工具函数，包含网络初始化、日志记录、数据包发送等
4. **tftp_handlers.c**: 协议处理器，实现RRQ和WRQ的具体逻辑

### 关键设计

- **状态机设计**: 每个传输会话维护独立的状态
- **异步处理**: 使用UDP socket实现非阻塞通信
- **资源管理**: 自动清理文件句柄和socket资源
- **安全检查**: 防止路径遍历和文件覆盖

## 测试方法

1. 启动服务器：`tftp_server.exe`
2. 将测试文件放入 `tftp_root/` 目录
3. 使用Windows自带的TFTP客户端或第三方工具进行测试
4. 查看 `logs/tftp_server.log` 了解详细运行情况

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
2. **详细注释**: 所有函数都有完整的中文注释
3. **编译说明**: 提供多种编译方式
4. **源代码**: 结构清晰，模块化设计
5. **测试说明**: 详细的使用和测试方法

## 作者

计算机网络实验项目 - TFTP服务器实现

# 1) 编译
.\compile.bat

# 2) 新窗口启动服务器（便于观察日志）
Start-Process powershell -ArgumentList '-NoExit','-Command','cd "D:\计算机网络\计算机网络实验\project"; .\tftp_server.exe'

# 3) 下载测试（GET）
tftp -i 127.0.0.1 get test.txt downloaded_test.txt
Get-Content -TotalCount 5 .\downloaded_test.txt

# 4) 上传测试（PUT）
Set-Content -Path my_upload.txt -Value "This is my upload file at $(Get-Date)."
tftp -i 127.0.0.1 put my_upload.txt uploaded.txt
Get-Item .\tftp_root\uploaded.txt | Select-Object FullName,Length

# 5) 查看日志尾部（吞吐量、ACK、完成记录）
Get-Content -Tail 20 .\logs\tftp_server.log

# 6) 停止服务器（如果要在当前窗口停，转到服务器窗口 Ctrl+C；或用下面的命令查找并结束）
Get-Process | Where-Object { $_.Path -like "*tftp_server.exe" } | Select-Object Id,Path
# 假设上条返回的 Id 为 30268，执行：
Stop-Process -Id 30268