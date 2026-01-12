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
    , m_is_multiplanar(false)
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

    // 打印设备信息（用于调试）
    printf("Device info: driver=%s, card=%s, bus_info=%s\n",
           cap.driver, cap.card, cap.bus_info);
    printf("Device capabilities: 0x%08x\n", cap.capabilities);

    // 检查是否支持视频捕获（单平面或多平面）
    bool has_capture = (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) != 0;
    bool has_capture_mplane = (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0;
    
    if (!has_capture && !has_capture_mplane) {
        fprintf(stderr, "❌ Device %s is not a video capture device\n", 
                device_path.c_str());
        fprintf(stderr, "   Device capabilities: 0x%08x\n", cap.capabilities);
        fprintf(stderr, "   Driver: %s\n", cap.driver);
        fprintf(stderr, "   Card: %s\n", cap.card);
        fprintf(stderr, "   Bus: %s\n", cap.bus_info);
        fprintf(stderr, "   V4L2_CAP_VIDEO_CAPTURE: %s\n", 
                has_capture ? "Yes" : "No");
        fprintf(stderr, "   V4L2_CAP_VIDEO_CAPTURE_MPLANE: %s\n", 
                has_capture_mplane ? "Yes" : "No");
        fprintf(stderr, "\n");
        fprintf(stderr, "提示: 请尝试其他设备节点，如 /dev/video63, /dev/video64 等\n");
        fprintf(stderr, "      或使用命令查看所有设备: v4l2-ctl --list-devices\n");
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

    // 检测是否支持多平面格式
    m_is_multiplanar = (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0;

    // 设置格式
    struct v4l2_format fmt;
    CLEAR(fmt);
    
    if (m_is_multiplanar) {
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = width;
        fmt.fmt.pix_mp.height = height;
        fmt.fmt.pix_mp.pixelformat = pixel_format;
        fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    } else {
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
        fmt.fmt.pix.pixelformat = pixel_format;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
    }

    if (xioctl(VIDIOC_S_FMT, &fmt) < 0) {
        fprintf(stderr, "VIDIOC_S_FMT failed: %s\n", strerror(errno));
        close(m_fd);
        m_fd = -1;
        return -1;
    }

    // 更新实际设置的格式
    if (m_is_multiplanar) {
        m_width = fmt.fmt.pix_mp.width;
        m_height = fmt.fmt.pix_mp.height;
        m_pixel_format = fmt.fmt.pix_mp.pixelformat;
    } else {
        m_width = fmt.fmt.pix.width;
        m_height = fmt.fmt.pix.height;
        m_pixel_format = fmt.fmt.pix.pixelformat;
    }

    printf("Camera initialized: %s, %ux%u, format=0x%08x, %s\n", 
           device_path.c_str(), m_width, m_height, m_pixel_format,
           m_is_multiplanar ? "MPLANE" : "single-plane");

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
    req.type = m_is_multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
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

    if (m_is_multiplanar) {
        // 多平面格式
        m_planes.resize(req.count);
        m_plane_lengths.resize(req.count);
        
        // 先查询第一个缓冲区获取平面数量
        struct v4l2_buffer buf_first;
        CLEAR(buf_first);
        buf_first.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf_first.memory = V4L2_MEMORY_MMAP;
        buf_first.index = 0;
        buf_first.length = 1;  // 先分配一个平面来查询
        buf_first.m.planes = new v4l2_plane[1];
        CLEAR(buf_first.m.planes[0]);
        
        if (xioctl(VIDIOC_QUERYBUF, &buf_first) < 0) {
            fprintf(stderr, "VIDIOC_QUERYBUF (first) failed: %s\n", strerror(errno));
            delete[] buf_first.m.planes;
            cleanupMmap();
            return -1;
        }
        
        uint32_t num_planes = buf_first.length;
        delete[] buf_first.m.planes;
        
        printf("Multiplanar format detected: %u planes per buffer\n", num_planes);
        
        // 现在为每个缓冲区查询并映射
        for (uint32_t i = 0; i < req.count; ++i) {
            struct v4l2_buffer buf;
            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            buf.length = num_planes;
            buf.m.planes = new v4l2_plane[num_planes];
            for (uint32_t j = 0; j < num_planes; ++j) {
                CLEAR(buf.m.planes[j]);
            }
            
            if (xioctl(VIDIOC_QUERYBUF, &buf) < 0) {
                fprintf(stderr, "VIDIOC_QUERYBUF failed for buffer %u: %s\n", i, strerror(errno));
                delete[] buf.m.planes;
                cleanupMmap();
                return -1;
            }

            m_planes[i].resize(num_planes);
            m_plane_lengths[i].resize(num_planes);

            // 映射每个平面
            for (uint32_t j = 0; j < num_planes; ++j) {
                m_plane_lengths[i][j] = buf.m.planes[j].length;
                m_planes[i][j] = mmap(NULL, buf.m.planes[j].length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED,
                                      m_fd, buf.m.planes[j].m.mem_offset);

                if (MAP_FAILED == m_planes[i][j]) {
                    fprintf(stderr, "mmap buffer %u plane %u failed: %s\n", i, j, strerror(errno));
                    delete[] buf.m.planes;
                    cleanupMmap();
                    return -1;
                }
            }
            
            delete[] buf.m.planes;
        }
    } else {
        // 单平面格式
        m_buffers.resize(req.count);
        m_buffer_lengths.resize(req.count);

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
    }

    return 0;
}

void CameraDevice::cleanupMmap() {
    if (m_is_multiplanar) {
        // 清理多平面缓冲区
        for (size_t i = 0; i < m_planes.size(); ++i) {
            for (size_t j = 0; j < m_planes[i].size(); ++j) {
                if (m_planes[i][j] && m_planes[i][j] != MAP_FAILED) {
                    munmap(m_planes[i][j], m_plane_lengths[i][j]);
                }
            }
        }
        m_planes.clear();
        m_plane_lengths.clear();
    } else {
        // 清理单平面缓冲区
        for (size_t i = 0; i < m_buffers.size(); ++i) {
            if (m_buffers[i] && m_buffers[i] != MAP_FAILED) {
                munmap(m_buffers[i], m_buffer_lengths[i]);
            }
        }
        m_buffers.clear();
        m_buffer_lengths.clear();
    }
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
        
        if (m_is_multiplanar) {
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            buf.length = m_planes[i].size();
            buf.m.planes = new v4l2_plane[buf.length];
            for (uint32_t j = 0; j < buf.length; ++j) {
                CLEAR(buf.m.planes[j]);
            }
        } else {
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
        }

        if (xioctl(VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "VIDIOC_QBUF failed: %s\n", strerror(errno));
            if (m_is_multiplanar) {
                delete[] buf.m.planes;
            }
            cleanupMmap();
            return -1;
        }
        
        if (m_is_multiplanar) {
            delete[] buf.m.planes;
        }
    }

    // 启动流
    enum v4l2_buf_type type = m_is_multiplanar ? 
        V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
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

    enum v4l2_buf_type type = m_is_multiplanar ? 
        V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(VIDIOC_STREAMOFF, &type);

    m_is_streaming = false;
    printf("Camera streaming stopped\n");
}

int CameraDevice::captureFrame(void** frame_data, uint32_t* frame_size, uint32_t* planes) {
    if (!m_is_streaming || !frame_data || !frame_size) {
        return -1;
    }

    struct v4l2_buffer buf;
    CLEAR(buf);
    
    if (m_is_multiplanar) {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = m_planes[0].size();  // 假设所有缓冲区平面数相同
        buf.m.planes = new v4l2_plane[buf.length];
        for (uint32_t i = 0; i < buf.length; ++i) {
            CLEAR(buf.m.planes[i]);
        }
    } else {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
    }

    // 从队列中取出一个缓冲区
    if (xioctl(VIDIOC_DQBUF, &buf) < 0) {
        if (m_is_multiplanar) {
            delete[] buf.m.planes;
        }
        if (errno == EAGAIN) {
            return -2;  // 暂时没有数据
        }
        fprintf(stderr, "VIDIOC_DQBUF failed: %s\n", strerror(errno));
        return -1;
    }

    if (m_is_multiplanar) {
        // 多平面格式：返回第一个平面的数据指针
        *frame_data = m_planes[buf.index][0];
        *frame_size = buf.m.planes[0].bytesused;
        if (planes) {
            *planes = buf.length;
        }
    } else {
        // 单平面格式
        *frame_data = m_buffers[buf.index];
        *frame_size = buf.bytesused;
        if (planes) {
            *planes = 1;
        }
    }

    // 将缓冲区重新入队
    // 注意：QBUF时不需要重新设置planes，使用DQBUF返回的值即可
    if (xioctl(VIDIOC_QBUF, &buf) < 0) {
        fprintf(stderr, "VIDIOC_QBUF failed: %s\n", strerror(errno));
        if (m_is_multiplanar) {
            delete[] buf.m.planes;
        }
        return -1;
    }

    if (m_is_multiplanar) {
        delete[] buf.m.planes;
    }

    return 0;
}

void CameraDevice::releaseFrame(void* frame_data) {
    // MMAP模式下，帧会在下次QBUF时自动释放
    (void)frame_data;
}

int CameraDevice::enumFormats(std::vector<uint32_t>& formats) {
    if (m_fd < 0) {
        return -1;
    }

    formats.clear();
    struct v4l2_fmtdesc fmt;
    CLEAR(fmt);
    fmt.type = m_is_multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.index = 0;

    while (xioctl(VIDIOC_ENUM_FMT, &fmt) == 0) {
        formats.push_back(fmt.pixelformat);
        fmt.index++;
    }

    return 0;
}

int CameraDevice::enumFrameSizes(uint32_t format, std::vector<std::pair<uint32_t, uint32_t>>& sizes) {
    if (m_fd < 0) {
        return -1;
    }

    sizes.clear();
    struct v4l2_frmsizeenum frm;
    CLEAR(frm);
    frm.pixel_format = format;
    frm.index = 0;

    if (xioctl(VIDIOC_ENUM_FRAMESIZES, &frm) < 0) {
        return -1;
    }

    if (frm.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        // 离散分辨率列表
        do {
            sizes.push_back(std::make_pair(frm.discrete.width, frm.discrete.height));
            frm.index++;
        } while (xioctl(VIDIOC_ENUM_FRAMESIZES, &frm) == 0);
    } else if (frm.type == V4L2_FRMSIZE_TYPE_STEPWISE || frm.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        // 步进或连续范围，返回范围边界
        sizes.push_back(std::make_pair(frm.stepwise.min_width, frm.stepwise.min_height));
        sizes.push_back(std::make_pair(frm.stepwise.max_width, frm.stepwise.max_height));
    }

    return 0;
}

int CameraDevice::setFrameRate(uint32_t fps) {
    if (m_fd < 0) {
        return -1;
    }

    struct v4l2_streamparm parm;
    CLEAR(parm);
    parm.type = m_is_multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;

    if (xioctl(VIDIOC_S_PARM, &parm) < 0) {
        fprintf(stderr, "VIDIOC_S_PARM failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int CameraDevice::getFrameRate(uint32_t* fps) {
    if (m_fd < 0 || !fps) {
        return -1;
    }

    struct v4l2_streamparm parm;
    CLEAR(parm);
    parm.type = m_is_multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(VIDIOC_G_PARM, &parm) < 0) {
        return -1;
    }

    if (parm.parm.capture.timeperframe.denominator > 0) {
        *fps = parm.parm.capture.timeperframe.denominator / parm.parm.capture.timeperframe.numerator;
    } else {
        *fps = 0;
    }

    return 0;
}

int CameraDevice::setControl(uint32_t id, int32_t value) {
    if (m_fd < 0) {
        return -1;
    }

    struct v4l2_control ctrl;
    CLEAR(ctrl);
    ctrl.id = id;
    ctrl.value = value;

    if (xioctl(VIDIOC_S_CTRL, &ctrl) < 0) {
        return -1;
    }

    return 0;
}

int CameraDevice::getControl(uint32_t id, int32_t* value) {
    if (m_fd < 0 || !value) {
        return -1;
    }

    struct v4l2_control ctrl;
    CLEAR(ctrl);
    ctrl.id = id;

    if (xioctl(VIDIOC_G_CTRL, &ctrl) < 0) {
        return -1;
    }

    *value = ctrl.value;
    return 0;
}

int CameraDevice::queryControl(uint32_t id, int32_t* min, int32_t* max, int32_t* step, int32_t* def) {
    if (m_fd < 0) {
        return -1;
    }

    struct v4l2_queryctrl qctrl;
    CLEAR(qctrl);
    qctrl.id = id;

    if (xioctl(VIDIOC_QUERYCTRL, &qctrl) < 0) {
        return -1;
    }

    if (min) *min = qctrl.minimum;
    if (max) *max = qctrl.maximum;
    if (step) *step = qctrl.step;
    if (def) *def = qctrl.default_value;

    return 0;
}

int CameraDevice::setExposure(int32_t exposure) {
    return setControl(V4L2_CID_EXPOSURE_ABSOLUTE, exposure);
}

int CameraDevice::getExposure(int32_t* exposure) {
    return getControl(V4L2_CID_EXPOSURE_ABSOLUTE, exposure);
}

int CameraDevice::setGain(int32_t gain) {
    return setControl(V4L2_CID_GAIN, gain);
}

int CameraDevice::getGain(int32_t* gain) {
    return getControl(V4L2_CID_GAIN, gain);
}

int CameraDevice::setWhiteBalance(int32_t wb) {
    // 先关闭自动白平衡
    setControl(V4L2_CID_AUTO_WHITE_BALANCE, 0);
    // 设置白平衡色温（如果支持）
    return setControl(V4L2_CID_WHITE_BALANCE_TEMPERATURE, wb);
}

int CameraDevice::getWhiteBalance(int32_t* wb) {
    return getControl(V4L2_CID_WHITE_BALANCE_TEMPERATURE, wb);
}

