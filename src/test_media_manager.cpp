#include "MediaManager.h"
#include "VideoEncoderSvc.h"
#include "YUVOutputSvc.h"
#include "VideoFrame.h"
#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <mutex>
#include <cstring>

// MPP 系统头文件
#include "rk_mpi_sys.h"

// 测试参数（写死）
static const int WIDTH = 3840;
static const int HEIGHT = 2160;
static const std::string DEVICE = "/dev/video62";
static const int VI_DEV_ID = 0;
static const int VI_PIPE_ID = 0;
// 对齐 test_mpi_vi 的默认通道配置：channelId 默认为 1
static const int VI_CHN_ID = 1;
static const std::string VENC_OUTPUT_FILE = "/data/venc_0.bin";
static const std::string YUV_OUTPUT_FILE = "/data/yuv_0.raw";
static const size_t MAX_FILE_SIZE = 50 * 1024 * 1024;  // 50MB

static volatile bool g_running = true;
static std::ofstream g_venc_file;
static std::ofstream g_yuv_file;
static std::mutex g_venc_file_mutex;
static std::mutex g_yuv_file_mutex;
static size_t g_venc_file_size = 0;
static size_t g_yuv_file_size = 0;
static int g_frame_count = 0;
static int g_yuv_count = 0;

void signalHandler(int sig) {
    (void)sig;
    g_running = false;
    std::cout << "\n[Test] Received signal, stopping..." << std::endl;
}

// 编码数据回调
void onEncodedFrame(const EncodedFrame& frame) {
    std::cout << "[Test] onEncodedFrame called, size=" << frame.size
              << ", isKeyFrame=" << (frame.isKeyFrame ? "Yes" : "No") << std::endl;

    if (frame.size > 0) {
        std::lock_guard<std::mutex> lock(g_venc_file_mutex);
        
        // 检查文件大小，超过50M则清空重新开始
        if (g_venc_file_size + frame.size > MAX_FILE_SIZE) {
            g_venc_file.close();
            g_venc_file.open(VENC_OUTPUT_FILE, std::ios::binary | std::ios::trunc);
            g_venc_file_size = 0;
            std::cout << "[Test] VENC file reset (reached 50MB limit)" << std::endl;
        }
        
        if (g_venc_file.is_open()) {
            g_venc_file.write(reinterpret_cast<const char*>(frame.data.get()), frame.size);
            g_venc_file.flush();
            g_venc_file_size += frame.size;
            g_frame_count++;
            
            if (g_frame_count % 30 == 0) {
                std::cout << "[Test] Encoded frames: " << g_frame_count 
                          << ", KeyFrame: " << (frame.isKeyFrame ? "Yes" : "No")
                          << ", Size: " << frame.size << " bytes"
                          << ", File: " << (g_venc_file_size / 1024 / 1024) << "MB" << std::endl;
            }
        }
    }
}

