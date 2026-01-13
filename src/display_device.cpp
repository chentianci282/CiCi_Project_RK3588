// 先包含drm.h，让xf86drm.h能找到它
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <drm/drm_fourcc.h>

#include "display_device.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <vector>
#include <cstring>

DisplayDevice::DisplayDevice()
    : m_fd(-1)
    , m_initialized(false)
    , m_resources(nullptr)
    , m_connector(nullptr)
    , m_crtc(nullptr)
    , m_connector_id(0)
    , m_crtc_id(0)
    , m_fb_id(0)
    , m_fb_width(0)
    , m_fb_height(0)
    , m_fb_format(0)
    , m_fb_data(nullptr)
    , m_fb_size(0)
    , m_dumb_handle(0)
    , m_width(0)
    , m_height(0)
    , m_selected_mode_index(0)
    , m_crtc_set(false)
{
}

DisplayDevice::~DisplayDevice() {
    deinit();
}

int DisplayDevice::init(const std::string& device_path, uint32_t connector_id) {
    printf("[DisplayDevice::init] Start initialization\n");
    
    if (m_initialized) {
        fprintf(stderr, "[DisplayDevice::init] Display already initialized\n");
        return -1;
    }

    // 打开DRM设备
    printf("[DisplayDevice::init] Opening DRM device...\n");
    if (device_path.empty()) {
        // 自动查找DRM设备
        printf("[DisplayDevice::init] Auto-detecting DRM device...\n");
        for (int i = 0; i < 16; ++i) {
            char path[64];
            snprintf(path, sizeof(path), "/dev/dri/card%d", i);
            printf("[DisplayDevice::init] Trying %s...\n", path);
            m_fd = open(path, O_RDWR | O_CLOEXEC);
            if (m_fd >= 0) {
                m_device_path = path;
                printf("[DisplayDevice::init] Opened DRM device: %s (fd=%d)\n", path, m_fd);
                break;
            } else {
                printf("[DisplayDevice::init] Failed to open %s: %s\n", path, strerror(errno));
            }
        }
    } else {
        m_device_path = device_path;
        printf("[DisplayDevice::init] Opening specified device: %s\n", device_path.c_str());
        m_fd = open(device_path.c_str(), O_RDWR | O_CLOEXEC);
    }

    if (m_fd < 0) {
        fprintf(stderr, "[DisplayDevice::init] Cannot open DRM device: %s\n", strerror(errno));
        return -1;
    }
    printf("[DisplayDevice::init] DRM device opened successfully, fd=%d\n", m_fd);

    // 获取DRM资源
    printf("[DisplayDevice::init] Getting DRM resources...\n");
    m_resources = drmModeGetResources(m_fd);
    if (!m_resources) {
        fprintf(stderr, "[DisplayDevice::init] Cannot get DRM resources: %s\n", strerror(errno));
        close(m_fd);
        m_fd = -1;
        return -1;
    }
    printf("[DisplayDevice::init] DRM resources: %d connectors, %d crtcs, %d encoders\n",
           m_resources->count_connectors, m_resources->count_crtcs, m_resources->count_encoders);

    // 查找connector
    printf("[DisplayDevice::init] Finding connector (requested_id=%u)...\n", connector_id);
    m_connector_id = connector_id;
    if (findConnector() < 0) {
        fprintf(stderr, "[DisplayDevice::init] Cannot find connector\n");
        drmModeFreeResources(m_resources);
        close(m_fd);
        m_fd = -1;
        return -1;
    }
    printf("[DisplayDevice::init] Found connector: id=%u, connection=%s, modes=%d\n",
           m_connector_id,
           m_connector->connection == DRM_MODE_CONNECTED ? "connected" : "disconnected",
           m_connector->count_modes);

    // 查找crtc
    printf("[DisplayDevice::init] Finding CRTC...\n");
    if (findCrtc() < 0) {
        fprintf(stderr, "[DisplayDevice::init] Cannot find CRTC\n");
        drmModeFreeConnector(m_connector);
        drmModeFreeResources(m_resources);
        close(m_fd);
        m_fd = -1;
        return -1;
    }
    printf("[DisplayDevice::init] Found CRTC: id=%u\n", m_crtc_id);
    if (m_crtc) {
        printf("[DisplayDevice::init] CRTC info: x=%d, y=%d, mode_valid=%d, buffer_id=%u\n",
               m_crtc->x, m_crtc->y, m_crtc->mode_valid, m_crtc->buffer_id);
    }

    // 获取显示分辨率
    if (m_connector->count_modes > 0) {
        // 查找1080x1920模式，如果没有则使用第一个模式
        m_selected_mode_index = 0;
        printf("[DisplayDevice::init] Available display modes:\n");
        for (int i = 0; i < m_connector->count_modes; ++i) {
            printf("[DisplayDevice::init]   mode[%d]: %ux%u@%dHz\n",
                   i, m_connector->modes[i].hdisplay, m_connector->modes[i].vdisplay,
                   m_connector->modes[i].vrefresh);
            // 优先选择1080x1920模式
            if (m_connector->modes[i].hdisplay == 1080 && m_connector->modes[i].vdisplay == 1920) {
                m_selected_mode_index = i;
                printf("[DisplayDevice::init] ✓ Found 1080x1920 mode, will use mode[%d]\n", i);
            }
        }
        m_width = m_connector->modes[m_selected_mode_index].hdisplay;
        m_height = m_connector->modes[m_selected_mode_index].vdisplay;
        printf("[DisplayDevice::init] Selected display mode[%d]: %ux%u@%dHz\n",
               m_selected_mode_index, m_width, m_height, 
               m_connector->modes[m_selected_mode_index].vrefresh);
    } else {
        fprintf(stderr, "[DisplayDevice::init] No display modes available\n");
        drmModeFreeCrtc(m_crtc);
        drmModeFreeConnector(m_connector);
        drmModeFreeResources(m_resources);
        close(m_fd);
        m_fd = -1;
        return -1;
    }

    printf("[DisplayDevice::init] Display initialized successfully:\n");
    printf("  Device: %s\n", m_device_path.c_str());
    printf("  Connector: %u (%s)\n", m_connector_id, 
           m_connector->connection == DRM_MODE_CONNECTED ? "connected" : "disconnected");
    printf("  CRTC: %u\n", m_crtc_id);
    printf("  Resolution: %ux%u\n", m_width, m_height);

    m_initialized = true;
    return 0;
}

