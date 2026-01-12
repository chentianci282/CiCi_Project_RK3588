#ifndef DISPLAY_DEVICE_H
#define DISPLAY_DEVICE_H

#include <cstdint>
#include <string>
// 注意：需要先定义包含路径，让xf86drm.h能找到drm.h
// 在Makefile中已经添加了 -I$(DRM_CORE_INCLUDE_PATH)
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

/**
 * @brief 显示设备类（抽象层）
 * 封装DRM/KMS显示操作，提供基本的帧显示功能
 */
class DisplayDevice {
public:
    DisplayDevice();
    ~DisplayDevice();

    /**
     * @brief 初始化显示设备
     * @param device_path DRM设备路径，如 "/dev/dri/card0"，如果为空则自动查找
     * @param connector_id 连接器ID，如果为0则自动查找
     * @return 0成功，负数失败
     */
    int init(const std::string& device_path = "", uint32_t connector_id = 0);

    /**
     * @brief 反初始化，释放资源
     */
    void deinit();

    /**
     * @brief 创建framebuffer
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 像素格式（DRM格式，如DRM_FORMAT_XRGB8888）
     * @return 0成功，负数失败
     */
    int createFramebuffer(uint32_t width, uint32_t height, uint32_t format);

    /**
     * @brief 显示一帧数据
     * @param data 图像数据指针（RGB格式）
     * @param width 图像宽度
     * @param height 图像高度
     * @return 0成功，负数失败
     */
    int displayFrame(const void* data, uint32_t width, uint32_t height);

    /**
     * @brief 显示YUV数据（NV12格式）
     * @param yuv_data YUV数据指针
     * @param width 图像宽度
     * @param height 图像高度
     * @return 0成功，负数失败
     */
    int displayFrameYUV(const void* yuv_data, uint32_t width, uint32_t height);

    /**
     * @brief 获取显示分辨率
     * @param width 输出宽度
     * @param height 输出高度
     * @return 0成功，负数失败
     */
    int getDisplaySize(uint32_t* width, uint32_t* height);

    // Getter方法
    bool isInitialized() const { return m_initialized; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    uint32_t getConnectorId() const { return m_connector_id; }
    uint32_t getCrtcId() const { return m_crtc_id; }

private:
    // 禁止拷贝
    DisplayDevice(const DisplayDevice&) = delete;
    DisplayDevice& operator=(const DisplayDevice&) = delete;

    /**
     * @brief 查找可用的connector
     */
    int findConnector();

    /**
     * @brief 查找可用的crtc
     */
    int findCrtc();

    /**
     * @brief 分配framebuffer内存
     */
    int allocateFramebuffer(uint32_t width, uint32_t height, uint32_t format);

    /**
     * @brief 释放framebuffer内存
     */
    void freeFramebuffer();

    /**
     * @brief YUV转RGB（简单实现，用于快速验证）
     */
    void yuvToRgb(const uint8_t* yuv, uint8_t* rgb, uint32_t width, uint32_t height);

private:
    int m_fd;                          // DRM设备文件描述符
    bool m_initialized;                 // 是否已初始化
    std::string m_device_path;         // 设备路径
    
    // DRM资源
    drmModeRes* m_resources;           // DRM资源
    drmModeConnector* m_connector;     // 连接器
    drmModeCrtc* m_crtc;               // CRTC
    uint32_t m_connector_id;           // 连接器ID
    uint32_t m_crtc_id;                // CRTC ID
    
    // Framebuffer
    uint32_t m_fb_id;                  // Framebuffer ID
    uint32_t m_fb_width;                // Framebuffer宽度
    uint32_t m_fb_height;               // Framebuffer高度
    uint32_t m_fb_format;               // Framebuffer格式
    void* m_fb_data;                    // Framebuffer数据指针
    uint32_t m_fb_size;                 // Framebuffer大小
    uint32_t m_dumb_handle;             // Dumb buffer handle
    
    // 显示信息
    uint32_t m_width;                   // 显示宽度
    uint32_t m_height;                  // 显示高度
    uint32_t m_selected_mode_index;      // 选中的显示模式索引
    bool m_crtc_set;                     // CRTC是否已设置
};

#endif // DISPLAY_DEVICE_H