// YUV 数据回调（保存YUV数据）
void onYUVFrame(const VideoFrame& frame) {
    std::cout << "[Test] onYUVFrame called, data=" << static_cast<const void*>(frame.data)
              << ", size=" << frame.size
              << ", w=" << frame.width
              << ", h=" << frame.height << std::endl;

    if (frame.data != nullptr && frame.size > 0) {
        std::lock_guard<std::mutex> lock(g_yuv_file_mutex);
        
        // 检查文件大小，超过50M则清空重新开始
        if (g_yuv_file_size + frame.size > MAX_FILE_SIZE) {
            g_yuv_file.close();
            g_yuv_file.open(YUV_OUTPUT_FILE, std::ios::binary | std::ios::trunc);
            g_yuv_file_size = 0;
            std::cout << "[Test] YUV file reset (reached 50MB limit)" << std::endl;
        }
        
        if (g_yuv_file.is_open()) {
            // 注意：frame.data 可能是指向MMAP的指针，需要立即写入
            // 因为 ReleaseChnFrame 后数据可能失效
            g_yuv_file.write(reinterpret_cast<const char*>(frame.data), frame.size);
            g_yuv_file.flush();
            g_yuv_file_size += frame.size;
            g_yuv_count++;
            
            if (g_yuv_count % 30 == 0) {
                std::cout << "[Test] YUV frames: " << g_yuv_count 
                          << ", Size: " << frame.width << "x" << frame.height
                          << ", File: " << (g_yuv_file_size / 1024 / 1024) << "MB" << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "========================================" << std::endl;
    std::cout << "  MediaManager Test Program" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Resolution: " << WIDTH << "x" << HEIGHT << std::endl;
    std::cout << "Device: " << DEVICE << std::endl;
    std::cout << "VI Dev/Pipe/Chn: " << VI_DEV_ID << "/" << VI_PIPE_ID << "/" << VI_CHN_ID << std::endl;
    std::cout << "VENC Output: " << VENC_OUTPUT_FILE << " (max 50MB)" << std::endl;
    std::cout << "YUV Output: " << YUV_OUTPUT_FILE << " (max 50MB)" << std::endl;
    std::cout << "VO Output: (temporarily disabled)" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 初始化 MPP 系统（非常关键，所有 RK_MPI_* 调用前必须调用）
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        std::cerr << "[Test] RK_MPI_SYS_Init failed" << std::endl;
        return -1;
    }
    std::cout << "[Test] RK_MPI_SYS_Init ok" << std::endl;

    // 创建 MediaManager
    MediaManager manager;

    // 初始化 MediaManager
    std::cout << "[Test] Initializing MediaManager..." << std::endl;
    if (!manager.init(VI_DEV_ID, VI_PIPE_ID, VI_CHN_ID, DEVICE)) {
        std::cerr << "[Test] Failed to initialize MediaManager" << std::endl;
        return -1;
    }
    std::cout << "[Test] MediaManager initialized successfully" << std::endl;

    // 设置编码回调
    auto encoderSvc = manager.getEncoderService();
    std::cout << "[Test] Got encoderSvc ptr: " << (encoderSvc ? "non-null" : "null") << std::endl;
    if (encoderSvc) {
        std::cout << "[Test] Before setEncodeCallback" << std::endl;
        encoderSvc->setEncodeCallback(onEncodedFrame);
        
        // 设置编码参数
        std::cout << "[Test] Before setEncodeParams" << std::endl;
        EncodeParams params;
        params.width = WIDTH;
        params.height = HEIGHT;
        params.bitrate = 10000000;  // 10Mbps
        params.fps = 30;
        params.gop = 30;
        params.useH265 = false;  // H264
        encoderSvc->setEncodeParams(params);
        
        std::cout << "[Test] Encoder service configured" << std::endl;
    }

    // 设置YUV回调
    auto yuvSvc = manager.getYUVService();
    if (yuvSvc) {
        yuvSvc->setYUVCallback(onYUVFrame);
        std::cout << "[Test] YUV service configured" << std::endl;
    }

    // 打开VENC输出文件
    g_venc_file.open(VENC_OUTPUT_FILE, std::ios::binary | std::ios::trunc);
    if (!g_venc_file.is_open()) {
        std::cerr << "[Test] Failed to open VENC output file: " << VENC_OUTPUT_FILE << std::endl;
        manager.deinit();
        return -1;
    }
    std::cout << "[Test] VENC output file opened: " << VENC_OUTPUT_FILE << std::endl;

    // 打开YUV输出文件
    g_yuv_file.open(YUV_OUTPUT_FILE, std::ios::binary | std::ios::trunc);
    if (!g_yuv_file.is_open()) {
        std::cerr << "[Test] Failed to open YUV output file: " << YUV_OUTPUT_FILE << std::endl;
        g_venc_file.close();
        manager.deinit();
        return -1;
    }
    std::cout << "[Test] YUV output file opened: " << YUV_OUTPUT_FILE << std::endl;

    // 启动所有服务
    std::cout << "[Test] Starting services (VO disabled)..." << std::endl;
    manager.startEncoderService();  // VENC编码
    //manager.startOutputService(); // 暂时屏蔽 VO 显示线程
    manager.startYUVService();      // YUV输出
    std::cout << "[Test] Encoder & YUV services started" << std::endl;
    std::cout << std::endl;

    // 运行循环（一直运行直到收到信号）
    std::cout << "[Test] Running... (Press Ctrl+C to stop)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    while (g_running) {
        sleep(1);  // 每秒检查一次
        
        // 每秒输出一次统计信息
        std::cout << "[Test] Running... "
                  << "VENC: " << g_frame_count << " frames (" << (g_venc_file_size / 1024 / 1024) << "MB), "
                  << "YUV: " << g_yuv_count << " frames (" << (g_yuv_file_size / 1024 / 1024) << "MB), "
                  << "VO: Disabled in this run" << std::endl;
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "[Test] Stopping services..." << std::endl;

    // 停止所有服务
    manager.stopEncoderService();
    //manager.stopOutputService(); // 本轮测试未启动 VO
    manager.stopYUVService();

    // 关闭文件
    {
        std::lock_guard<std::mutex> lock(g_venc_file_mutex);
        if (g_venc_file.is_open()) {
            g_venc_file.close();
            std::cout << "[Test] VENC output file closed" << std::endl;
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(g_yuv_file_mutex);
        if (g_yuv_file.is_open()) {
            g_yuv_file.close();
            std::cout << "[Test] YUV output file closed" << std::endl;
        }
    }

    // 反初始化
    manager.deinit();

    // 退出 MPP 系统
    RK_MPI_SYS_Exit();

    // 打印统计信息
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Test Statistics" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "VENC (Encoding):" << std::endl;
    std::cout << "  - Frames: " << g_frame_count << std::endl;
    std::cout << "  - File: " << VENC_OUTPUT_FILE << " (" << (g_venc_file_size / 1024 / 1024) << "MB)" << std::endl;
    std::cout << "YUV (Algorithm Feed):" << std::endl;
    std::cout << "  - Frames: " << g_yuv_count << std::endl;
    std::cout << "  - File: " << YUV_OUTPUT_FILE << " (" << (g_yuv_file_size / 1024 / 1024) << "MB)" << std::endl;
    std::cout << "VO (Display):" << std::endl;
    std::cout << "  - Status: Check screen output" << std::endl;
    std::cout << "========================================" << std::endl;

    bool success = (g_frame_count > 0 || g_yuv_count > 0);
    if (success) {
        std::cout << std::endl;
        std::cout << "✅ Test passed:" << std::endl;
        if (g_frame_count > 0) {
            std::cout << "   - VENC: " << g_frame_count << " frames encoded" << std::endl;
            std::cout << "   - File: " << VENC_OUTPUT_FILE << std::endl;
        }
        if (g_yuv_count > 0) {
            std::cout << "   - YUV: " << g_yuv_count << " frames saved" << std::endl;
            std::cout << "   - File: " << YUV_OUTPUT_FILE << std::endl;
        }
        std::cout << "   - VO: Check screen for display output" << std::endl;
        return 0;
    } else {
        std::cout << std::endl;
        std::cerr << "❌ Test failed: No data captured" << std::endl;
        return -1;
    }
}

