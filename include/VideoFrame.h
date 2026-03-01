#ifndef VIDEO_FRAME_H
#define VIDEO_FRAME_H

#include <cstdint>
#include <linux/videodev2.h>

/**
 * @brief 视频帧数据结构
 * 
 * 用于表示视频帧的元数据和数据指针
 */
class VideoFrame {
public:
    uint8_t* data;        // 帧数据指针（mmap 映射的地址或 MPP buffer 地址）
    size_t   size;        // 数据大小
    int width;            // 宽度
    int height;           // 高度
    uint64_t timestamp;   // 时间戳（微秒）
    uint32_t pixelFormat; // 像素格式（V4L2 格式或 MPP 格式）

    VideoFrame()
        : data(nullptr), size(0), width(0), height(0), timestamp(0), pixelFormat(0) {}

    VideoFrame(int w, int h, uint32_t fmt)
        : data(nullptr), size(0), width(w), height(h), timestamp(0), pixelFormat(fmt) {}

    inline void setTimestamp(uint64_t ts) { timestamp = ts; }
};

#endif // VIDEO_FRAME_H

