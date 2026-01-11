#include "camera_device.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

CameraDevice::CameraDevice()
    : m_fd(-1)
    , m_width(0)
    , m_height(0)
    , m_pixel_format(V4L2_PIX_FMT_NV12)
    , m_buffer_count(4)
    , m_is_streaming(false)
{
}

CameraDevice::~CameraDevice() {
    deinit();
}

int CameraDevice::xioctl(int request, void* arg) {
    int r;
    do {
        r = ioctl(m_fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

int CameraDevice::init(const std::string& device_path, 
                       uint32_t width, 
                       uint32_t height, 
                       uint32_t pixel_format) {
    if (m_fd >= 0) {
        fprintf(stderr, "Camera already initialized\n");
        return -1;
    }

    m_device_path = device_path;
    m_width = width;
    m_height = height;
    m_pixel_format = pixel_format;

    // 打开设备
    m_fd = open(device_path.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (m_fd < 0) {
        fprintf(stderr, "Cannot open device %s: %s\n", 
                device_path.c_str(), strerror(errno));
        return -1;
    }

    // 查询设备能力
    struct v4l2_capability cap;
    CLEAR(cap);
    if (xioctl(VIDIOC_QUERYCAP, &cap) < 0) {
        fprintf(stderr, "VIDIOC_QUERYCAP failed: %s\n", strerror(errno));
        close(m_fd);
        m_fd = -1;
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device %s is not a video capture device\n", 
                device_path.c_str());
        close(m_fd);
        m_fd = -1;
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device %s does not support streaming\n", 
                device_path.c_str());
        close(m_fd);
        m_fd = -1;
        return -1;
    }

    // 设置格式
    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (xioctl(VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr, "VIDIOC_S_FMT failed: %s\n", strerror(errno));
        close(m_fd);
        m_fd = -1;
        return -1;
    }

    // 更新实际设置的格式
    m_width = fmt.fmt.pix.width;
    m_height = fmt.fmt.pix.height;
    m_pixel_format = fmt.fmt.pix.pixelformat;

    printf("Camera initialized: %s, %ux%u, format=0x%08x\n", 
           device_path.c_str(), m_width, m_height, m_pixel_format);

    return 0;
}

void CameraDevice::deinit() {
    stop();
    cleanupMmap();

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

int CameraDevice::initMmap() {
    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = m_buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr, "VIDIOC_REQBUFS failed: %s\n", strerror(errno));
        return -1;
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory\n");
        return -1;
    }

    m_buffer_count = req.count;
    m_buffers.resize(req.count);
    m_buffer_lengths.resize(req.count);

    // 映射每个缓冲区
    for (uint32_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr, "VIDIOC_QUERYBUF failed: %s\n", strerror(errno));
            cleanupMmap();
            return -1;
        }

        m_buffer_lengths[i] = buf.length;
        m_buffers[i] = mmap(NULL, buf.length,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            m_fd, buf.m.offset);

        if (MAP_FAILED == m_buffers[i]) {
            fprintf(stderr, "mmap failed: %s\n", strerror(errno));
            cleanupMmap();
            return -1;
        }
    }

    return 0;
}

void CameraDevice::cleanupMmap() {
    for (size_t i = 0; i < m_buffers.size(); ++i) {
        if (m_buffers[i] && m_buffers[i] != MAP_FAILED) {
            munmap(m_buffers[i], m_buffer_lengths[i]);
        }
    }
    m_buffers.clear();
    m_buffer_lengths.clear();
    m_buffer_count = 0;
}

int CameraDevice::start() {
    if (m_fd < 0) {
        fprintf(stderr, "Camera not initialized\n");
        return -1;
    }

    if (m_is_streaming) {
        return 0;  // 已经在采集
    }

    // 申请缓冲区
    if (initMmap() < 0) {
        return -1;
    }

    // 将所有缓冲区入队
    for (uint32_t i = 0; i < m_buffer_count; ++i) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "VIDIOC_QBUF failed: %s\n", strerror(errno));
            cleanupMmap();
            return -1;
        }
    }

    // 启动流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(VIDIOC_STREAMON, &type) < 0) {
        fprintf(stderr, "VIDIOC_STREAMON failed: %s\n", strerror(errno));
        cleanupMmap();
        return -1;
    }

    m_is_streaming = true;
    printf("Camera streaming started\n");
    return 0;
}

void CameraDevice::stop() {
    if (!m_is_streaming) {
        return;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(VIDIOC_STREAMOFF, &type);

    m_is_streaming = false;
    printf("Camera streaming stopped\n");
}

int CameraDevice::captureFrame(void** frame_data, uint32_t* frame_size) {
    if (!m_is_streaming || !frame_data || !frame_size) {
        return -1;
    }

    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // 从队列中取出一个缓冲区
    if (xioctl(VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) {
            return -2;  // 暂时没有数据
        }
        fprintf(stderr, "VIDIOC_DQBUF failed: %s\n", strerror(errno));
        return -1;
    }

    *frame_data = m_buffers[buf.index];
    *frame_size = buf.bytesused;

    // 将缓冲区重新入队
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr, "VIDIOC_QBUF failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

void CameraDevice::releaseFrame(void* frame_data) {
    // MMAP模式下，帧会在下次QBUF时自动释放
    (void)frame_data;
}

