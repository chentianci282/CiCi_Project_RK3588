#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include <string>
#include <functional>
#include <mutex>

#include "VideoFrame.h"  // VideoFrame 用于表示存储的视频帧

class CaptureThread {
public:
    // 帧回调函数类型
    using FrameCallback = std::function<void(const VideoFrame&)>;

    // 构造函数
    CaptureThread(int width, int height, std::string deviceName);
    // 析构函数
    ~CaptureThread();

    // 启动采集线程
    void start();

    // 停止采集线程
    void stop();

    // 获取最新的视频帧
    VideoFrame& getAvailableBuffer();

    // 获取最新帧索引
    size_t getLatestFrameIndex() const {
        return latestFrameIndex.load();
    }

    // 设置帧回调（支持多消费者）
    void setFrameCallback(FrameCallback callback);

    // 内联函数
    inline bool isRunning() const {
        return running.load();
    }

private:
    void captureLoop();
    bool initializeV4L2();  // 初始化 V4L2 设备
    void cleanupV4L2();     // 清理 V4L2 设备
    bool getFrame();        // 从 V4L2 获取视频帧

    // 设备节点
    int fd;
    std::string deviceName;
    std::vector<VideoFrame> videoFrames;  // 封装好的 VideoFrame 对象（包含 mmap 映射的缓冲区地址）
    int width;
    int height;

    // 采集线程状态
    std::atomic<bool> running;

    // 当前最新可用帧的下标（与 videoFrames 中的索引对应）
    std::atomic<size_t> latestFrameIndex{0};

    // 帧回调函数
    FrameCallback frameCallback;
    std::mutex callbackMutex;

    std::unique_ptr<std::thread> captureThread;
};

#endif // CAPTURE_THREAD_H