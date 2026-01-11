# 交叉编译工具链路径（RK3588 SDK）
SDK_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
TOOLCHAIN_PATH := $(SDK_ROOT)/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
CROSS_COMPILE_PREFIX := aarch64-none-linux-gnu-

# 交叉编译工具
CXX = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE_PREFIX)g++
CC = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE_PREFIX)gcc
STRIP = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE_PREFIX)strip

# 编译选项
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++11
LDFLAGS = 

# 目录
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# 源文件
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 目标
TARGET = $(BUILD_DIR)/camera_test

# 包含路径
INCLUDES = -I$(INC_DIR)

.PHONY: all clean

# 检查工具链是否存在（在编译前自动检查）
all: $(TARGET)

# 在编译前检查工具链
$(TARGET): | check-toolchain

check-toolchain:
	@if [ ! -f "$(CXX)" ]; then \
		echo "Error: Cross-compiler not found: $(CXX)"; \
		echo "Please check SDK_ROOT: $(SDK_ROOT)"; \
		exit 1; \
	fi
	@echo "Using cross-compiler: $(CXX)"
	@$(CXX) --version | head -1

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	$(STRIP) $@  # 去除调试符号，减小文件大小
	@echo "Build complete: $@"
	@file $@  # 显示文件信息，确认是ARM64架构

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

install: $(TARGET)
	@echo "Install to /usr/local/bin (optional)"
	# cp $(TARGET) /usr/local/bin/

