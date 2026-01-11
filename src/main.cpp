#include "camera_device.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <csignal>
#include <linux/videodev2.h>

static volatile bool g_running = true;

void signalHandler(int sig) {
    (void)sig;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::string device_path = "/dev/video62";  // MIPI CSI1对应的设备
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t pixel_format = V4L2_PIX_FMT_NV12;

    // 解析命令行参数
    if (argc > 1) {
        device_path = argv[1];
    }
    if (argc > 3) {
        width = static_cast<uint32_t>(atoi(argv[2]));
        height = static_cast<uint32_t>(atoi(argv[3]));
    }

    printf("=== 摄像头采集测试程序 (C++) ===\n");
    printf("设备: %s\n", device_path.c_str());
    printf("分辨率: %ux%u\n", width, height);
    printf("格式: NV12\n");
    printf("按Ctrl+C退出\n\n");

    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 创建摄像头设备对象
    CameraDevice camera;

    // 初始化摄像头
    if (camera.init(device_path, width, height, pixel_format) < 0) {
        fprintf(stderr, "Failed to initialize camera\n");
        return -1;
    }

    // 启动采集
    if (camera.start() < 0) {
        fprintf(stderr, "Failed to start camera\n");
        return -1;
    }

    // 采集循环
    int frame_count = 0;
    while (g_running) {
        void* frame_data = nullptr;
        uint32_t frame_size = 0;

        // 捕获一帧
        int ret = camera.captureFrame(&frame_data, &frame_size);
        if (ret == 0) {
            frame_count++;
            if (frame_count % 30 == 0) {
                printf("Captured %d frames, last frame size: %u bytes\n", 
                       frame_count, frame_size);
            }
        } else if (ret == -2) {
            // 暂时没有数据，稍等
            usleep(10000);  // 10ms
        } else {
            fprintf(stderr, "Failed to capture frame\n");
            break;
        }
    }

    printf("\nStopping...\n");

    // 清理（析构函数会自动处理）
    camera.stop();
    camera.deinit();

    printf("Total frames captured: %d\n", frame_count);
    return 0;
}

