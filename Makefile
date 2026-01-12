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

# DRM库路径（使用camera_engine_rkaiq中的库，或开发板系统库）
# 开发板上应该有系统自带的libdrm，运行时会自动找到
DRM_LIB_PATH = $(SDK_ROOT)/external/camera_engine_rkaiq/rkisp_demo/demo/libs/arm64
LDFLAGS = -L$(DRM_LIB_PATH) -ldrm -Wl,-rpath,$(DRM_LIB_PATH) 

# 目录
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# 源文件
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 目标
TARGET = $(BUILD_DIR)/camera_test
TARGET_DISPLAY = $(BUILD_DIR)/camera_display_test

# 包含路径
# 使用camera_engine_rkaiq中的DRM头文件（更完整）
DRM_INCLUDE_PATH = $(SDK_ROOT)/external/camera_engine_rkaiq/rkisp_demo/demo/include
DRM_CORE_INCLUDE_PATH = $(SDK_ROOT)/external/linux-rga/core/3rdparty/libdrm/include
# 创建符号链接让xf86drm.h能找到drm.h
$(BUILD_DIR)/drm:
	@mkdir -p $(BUILD_DIR)
	@ln -sf ../../../external/linux-rga/core/3rdparty/libdrm/include/drm $(BUILD_DIR)/drm
	@ln -sf drm/drm.h $(BUILD_DIR)/drm.h
	@ln -sf drm/drm_mode.h $(BUILD_DIR)/drm_mode.h
INCLUDES = -I$(INC_DIR) -I$(BUILD_DIR) -I$(DRM_CORE_INCLUDE_PATH) -I$(DRM_INCLUDE_PATH)

.PHONY: all clean

# 检查工具链是否存在（在编译前自动检查）
all: $(TARGET) $(TARGET_DISPLAY)

# 在编译前检查工具链和创建符号链接
$(TARGET): | check-toolchain $(BUILD_DIR)/drm
$(TARGET_DISPLAY): | check-toolchain $(BUILD_DIR)/drm

check-toolchain:
	@if [ ! -f "$(CXX)" ]; then \
		echo "Error: Cross-compiler not found: $(CXX)"; \
		echo "Please check SDK_ROOT: $(SDK_ROOT)"; \
		exit 1; \
	fi
	@echo "Using cross-compiler: $(CXX)"
	@$(CXX) --version | head -1

$(TARGET): $(BUILD_DIR)/camera_device.o $(BUILD_DIR)/main.o
	$(CXX) $^ -o $@ $(LDFLAGS)
	$(STRIP) $@
	@echo "Build complete: $@"
	@file $@

$(TARGET_DISPLAY): $(BUILD_DIR)/camera_device.o $(BUILD_DIR)/display_device.o $(BUILD_DIR)/test_camera_display.o
	$(CXX) $^ -o $@ $(LDFLAGS)
	$(STRIP) $@
	@echo "Build complete: $@"
	@file $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

install: $(TARGET)
	@echo "Install to /usr/local/bin (optional)"
	# cp $(TARGET) /usr/local/bin/

