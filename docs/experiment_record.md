## 计算机网络实验记录

### 1. 获取本机IP地址

在终端中使用 `ipconfig` 命令，得到本机IP地址：

```
10.17.180.24
```

---

### 2. 工具学习与使用

#### 2.1 tftpd64.exe

- 这是一个用于测试网络连接的图形化TFTP服务器/客户端工具。
- 可以测试TCP和UDP连接，并提供详细报告。
- 优点：界面友好，操作直观。
- 我将TFTP根目录设置为：

```
D:\计算机网络\计算机网络实验\project\tfpt_root
```

#### 2.2 Windows自带tftp命令

- 学会了在命令行中使用`tftp`指令进行文件上传和下载测试。
  
**使用方法：**
```powershell
向运行 TFTP 服务的远程计算机传入或从该计算机传出文件。

TFTP [-i] host [GET | PUT] source [destination]

  -i              指定二进制映像传输模式(也称为
                  八进制)。在二进制映像模式中，逐字节地
                  移动文件。在传输二进制文件时，
                  使用此模式。
  host            指定本地或远程主机。
  GET             将远程主机上的文件目标传输到
                  本地主机的文件源中。
  PUT             将本地主机上的文件源传输到
                  远程主机上的文件目标。
  source          指定要传输的文件。
  destination     指定要将文件传输到的位置。
```

**下载文件示例：**

```powershell
tftp -i 10.17.180.24 get test.txt test.txt
```
这条命令会从 `10.17.180.24` 这个IP地址的TFTP服务器下载 `test.txt` 文件到当前目录。

> 注意：`get` 后面的第一个参数是服务器上的文件名，第二个参数是下载到本地的文件名（可带路径）。如果没有指定路径，默认下载到当前目录。

**上传文件示例：**

```powershell
tftp -i 10.17.180.24 put D:\计算机网络\计算机网络实验\计网实验相关资料\链路丢包模拟软件\config.txt config.txt
```
这条命令会将本地 `config.txt` 文件上传到 `10.17.180.24` 服务器的TFTP根目录，文件名为 `config.txt`。

---

### 3. 日志记录

```
Connection received from 10.17.180.24 on port 52327 [30/09 20:26:01.801]
Read request for file <test.txt>. Mode octet [30/09 20:26:01.802]
Using local port 52328 [30/09 20:26:01.802]
<test.txt>: sent 1 blk, 6 bytes in 0 s. 0 blk resent [30/09 20:26:01.803]
Connection received from 10.17.180.24 on port 52856 [30/09 20:28:30.298]
Write request for file <config.txt>. Mode octet [30/09 20:28:30.299]
Using local port 52857 [30/09 20:28:30.299]
<config.txt>: rcvd 3 blks, 1408 bytes in 0 s. 0 blk resent [30/09 20:28:30.303]
```
