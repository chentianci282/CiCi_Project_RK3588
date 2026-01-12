#include "camera_device.h"
#include "display_device.h"
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
    std::string camera_device = "/dev/video62";
    uint32_t width = 1280;   // 720p宽度
    uint32_t height = 720;   // 720p高度

    // 解析命令行参数
    if (argc > 1) {
        camera_device = argv[1];
    }
    if (argc > 3) {
        width = static_cast<uint32_t>(atoi(argv[2]));
        height = static_cast<uint32_t>(atoi(argv[3]));
    }

    printf("========================================\n");
    printf("  摄像头采集+显示测试程序\n");
    printf("========================================\n");
    printf("摄像头设备: %s\n", camera_device.c_str());
    printf("分辨率: %ux%u\n", width, height);
    printf("按 Ctrl+C 退出\n");
    printf("========================================\n\n");

    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 初始化摄像头
    CameraDevice camera;
    printf("[1/3] 初始化摄像头...\n");
    if (camera.init(camera_device, width, height, V4L2_PIX_FMT_NV12) < 0) {
        fprintf(stderr, "❌ 摄像头初始化失败\n");
        return -1;
    }
    printf("✅ 摄像头初始化成功\n\n");

    // 启动采集
    printf("[2/3] 启动摄像头采集...\n");
    if (camera.start() < 0) {
        fprintf(stderr, "❌ 摄像头启动失败\n");
        camera.deinit();
        return -1;
    }
    printf("✅ 摄像头采集已启动\n\n");

    // 初始化显示
    DisplayDevice display;
    printf("[3/3] 初始化显示设备...\n");
    if (display.init() < 0) {
        fprintf(stderr, "❌ 显示设备初始化失败\n");
        camera.stop();
        camera.deinit();
        return -1;
    }
    
    uint32_t display_width, display_height;
    display.getDisplaySize(&display_width, &display_height);
    printf("✅ 显示设备初始化成功\n");
    printf("   显示分辨率: %ux%u\n\n", display_width, display_height);

    printf("========================================\n");
    printf("  开始采集并显示...\n");
    printf("========================================\n");

    // 采集并显示循环
    int frame_count = 0;
    int display_count = 0;
    
    while (g_running) {
        void* frame_data = nullptr;
        uint32_t frame_size = 0;
        uint32_t planes = 0;

        // 捕获一帧
        int ret = camera.captureFrame(&frame_data, &frame_size, &planes);
        if (ret == 0) {
            frame_count++;
            
            // 显示到屏幕（每帧都显示，或可以控制显示频率）
            // 如果CRTC还没设置成功，降低显示频率（每10帧尝试一次）
            static int display_retry_counter = 0;
            bool should_try_display = (display_count > 0) || (display_retry_counter % 10 == 0);
            
            if (should_try_display) {
                int display_ret = display.displayFrameYUV(frame_data, camera.getWidth(), camera.getHeight());
                if (display_ret == 0) {
                    display_count++;
                    display_retry_counter = 0;  // 成功后重置计数器
                } else {
                    display_retry_counter++;
                    if (display_count == 0 && frame_count % 30 == 0) {
                        fprintf(stderr, "Display still failing, retry counter: %d\n", display_retry_counter);
                    }
                }
            } else {
                display_retry_counter++;
            }
            
            // 每30帧打印一次统计
            if (frame_count % 30 == 0) {
                printf("采集: %d 帧 | 显示: %d 帧 | 帧大小: %u 字节\n",
                       frame_count, display_count, frame_size);
            }
        } else if (ret == -2) {
            // 暂时没有数据
            usleep(1000);  // 1ms
        } else {
            fprintf(stderr, "❌ 采集错误\n");
            break;
        }
    }

    printf("\n正在停止...\n");

    // 清理
    display.deinit();
    camera.stop();
    camera.deinit();

    printf("\n========================================\n");
    printf("  统计信息\n");
    printf("========================================\n");
    printf("总采集帧数: %d\n", frame_count);
    printf("总显示帧数: %d\n", display_count);
    printf("========================================\n");

    return 0;
}