void DisplayDevice::deinit() {
    // 恢复CRTC原始状态（如果之前设置过）
    if (m_crtc_set && m_crtc && m_fd >= 0) {
        drmModeSetCrtc(m_fd, m_crtc->crtc_id, m_crtc->buffer_id,
                       m_crtc->x, m_crtc->y, &m_connector_id, 1, &m_crtc->mode);
        m_crtc_set = false;
    }

    freeFramebuffer();

    if (m_crtc) {
        drmModeFreeCrtc(m_crtc);
        m_crtc = nullptr;
    }

    if (m_connector) {
        drmModeFreeConnector(m_connector);
        m_connector = nullptr;
    }

    if (m_resources) {
        drmModeFreeResources(m_resources);
        m_resources = nullptr;
    }

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }

    m_initialized = false;
}

int DisplayDevice::findConnector() {
    if (m_connector_id != 0) {
        // 使用指定的connector
        m_connector = drmModeGetConnector(m_fd, m_connector_id);
        if (m_connector && m_connector->connection == DRM_MODE_CONNECTED) {
            return 0;
        }
        if (m_connector) {
            drmModeFreeConnector(m_connector);
            m_connector = nullptr;
        }
    }

    // 自动查找第一个已连接的connector
    for (int i = 0; i < m_resources->count_connectors; ++i) {
        drmModeConnector* conn = drmModeGetConnector(m_fd, m_resources->connectors[i]);
        if (conn && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            m_connector = conn;
            m_connector_id = conn->connector_id;
            return 0;
        }
        if (conn) {
            drmModeFreeConnector(conn);
        }
    }

    return -1;
}

