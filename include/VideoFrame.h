#ifndef VIDEO_FRAME_H
#define VIDEO_FRAME_H

#include <cstdint>
#include <linux/videodev2.h>

class VideoFrame {
public:
    // 直接指向外部（例如 V4L2 mmap）缓冲区的指针
    uint8_t* data;
    size_t   size;

    // 视频帧的宽度和高度
    int width;
    int height;

    // 帧的时间戳
    uint64_t timestamp;

    // 原生 V4L2 像素格式（例如 V4L2_PIX_FMT_YUV420 / V4L2_PIX_FMT_NV12 等）
    uint32_t pixelFormat;

    VideoFrame()
        : data(nullptr),
          size(0),
          width(0),
          height(0),
          timestamp(0),
          pixelFormat(0) {}

    VideoFrame(int w, int h, uint32_t fmt)
        : data(nullptr),
          size(0),
          width(w),
          height(h),
          timestamp(0),
          pixelFormat(fmt) {}

    inline void setTimestamp(uint64_t ts) { timestamp = ts; }
};

#endif // VIDEO_FRAME_H