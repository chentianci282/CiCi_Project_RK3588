# 交叉编译工具链路径（RK3588 SDK）
SDK_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
TOOLCHAIN_PATH := $(SDK_ROOT)/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
CROSS_COMPILE_PREFIX := aarch64-none-linux-gnu-

# 交叉编译工具
CXX = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE_PREFIX)g++
CC = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE_PREFIX)gcc
STRIP = $(TOOLCHAIN_PATH)/$(CROSS_COMPILE_PREFIX)strip

# 编译选项
CXXFLAGS = -Wall -Wextra -O2 -g -std=c++11 -fno-rtti

# DRM库路径（使用camera_engine_rkaiq中的库，或开发板系统库）
# 开发板上应该有系统自带的libdrm，运行时会自动找到
DRM_LIB_PATH = $(SDK_ROOT)/external/camera_engine_rkaiq/rkisp_demo/demo/libs/arm64

# Rockit MPI库路径
ROCKIT_LIB_PATH = $(SDK_ROOT)/external/rockit/mpi/sdk/lib/lib64
ROCKIT_INCLUDE_PATH = $(SDK_ROOT)/external/rockit/mpi/sdk/include
ROCKIT_EXAMPLE_INCLUDE_PATH = $(SDK_ROOT)/external/rockit/mpi/example/include
ROCKIT_EXAMPLE_COMMON_PATH = $(SDK_ROOT)/external/rockit/mpi/example/common
ROCKIT_LIB_INCLUDE_PATH = $(SDK_ROOT)/external/rockit/lib/lib64

# MPP库路径（rockchip_mpp）
MPP_LIB_PATH = $(SDK_ROOT)/external/mpp/build/install/usr/local/lib

# 链接库（asound在运行时由系统提供，链接时使用-Wl,--allow-shlib-undefined）
LDFLAGS = -L$(DRM_LIB_PATH) -L$(ROCKIT_LIB_PATH) -L$(MPP_LIB_PATH) \
          -ldrm -lrockit -lrockchip_mpp -lpthread \
          -Wl,-rpath,$(DRM_LIB_PATH):$(ROCKIT_LIB_PATH):$(MPP_LIB_PATH) \
          -Wl,--allow-shlib-undefined 

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
TARGET_VI_VENC = $(BUILD_DIR)/vi_venc_capture

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
INCLUDES = -I$(INC_DIR) -I$(BUILD_DIR) -I$(DRM_CORE_INCLUDE_PATH) -I$(DRM_INCLUDE_PATH) \
           -I$(ROCKIT_INCLUDE_PATH) -I$(ROCKIT_EXAMPLE_INCLUDE_PATH) -I$(ROCKIT_LIB_INCLUDE_PATH)

.PHONY: all clean

# 检查工具链是否存在（在编译前自动检查）
all: $(TARGET) $(TARGET_DISPLAY) $(TARGET_VI_VENC)

# 在编译前检查工具链和创建符号链接
$(TARGET): | check-toolchain $(BUILD_DIR)/drm
$(TARGET_DISPLAY): | check-toolchain $(BUILD_DIR)/drm
$(TARGET_VI_VENC): | check-toolchain $(BUILD_DIR)/drm

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

# VI+VENC采集编码程序（需要链接test_comm_utils）
$(TARGET_VI_VENC): $(BUILD_DIR)/vi_venc_capture.o $(BUILD_DIR)/test_comm_utils.o
	$(CXX) $^ -o $@ $(LDFLAGS)
	$(STRIP) $@
	@echo "Build complete: $@"
	@file $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 编译test_comm_utils.cpp（从rockit example中）
$(BUILD_DIR)/test_comm_utils.o: $(ROCKIT_EXAMPLE_COMMON_PATH)/test_comm_utils.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

install: $(TARGET)
	@echo "Install to /usr/local/bin (optional)"
	# cp $(TARGET) /usr/local/bin/