int DisplayDevice::findCrtc() {
    printf("[DisplayDevice::findCrtc] Finding CRTC...\n");
    
    if (m_connector->encoder_id) {
        printf("[DisplayDevice::findCrtc] Connector has encoder_id: %u\n", m_connector->encoder_id);
        drmModeEncoder* encoder = drmModeGetEncoder(m_fd, m_connector->encoder_id);
        if (encoder) {
            m_crtc_id = encoder->crtc_id;
            printf("[DisplayDevice::findCrtc] Encoder provides CRTC ID: %u\n", m_crtc_id);
            drmModeFreeEncoder(encoder);
        }
    }

    if (m_crtc_id == 0) {
        // 查找可用的crtc
        printf("[DisplayDevice::findCrtc] No CRTC from encoder, searching for available CRTC...\n");
        printf("[DisplayDevice::findCrtc] Available CRTCs: ");
        for (int i = 0; i < m_resources->count_crtcs; ++i) {
            printf("%u ", m_resources->crtcs[i]);
        }
        printf("\n");
        
        for (int i = 0; i < m_resources->count_crtcs; ++i) {
            uint32_t crtc_id = m_resources->crtcs[i];
            printf("[DisplayDevice::findCrtc] Trying CRTC %u...\n", crtc_id);
            m_crtc = drmModeGetCrtc(m_fd, crtc_id);
            if (m_crtc) {
                m_crtc_id = crtc_id;
                printf("[DisplayDevice::findCrtc] Selected CRTC: %u\n", m_crtc_id);
                printf("[DisplayDevice::findCrtc] CRTC state: buffer_id=%u, x=%d, y=%d, mode_valid=%d\n",
                       m_crtc->buffer_id, m_crtc->x, m_crtc->y, m_crtc->mode_valid);
                return 0;
            } else {
                printf("[DisplayDevice::findCrtc] Failed to get CRTC %u: %s\n", crtc_id, strerror(errno));
            }
        }
        fprintf(stderr, "[DisplayDevice::findCrtc] No available CRTC found\n");
        return -1;
    }

    printf("[DisplayDevice::findCrtc] Using CRTC from encoder: %u\n", m_crtc_id);
    m_crtc = drmModeGetCrtc(m_fd, m_crtc_id);
    if (m_crtc) {
        printf("[DisplayDevice::findCrtc] CRTC state: buffer_id=%u, x=%d, y=%d, mode_valid=%d\n",
               m_crtc->buffer_id, m_crtc->x, m_crtc->y, m_crtc->mode_valid);
        return 0;
    } else {
        fprintf(stderr, "[DisplayDevice::findCrtc] Failed to get CRTC %u: %s\n", m_crtc_id, strerror(errno));
        return -1;
    }
}

int DisplayDevice::createFramebuffer(uint32_t width, uint32_t height, uint32_t format) {
    printf("[DisplayDevice::createFramebuffer] Request: %ux%u, format=0x%08x\n", width, height, format);
    
    if (!m_initialized) {
        fprintf(stderr, "[DisplayDevice::createFramebuffer] Display not initialized\n");
        return -1;
    }

    // 如果framebuffer已存在且尺寸相同，直接返回
    if (m_fb_id != 0 && m_fb_width == width && m_fb_height == height && m_fb_format == format) {
        printf("[DisplayDevice::createFramebuffer] Framebuffer already exists (fb_id=%u), reusing\n", m_fb_id);
        return 0;
    }

    // 释放旧的framebuffer
    if (m_fb_id != 0) {
        printf("[DisplayDevice::createFramebuffer] Freeing old framebuffer (fb_id=%u)\n", m_fb_id);
        freeFramebuffer();
    }

    // 分配新的framebuffer
    printf("[DisplayDevice::createFramebuffer] Allocating new framebuffer...\n");
    if (allocateFramebuffer(width, height, format) < 0) {
        fprintf(stderr, "[DisplayDevice::createFramebuffer] Failed to allocate framebuffer\n");
        return -1;
    }

    m_fb_width = width;
    m_fb_height = height;
    m_fb_format = format;

    printf("[DisplayDevice::createFramebuffer] Framebuffer created successfully: %ux%u, format=0x%08x, fb_id=%u, size=%u bytes\n",
           width, height, format, m_fb_id, m_fb_size);

    return 0;
}

