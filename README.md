# 摄像头采集程序 (C++)

基于V4L2的摄像头采集实现，按照分层架构设计，使用C++面向对象编程。

## 项目结构

```
camera_display/
├── include/
│   └── camera_device.h      # 摄像头设备类（C++）
├── src/
│   ├── camera_device.cpp   # V4L2摄像头实现
│   └── main.cpp            # 测试主程序
├── build/                   # 编译输出目录
├── Makefile                 # 编译脚本
└── README.md
```

## 编译

```bash
cd /home/alientek/rk3588_linux_sdk/app/camera_display
make
```

## 运行

```bash
# 使用默认设备 /dev/video62
./build/camera_test

# 指定设备
./build/camera_test /dev/video62

# 指定设备和分辨率
./build/camera_test /dev/video62 1920 1080
```

## 当前实现

- ✅ 摄像头设备初始化
- ✅ V4L2 MMAP缓冲区管理
- ✅ 图像采集循环
- ⏳ 显示功能（下一步）
- ⏳ 硬件加速（RGA）（后续）

## 下一步

1. 添加显示功能（DRM/KMS）
2. 添加格式转换（RGA）
3. 整合到完整框架

