#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <thread>
#include <memory>
#include <atomic>
#include "VideoFrame.h"  // VideoFrame 用于表示存储的视频帧

class CaptureThread {
    public:
    //构造函数
    CaptureThread(int width, int height);
    //析构函数
    ~CaptureThread();

    //启动采集线程
    void start();

    //停止采集线程
    void stop();

    //获取最新的视频帧
    VideoFrame& getAvailableBuffer();
    
    //内联函数
    inline bool isRunning() const {
        return running.load();
    }
    private:
    void captureLoop();
    uint8_t* acquireRawData();

    std::atomic<bool> running;
    std::atomic<bool> isBuffer1InUse;

    //采样乒乓缓冲区
    VideoFrame buffer1;
    VideoFrame buffer2;

    std::unique_ptr<std::thread> captureThread;

}

#endif // CAPTURE_THREAD_H