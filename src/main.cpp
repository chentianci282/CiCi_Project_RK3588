#include "camera_device.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <csignal>
#include <vector>
#include <ctime>
#include <sys/time.h>
#include <linux/videodev2.h>

static volatile bool g_running = true;

void signalHandler(int sig) {
    (void)sig;
    g_running = false;
}

// 获取当前时间戳（毫秒）
int64_t getCurrentTimeMs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

// 格式化像素格式名称
const char* formatName(uint32_t format) {
    static char name[5];
    name[0] = (format >> 0) & 0xFF;
    name[1] = (format >> 8) & 0xFF;
    name[2] = (format >> 16) & 0xFF;
    name[3] = (format >> 24) & 0xFF;
    name[4] = '\0';
    return name;
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

    printf("========================================\n");
    printf("  摄像头采集测试程序 (C++)\n");
    printf("========================================\n");
    printf("设备路径: %s\n", device_path.c_str());
    printf("请求分辨率: %ux%u\n", width, height);
    printf("请求格式: NV12 (0x%08x)\n", pixel_format);
    printf("按 Ctrl+C 退出\n");
    printf("========================================\n");
    printf("\n提示: 如果设备打开失败，请尝试:\n");
    printf("  - 检查设备是否存在: ls -l %s\n", device_path.c_str());
    printf("  - 查看所有视频设备: v4l2-ctl --list-devices\n");
    printf("  - 尝试其他设备节点: /dev/video63, /dev/video64 等\n");
    printf("========================================\n\n");

    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 创建摄像头设备对象
    CameraDevice camera;

    // 初始化摄像头
    printf("[1/4] 初始化摄像头设备...\n");
    if (camera.init(device_path, width, height, pixel_format) < 0) {
        fprintf(stderr, "❌ 初始化失败\n");
        return -1;
    }
    printf("✅ 初始化成功\n\n");

    // 显示摄像头信息
    printf("[2/4] 摄像头信息:\n");
    printf("  设备路径: %s\n", camera.getDevicePath().c_str());
    printf("  实际分辨率: %ux%u\n", camera.getWidth(), camera.getHeight());
    printf("  实际格式: %s (0x%08x)\n", formatName(camera.getPixelFormat()), camera.getPixelFormat());
    printf("  多平面格式: %s\n", camera.isMultiplanar() ? "是" : "否");
    
    // 枚举支持的格式
    std::vector<uint32_t> formats;
    if (camera.enumFormats(formats) == 0 && !formats.empty()) {
        printf("  支持的格式数量: %zu\n", formats.size());
        printf("  格式列表: ");
        for (size_t i = 0; i < formats.size() && i < 5; ++i) {
            printf("%s ", formatName(formats[i]));
        }
        if (formats.size() > 5) {
            printf("...");
        }
        printf("\n");
    }
    printf("\n");

    // 启动采集
    printf("[3/4] 启动图像采集...\n");
    if (camera.start() < 0) {
        fprintf(stderr, "❌ 启动失败\n");
        camera.deinit();
        return -1;
    }
    printf("✅ 采集已启动\n\n");

    // 采集循环
    printf("[4/4] 开始采集数据...\n");
    printf("----------------------------------------\n");
    
    int frame_count = 0;
    int error_count = 0;
    int64_t start_time = getCurrentTimeMs();
    int64_t last_report_time = start_time;
    int64_t last_frame_time = start_time;
    
    // 统计信息
    uint32_t total_bytes = 0;
    uint32_t min_frame_size = UINT32_MAX;
    uint32_t max_frame_size = 0;
    
    printf("时间(秒) | 帧数 | 帧率(fps) | 帧大小(字节) | 总数据量(MB) | 状态\n");
    printf("----------------------------------------\n");

    while (g_running) {
        void* frame_data = nullptr;
        uint32_t frame_size = 0;
        uint32_t planes = 0;

        // 捕获一帧
        int ret = camera.captureFrame(&frame_data, &frame_size, &planes);
        
        if (ret == 0) {
            // 采集成功
            frame_count++;
            total_bytes += frame_size;
            
            // 更新统计信息
            if (frame_size < min_frame_size) min_frame_size = frame_size;
            if (frame_size > max_frame_size) max_frame_size = frame_size;
            
            int64_t current_time = getCurrentTimeMs();
            int64_t elapsed = current_time - start_time;
            int64_t report_elapsed = current_time - last_report_time;
            
            // 每1秒报告一次
            if (report_elapsed >= 1000) {
                double fps = (frame_count * 1000.0) / elapsed;
                double mb_total = total_bytes / (1024.0 * 1024.0);
                double elapsed_sec = elapsed / 1000.0;
                
                printf("%8.1f | %5d | %9.2f | %13u | %12.2f | ✅\n",
                       elapsed_sec, frame_count, fps, frame_size, mb_total);
                
                last_report_time = current_time;
            }
            
            // 检查帧间隔（用于检测是否真的在采集）
            int64_t frame_interval = current_time - last_frame_time;
            if (frame_interval > 200) {  // 如果超过200ms没有帧，可能有问题
                printf("⚠️  警告: 帧间隔 %ld ms (可能采集异常)\n", frame_interval);
            }
            last_frame_time = current_time;
            
        } else if (ret == -2) {
            // 暂时没有数据，稍等
            usleep(1000);  // 1ms
        } else {
            // 采集错误
            error_count++;
            if (error_count % 10 == 0) {
                fprintf(stderr, "❌ 采集错误 (错误次数: %d)\n", error_count);
            }
            if (error_count > 100) {
                fprintf(stderr, "❌ 错误过多，停止采集\n");
                break;
            }
            usleep(10000);  // 10ms
        }
    }

    printf("----------------------------------------\n");
    printf("\n正在停止...\n");

    // 停止采集
    camera.stop();
    camera.deinit();

    // 打印最终统计
    int64_t total_time = getCurrentTimeMs() - start_time;
    double total_sec = total_time / 1000.0;
    double avg_fps = frame_count / total_sec;
    double mb_total = total_bytes / (1024.0 * 1024.0);
    double mbps = (mb_total * 8) / total_sec;  // 数据传输速率 (Mbps)

    printf("\n========================================\n");
    printf("  采集统计信息\n");
    printf("========================================\n");
    printf("总采集时间: %.2f 秒\n", total_sec);
    printf("总帧数: %d 帧\n", frame_count);
    printf("平均帧率: %.2f fps\n", avg_fps);
    printf("总数据量: %.2f MB\n", mb_total);
    printf("数据传输速率: %.2f Mbps\n", mbps);
    printf("帧大小范围: %u ~ %u 字节\n", min_frame_size, max_frame_size);
    printf("错误次数: %d\n", error_count);
    
    if (frame_count > 0) {
        printf("\n✅ 测试通过: 成功采集 %d 帧数据\n", frame_count);
    } else {
        printf("\n❌ 测试失败: 未采集到任何数据\n");
    }
    printf("========================================\n");

    return (frame_count > 0) ? 0 : -1;
}

