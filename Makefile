# TFTP服务器 Makefile
# 适用于Windows环境下的MinGW编译器

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS = -lws2_32

# 目录设置
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
TARGET = tftp_server.exe

# 源文件
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# 默认目标
all: $(BUILD_DIR) $(TARGET)

# 创建构建目录
$(BUILD_DIR):
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

# 生成可执行文件
$(TARGET): $(OBJECTS)
	@echo 正在链接 $(TARGET)...
	@$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo 编译完成！

# 编译源文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo 正在编译 $<...
	@$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# 清理构建文件
clean:
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	@if exist $(TARGET) del $(TARGET)
	@echo 清理完成！

# 运行程序
run: $(TARGET)
	@echo 启动TFTP服务器...
	@.\$(TARGET)

# 帮助信息
help:
	@echo TFTP服务器编译选项:
	@echo   all     - 编译整个项目
	@echo   clean   - 清理构建文件
	@echo   run     - 编译并运行程序
	@echo   help    - 显示此帮助信息

.PHONY: all clean run help