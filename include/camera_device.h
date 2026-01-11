#ifndef CAMERA_DEVICE_H
#define CAMERA_DEVICE_H

#include <cstdint>
#include <string>
#include <vector>
#include <linux/videodev2.h>

/**
 * @brief 摄像头设备类
 * 封装V4L2摄像头操作，提供图像采集功能
 */
class CameraDevice {
public:
    CameraDevice();
    ~CameraDevice();

    /**
     * @brief 初始化摄像头设备
     * @param device_path 设备路径，如 "/dev/video62"
     * @param width 图像宽度
     * @param height 图像高度
     * @param pixel_format 像素格式，如 V4L2_PIX_FMT_NV12
     * @return 0成功，负数失败
     */
    int init(const std::string& device_path, 
             uint32_t width, 
             uint32_t height, 
             uint32_t pixel_format = V4L2_PIX_FMT_NV12);

    /**
     * @brief 反初始化，释放资源
     */
    void deinit();

    /**
     * @brief 启动图像采集
     * @return 0成功，负数失败
     */
    int start();

    /**
     * @brief 停止图像采集
     */
    void stop();

    /**
     * @brief 捕获一帧图像
     * @param frame_data 输出帧数据指针
     * @param frame_size 输出帧大小（字节）
     * @return 0成功，-1错误，-2暂时无数据
     */
    int captureFrame(void** frame_data, uint32_t* frame_size);

    /**
     * @brief 释放帧（MMAP模式下自动管理）
     */
    void releaseFrame(void* frame_data);

    // Getter方法
    bool isStreaming() const { return m_is_streaming; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    uint32_t getPixelFormat() const { return m_pixel_format; }
    const std::string& getDevicePath() const { return m_device_path; }

private:
    // 禁止拷贝
    CameraDevice(const CameraDevice&) = delete;
    CameraDevice& operator=(const CameraDevice&) = delete;

    /**
     * @brief 执行ioctl，自动重试
     */
    int xioctl(int request, void* arg);

    /**
     * @brief 初始化MMAP缓冲区
     */
    int initMmap();

    /**
     * @brief 清理MMAP缓冲区
     */
    void cleanupMmap();

private:
    int m_fd;                              // 设备文件描述符
    std::string m_device_path;            // 设备路径
    uint32_t m_width;                      // 图像宽度
    uint32_t m_height;                     // 图像高度
    uint32_t m_pixel_format;               // 像素格式
    uint32_t m_buffer_count;               // 缓冲区数量
    std::vector<void*> m_buffers;          // 缓冲区指针数组
    std::vector<uint32_t> m_buffer_lengths; // 每个缓冲区的长度
    bool m_is_streaming;                   // 是否正在采集
};

#endif // CAMERA_DEVICE_H
