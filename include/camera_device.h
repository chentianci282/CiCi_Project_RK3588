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
     * @param frame_data 输出帧数据指针（多平面格式时返回数组）
     * @param frame_size 输出帧大小（字节）
     * @param planes 输出平面数量（多平面格式）
     * @return 0成功，-1错误，-2暂时无数据
     */
    int captureFrame(void** frame_data, uint32_t* frame_size, uint32_t* planes = nullptr);

    /**
     * @brief 释放帧（MMAP模式下自动管理）
     */
    void releaseFrame(void* frame_data);

    /**
     * @brief 枚举支持的像素格式
     * @param formats 输出格式列表
     * @return 0成功，负数失败
     */
    int enumFormats(std::vector<uint32_t>& formats);

    /**
     * @brief 枚举指定格式支持的分辨率
     * @param format 像素格式
     * @param sizes 输出分辨率列表
     * @return 0成功，负数失败
     */
    int enumFrameSizes(uint32_t format, std::vector<std::pair<uint32_t, uint32_t>>& sizes);

    /**
     * @brief 设置帧率
     * @param fps 帧率（fps）
     * @return 0成功，负数失败
     */
    int setFrameRate(uint32_t fps);

    /**
     * @brief 获取当前帧率
     * @param fps 输出帧率
     * @return 0成功，负数失败
     */
    int getFrameRate(uint32_t* fps);

    /**
     * @brief 设置曝光值
     * @param exposure 曝光值
     * @return 0成功，负数失败
     */
    int setExposure(int32_t exposure);

    /**
     * @brief 获取曝光值
     * @param exposure 输出曝光值
     * @return 0成功，负数失败
     */
    int getExposure(int32_t* exposure);

    /**
     * @brief 设置增益
     * @param gain 增益值
     * @return 0成功，负数失败
     */
    int setGain(int32_t gain);

    /**
     * @brief 获取增益
     * @param gain 输出增益值
     * @return 0成功，负数失败
     */
    int getGain(int32_t* gain);

    /**
     * @brief 设置白平衡
     * @param wb 白平衡值（色温，单位K）
     * @return 0成功，负数失败
     */
    int setWhiteBalance(int32_t wb);

    /**
     * @brief 获取白平衡
     * @param wb 输出白平衡值
     * @return 0成功，负数失败
     */
    int getWhiteBalance(int32_t* wb);

    // Getter方法
    bool isStreaming() const { return m_is_streaming; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    uint32_t getPixelFormat() const { return m_pixel_format; }
    const std::string& getDevicePath() const { return m_device_path; }
    bool isMultiplanar() const { return m_is_multiplanar; }

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

    /**
     * @brief 设置V4L2控制参数
     */
    int setControl(uint32_t id, int32_t value);

    /**
     * @brief 获取V4L2控制参数
     */
    int getControl(uint32_t id, int32_t* value);

    /**
     * @brief 查询控制参数范围
     */
    int queryControl(uint32_t id, int32_t* min, int32_t* max, int32_t* step, int32_t* def);

private:
    int m_fd;                              // 设备文件描述符
    std::string m_device_path;            // 设备路径
    uint32_t m_width;                      // 图像宽度
    uint32_t m_height;                     // 图像高度
    uint32_t m_pixel_format;               // 像素格式
    uint32_t m_buffer_count;               // 缓冲区数量
    bool m_is_multiplanar;                 // 是否多平面格式
    std::vector<void*> m_buffers;          // 缓冲区指针数组（单平面）
    std::vector<std::vector<void*>> m_planes; // 多平面缓冲区（每个缓冲区可能有多个平面）
    std::vector<uint32_t> m_buffer_lengths; // 每个缓冲区的长度（单平面）
    std::vector<std::vector<uint32_t>> m_plane_lengths; // 每个平面的长度（多平面）
    bool m_is_streaming;                   // 是否正在采集
};

#endif // CAMERA_DEVICE_H
