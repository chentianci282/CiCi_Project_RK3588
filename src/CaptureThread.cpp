#include "CaptureThread.h"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/select.h>

CaptureThread::CaptureThread(int width, int height, std::string deviceName)
    : fd(-1),
      deviceName(std::move(deviceName)),
      width(width),
      height(height),
      running(false) {}

CaptureThread::~CaptureThread() {
    stop();
}

void CaptureThread::start() {
    if (running.load()) {
        std::cerr << "CaptureThread is already running" << std::endl;
        return;
    }
    if (!initializeV4L2()) {
        std::cerr << "Failed to initialize V4L2" << std::endl;
        return;
    }
    running.store(true);
    captureThread = std::make_unique<std::thread>(&CaptureThread::captureLoop, this);
}

void CaptureThread::stop() {
    if (!running.load()) {
        std::cerr << "CaptureThread is not running" << std::endl;
        return;
    }
    running.store(false);
    if (captureThread && captureThread->joinable()) {
        captureThread->join();
    }
    cleanupV4L2();
}

bool CaptureThread::initializeV4L2() {
    // 打开设备节点
    fd = open(deviceName.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Failed to open device: " << deviceName << std::endl;
        return false;
    }

    // 获取设备的能力
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        std::cerr << "Failed to query device capabilities" << std::endl;
        return false;
    }
    std::cout << "Device capabilities: " << cap.card << std::endl;
    std::cout << "Device capabilities: " << cap.driver << std::endl;
    std::cout << "Device capabilities: " << cap.bus_info << std::endl;
    std::cout << "Device capabilities: " << cap.version << std::endl;
    std::cout << "Device capabilities: " << cap.capabilities << std::endl;

    // 为本次采集选择一种图像格式（使用原生 V4L2 fourcc）
    uint32_t pixelFormat = V4L2_PIX_FMT_YUV420;

    // 设置格式
    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelFormat;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        std::cerr << "Failed to set format" << std::endl;
        return false;
    }
    std::cout << "Format set to: " << fmt.fmt.pix.pixelformat << std::endl;

    // 申请缓冲区
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = 3;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        std::cerr << "Failed to request buffers" << std::endl;
        return false;
    }
    std::cout << "Buffers requested: " << req.count << std::endl;

    // 映射缓冲区
    buffers.resize(req.count);
    videoFrames.resize(req.count);

    for (uint32_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            std::cerr << "Failed to query buffer" << std::endl;
            return false;
        }

        void* addr = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (addr == MAP_FAILED) {
            std::cerr << "Failed to mmap buffer" << std::endl;
            return false;
        }

        buffers[i] = addr;

        std::cout << "Buffer " << i << " mapped at: " << buffers[i] << std::endl;

        //VideoFrame 只保存指针和元信息，不自己分配内存
        videoFrames[i].width       = width;
        videoFrames[i].height      = height;
        videoFrames[i].pixelFormat = pixelFormat;
        videoFrames[i].data        = static_cast<uint8_t*>(addr);
        videoFrames[i].size        = buf.length;
    }
    return true;
}

void CaptureThread::cleanupV4L2() {
    for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i] != nullptr) {
            munmap(buffers[i], videoFrames[i].size);
            buffers[i] = nullptr;
        }
    }
    buffers.clear();
    videoFrames.clear();
    close(fd);
    fd = -1;
}


bool CaptureThread::getFrame() {
    if (fd < 0) {
        return false;
    }

    // 使用 select 等待新帧到达，避免忙轮询
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    struct timeval tv;
    tv.tv_sec  = 2;   // 最长等待 2s
    tv.tv_usec = 0;

    int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (ret == -1) {
        std::perror("select");
        return false;
    } else if (ret == 0) {
        std::cerr << "select timeout while waiting for frame" << std::endl;
        return false;  // 超时直接返回，循环里可选择继续等
    }

    struct v4l2_buffer buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // 取出一帧
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        std::perror("VIDIOC_DQBUF");
        return false;
    }

    if (buf.index >= videoFrames.size()) {
        std::cerr << "Invalid buffer index from driver: " << buf.index << std::endl;
        return false;
    }

    // 记录时间戳（转换为 us）
    uint64_t ts =
        static_cast<uint64_t>(buf.timestamp.tv_sec) * 1000000ULL +
        static_cast<uint64_t>(buf.timestamp.tv_usec);
    videoFrames[buf.index].setTimestamp(ts);

    // 更新最新帧索引
    latestFrameIndex.store(buf.index, std::memory_order_release);

    // 处理完后再把缓冲区重新放回队列
    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        std::perror("VIDIOC_QBUF");
        return false;
    }

    return true;
}

void CaptureThread::captureLoop() {
    // 1. 先把所有缓冲区排队
    for (uint32_t i = 0; i < buffers.size(); ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            std::perror("VIDIOC_QBUF (initial queue)");
            return;
        }
    }

    // 2. 启动视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        std::perror("VIDIOC_STREAMON");
        return;
    }

    // 3. 采集循环
    while (running.load()) {
        if (!getFrame()) {
            // 出错或超时时稍微休息一下，避免打印过快
            usleep(10 * 1000);  // 10ms
        }
    }

    // 4. 停止视频流
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        std::perror("VIDIOC_STREAMOFF");
    }
}

VideoFrame& CaptureThread::getAvailableBuffer() {
    // 简单实现：返回最近一次更新的那一帧
    size_t index = latestFrameIndex.load(std::memory_order_acquire);
    return videoFrames[index];
}