int DisplayDevice::allocateFramebuffer(uint32_t width, uint32_t height, uint32_t format) {
    printf("[DisplayDevice::allocateFramebuffer] Creating dumb buffer: %ux%u, bpp=32\n", width, height);
    
    struct drm_mode_create_dumb create_req;
    memset(&create_req, 0, sizeof(create_req));
    create_req.width = width;
    create_req.height = height;
    create_req.bpp = 32;  // 32位RGB
    create_req.flags = 0;

    if (drmIoctl(m_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        fprintf(stderr, "[DisplayDevice::allocateFramebuffer] Cannot create dumb buffer: %s (errno=%d)\n", 
                strerror(errno), errno);
        return -1;
    }

    uint32_t handle = create_req.handle;
    uint32_t pitch = create_req.pitch;
    uint64_t size = create_req.size;
    printf("[DisplayDevice::allocateFramebuffer] Dumb buffer created: handle=%u, pitch=%u, size=%lu\n",
           handle, pitch, size);

    // 添加framebuffer
    printf("[DisplayDevice::allocateFramebuffer] Adding framebuffer to DRM...\n");
    uint32_t fb_id = 0;
    uint32_t offsets[4] = {0};
    uint32_t pitches[4] = {pitch, 0, 0, 0};
    uint32_t handles[4] = {handle, 0, 0, 0};
    if (drmModeAddFB2(m_fd, width, height, format, handles, pitches, offsets, &fb_id, 0) < 0) {
        fprintf(stderr, "[DisplayDevice::allocateFramebuffer] Cannot add framebuffer: %s (errno=%d)\n", 
                strerror(errno), errno);
        struct drm_mode_destroy_dumb destroy_req;
        destroy_req.handle = handle;
        drmIoctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        return -1;
    }
    printf("[DisplayDevice::allocateFramebuffer] Framebuffer added: fb_id=%u\n", fb_id);

    // 映射内存
    printf("[DisplayDevice::allocateFramebuffer] Mapping dumb buffer to memory...\n");
    struct drm_mode_map_dumb map_req;
    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = handle;
    if (drmIoctl(m_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        fprintf(stderr, "[DisplayDevice::allocateFramebuffer] Cannot map dumb buffer: %s (errno=%d)\n", 
                strerror(errno), errno);
        drmModeRmFB(m_fd, fb_id);
        struct drm_mode_destroy_dumb destroy_req;
        destroy_req.handle = handle;
        drmIoctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        return -1;
    }
    printf("[DisplayDevice::allocateFramebuffer] Map offset: 0x%llx\n", map_req.offset);

    m_fb_data = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, map_req.offset);
    if (m_fb_data == MAP_FAILED) {
        fprintf(stderr, "[DisplayDevice::allocateFramebuffer] Cannot mmap framebuffer: %s (errno=%d)\n", 
                strerror(errno), errno);
        drmModeRmFB(m_fd, fb_id);
        struct drm_mode_destroy_dumb destroy_req;
        destroy_req.handle = handle;
        drmIoctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        return -1;
    }
    printf("[DisplayDevice::allocateFramebuffer] Memory mapped successfully: %p, size=%lu\n", m_fb_data, size);

    m_fb_id = fb_id;
    m_fb_size = size;
    m_dumb_handle = handle;  // 需要保存handle用于清理

    return 0;
}

void DisplayDevice::freeFramebuffer() {
    if (m_fb_data && m_fb_data != MAP_FAILED) {
        munmap(m_fb_data, m_fb_size);
        m_fb_data = nullptr;
    }

    if (m_fb_id != 0) {
        drmModeRmFB(m_fd, m_fb_id);
        m_fb_id = 0;
    }

    if (m_dumb_handle != 0) {
        struct drm_mode_destroy_dumb destroy_req;
        destroy_req.handle = m_dumb_handle;
        drmIoctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        m_dumb_handle = 0;
    }

    m_fb_size = 0;
}

int DisplayDevice::displayFrame(const void* data, uint32_t width, uint32_t height) {
    if (!m_initialized || !data) {
        return -1;
    }

    // 创建framebuffer（如果需要）
    if (createFramebuffer(width, height, DRM_FORMAT_XRGB8888) < 0) {
        return -1;
    }

    // 拷贝数据到framebuffer
    uint32_t stride = width * 4;  // 32位RGB
    const uint8_t* src = static_cast<const uint8_t*>(data);
    uint8_t* dst = static_cast<uint8_t*>(m_fb_data);

    for (uint32_t y = 0; y < height; ++y) {
        memcpy(dst + y * m_fb_width * 4, src + y * stride, width * 4);
    }

    // 只在第一次设置CRTC，之后只需要更新framebuffer内容
    if (!m_crtc_set) {
        int ret = drmModeSetCrtc(m_fd, m_crtc_id, m_fb_id, 0, 0,
                                 &m_connector_id, 1, &m_connector->modes[m_selected_mode_index]);
        if (ret < 0) {
            fprintf(stderr, "[DisplayDevice::displayFrame] Cannot set CRTC: %s (errno=%d)\n", 
                    strerror(errno), errno);
            // 不要设置为true，让后续可以重试
            return -1;
        }
        printf("[DisplayDevice::displayFrame] CRTC set successfully, fb_id=%u\n", m_fb_id);
        m_crtc_set = true;
    }

    return 0;
}

int DisplayDevice::displayFrameYUV(const void* yuv_data, uint32_t width, uint32_t height) {
    if (!m_initialized || !yuv_data) {
        fprintf(stderr, "[DisplayDevice::displayFrameYUV] Not initialized or invalid data\n");
        return -1;
    }

    // 使用屏幕分辨率创建framebuffer（1080x1920）
    uint32_t fb_width = m_width;   // 屏幕宽度 1080
    uint32_t fb_height = m_height; // 屏幕高度 1920
    
    if (createFramebuffer(fb_width, fb_height, DRM_FORMAT_XRGB8888) < 0) {
        fprintf(stderr, "[DisplayDevice::displayFrameYUV] Failed to create framebuffer\n");
        return -1;
    }

    // YUV转RGB
    printf("[DisplayDevice::displayFrameYUV] Converting YUV to RGB: %ux%u -> %ux%u\n", 
           width, height, fb_width, fb_height);
    uint8_t* rgb = static_cast<uint8_t*>(m_fb_data);
    
    // 直接转换（注意：如果输入分辨率与屏幕分辨率不匹配，可能会显示异常）
    if (width != fb_width || height != fb_height) {
        fprintf(stderr, "[DisplayDevice::displayFrameYUV] ⚠️  Resolution mismatch: input %ux%u, screen %ux%u\n",
                width, height, fb_width, fb_height);
    }
    yuvToRgb(static_cast<const uint8_t*>(yuv_data), rgb, width, height);
    printf("[DisplayDevice::displayFrameYUV] YUV to RGB conversion completed\n");

    // 只在第一次设置CRTC，之后只需要更新framebuffer内容
    if (!m_crtc_set) {
        printf("[DisplayDevice::displayFrameYUV] Setting CRTC (first time)...\n");
        printf("[DisplayDevice::displayFrameYUV]   CRTC ID: %u\n", m_crtc_id);
        printf("[DisplayDevice::displayFrameYUV]   FB ID: %u\n", m_fb_id);
        printf("[DisplayDevice::displayFrameYUV]   Connector ID: %u\n", m_connector_id);
        printf("[DisplayDevice::displayFrameYUV]   Mode: %ux%u@%dHz (mode_index=%u)\n",
               m_connector->modes[m_selected_mode_index].hdisplay, 
               m_connector->modes[m_selected_mode_index].vdisplay,
               m_connector->modes[m_selected_mode_index].vrefresh,
               m_selected_mode_index);
        printf("[DisplayDevice::displayFrameYUV]   Framebuffer: %ux%u\n", width, height);
        
        // 检查CRTC当前状态
        if (m_crtc) {
            printf("[DisplayDevice::displayFrameYUV]   Current CRTC state: buffer_id=%u, mode_valid=%d\n",
                   m_crtc->buffer_id, m_crtc->mode_valid);
            
            // 如果CRTC已经被其他framebuffer占用，先尝试释放
            if (m_crtc->buffer_id != 0 && m_crtc->buffer_id != m_fb_id) {
                printf("[DisplayDevice::displayFrameYUV]   CRTC is using FB %u, will replace with FB %u\n",
                       m_crtc->buffer_id, m_fb_id);
            }
        }
        
        // 重新获取CRTC状态（可能已变化）
        if (m_crtc) {
            drmModeFreeCrtc(m_crtc);
        }
        m_crtc = drmModeGetCrtc(m_fd, m_crtc_id);
        
        int ret = drmModeSetCrtc(m_fd, m_crtc_id, m_fb_id, 0, 0,
                                 &m_connector_id, 1, &m_connector->modes[m_selected_mode_index]);
        if (ret < 0) {
            fprintf(stderr, "[DisplayDevice::displayFrameYUV] ❌ Cannot set CRTC: %s (errno=%d)\n", 
                    strerror(errno), errno);
            fprintf(stderr, "[DisplayDevice::displayFrameYUV]   CRTC ID: %u\n", m_crtc_id);
            fprintf(stderr, "[DisplayDevice::displayFrameYUV]   FB ID: %u\n", m_fb_id);
            fprintf(stderr, "[DisplayDevice::displayFrameYUV]   Connector ID: %u\n", m_connector_id);
            fprintf(stderr, "[DisplayDevice::displayFrameYUV]   Mode: %ux%u@%dHz (mode_index=%u)\n",
                    m_connector->modes[m_selected_mode_index].hdisplay, 
                    m_connector->modes[m_selected_mode_index].vdisplay,
                    m_connector->modes[m_selected_mode_index].vrefresh,
                    m_selected_mode_index);
            
            // 检查是否有其他framebuffer占用
            printf("[DisplayDevice::displayFrameYUV] Checking for other framebuffers...\n");
            for (int i = 0; i < m_resources->count_fbs; ++i) {
                uint32_t fb_id = m_resources->fbs[i];
                drmModeFB* fb = drmModeGetFB(m_fd, fb_id);
                if (fb) {
                    printf("[DisplayDevice::displayFrameYUV]   Existing FB: id=%u, width=%u, height=%u, pitch=%u\n",
                           fb_id, fb->width, fb->height, fb->pitch);
                    drmModeFreeFB(fb);
                }
            }
            
            fprintf(stderr, "[DisplayDevice::displayFrameYUV]   CRTC may be already in use by another application\n");
            fprintf(stderr, "[DisplayDevice::displayFrameYUV]   Try: stop other display applications or use a different CRTC\n");
            // 不要设置为true，让后续可以重试（但降低频率）
            // 注意：如果CRTC设置失败，不要标记为已设置，这样后续可以重试
            return -1;
        }
        printf("[DisplayDevice::displayFrameYUV] ✅ CRTC set successfully, fb_id=%u\n", m_fb_id);
        m_crtc_set = true;
    } else {
        // 静默更新，减少打印
        // printf("[DisplayDevice::displayFrameYUV] CRTC already set, just updating framebuffer content\n");
    }

    return 0;
}

void DisplayDevice::yuvToRgb(const uint8_t* yuv, uint8_t* rgb, uint32_t width, uint32_t height) {
    // NV12转RGB实现
    // NV12格式：Y平面 + 交错的UV平面（UVUV...）
    // 注意：NV12的UV顺序是U,V,U,V...（不是V,U）
    const uint8_t* y_plane = yuv;
    const uint8_t* uv_plane = yuv + width * height;
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // Y值
            int y_val = y_plane[y * width + x];
            
            // UV索引：每个UV对对应2x2的Y像素块
            uint32_t uv_x = x / 2;
            uint32_t uv_y = y / 2;
            uint32_t uv_idx = uv_y * (width / 2) + uv_x;
            uint32_t uv_offset = uv_idx * 2;  // 每个UV对占2字节
            
            // NV12格式：UV平面存储顺序
            // 标准NV12是U,V,U,V...，但有些系统可能是V,U,V,U...
            // 如果颜色不对（比如偏绿或偏紫），尝试交换U和V
            // 先尝试标准NV12：U在前
            int u_val = uv_plane[uv_offset] - 128;
            int v_val = uv_plane[uv_offset + 1] - 128;
            
            // 如果颜色不对，可以尝试交换U和V（取消下面两行的注释）
            // int temp = u_val;
            // u_val = v_val;
            // v_val = temp;

            // YUV to RGB转换（ITU-R BT.601标准）
            // 使用浮点数计算以提高精度
            float y_f = static_cast<float>(y_val);
            float u_f = static_cast<float>(u_val);
            float v_f = static_cast<float>(v_val);
            
            int r = static_cast<int>(y_f + 1.402f * v_f);
            int g = static_cast<int>(y_f - 0.344f * u_f - 0.714f * v_f);
            int b = static_cast<int>(y_f + 1.772f * u_f);

            // 限制范围到0-255
            r = (r < 0) ? 0 : (r > 255) ? 255 : r;
            g = (g < 0) ? 0 : (g > 255) ? 255 : g;
            b = (b < 0) ? 0 : (b > 255) ? 255 : b;

            // 写入RGB数据（XRGB8888格式：内存布局是 B, G, R, X）
            uint32_t idx = (y * width + x) * 4;
            rgb[idx + 0] = static_cast<uint8_t>(b);  // B
            rgb[idx + 1] = static_cast<uint8_t>(g);  // G
            rgb[idx + 2] = static_cast<uint8_t>(r);  // R
            rgb[idx + 3] = 0;  // X (未使用，alpha通道)
        }
    }
}

int DisplayDevice::getDisplaySize(uint32_t* width, uint32_t* height) {
    if (!m_initialized || !width || !height) {
        return -1;
    }

    *width = m_width;
    *height = m_height;
    return 0;
}

