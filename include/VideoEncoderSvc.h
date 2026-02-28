#ifndef VIDEO_ENCODER_SVC_H
#define VIDEO_ENCODER_SVC_H

#include "ServiceBase.h"
#include "VideoFrame.h"
#include <functional>
#include <memory>

/**
 * @brief 编码后的帧数据
 */
struct EncodedFrame {
    std::shared_ptr<uint8_t> data;  // 编码后的数据
    size_t size;                     // 数据大小
    uint64_t timestamp;              // 时间戳
    bool isKeyFrame;                 // 是否为关键帧
    uint32_t width;                  // 原始宽度
    uint32_t height;                 // 原始高度
};

/**
 * @brief 编码参数
 */
struct EncodeParams {
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t bitrate = 2000000;      // 2Mbps
    uint32_t fps = 30;
    uint32_t gop = 30;               // GOP大小
    bool useH265 = false;            // false=H264, true=H265
};

/**
 * @brief 视频编码服务
 * 
 * 职责：
 * - 接收 YUV 数据
 * - 编码为 H264/H265
 * - 回调编码后的数据
 */
class VideoEncoderSvc : public ServiceBase {
public:
    using EncodeCallback = std::function<void(const EncodedFrame&)>;

    VideoEncoderSvc();
    virtual ~VideoEncoderSvc();

    /**
     * @brief 设置编码参数
     */
    void setEncodeParams(const EncodeParams& params);

    /**
     * @brief 设置编码数据回调
     */
    void setEncodeCallback(EncodeCallback callback);

    /**
     * @brief 输入 YUV 数据帧
     * 
     * @param frame YUV 视频帧
     */
    void inputFrame(const VideoFrame& frame);

protected:
    void run() override;

private:
    /**
     * @brief 编码一帧数据
     */
    void encodeFrame(const VideoFrame& frame);

    /**
     * @brief 初始化编码器
     */
    bool initEncoder();

    /**
     * @brief 清理编码器
     */
    void cleanupEncoder();

    // 编码参数
    EncodeParams m_params;
    std::mutex m_paramsMutex;

    // 回调函数
    EncodeCallback m_callback;
    std::mutex m_callbackMutex;

    // 编码器状态
    bool m_encoderInitialized = false;
    
    // 帧队列
    std::queue<VideoFrame> m_frameQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
};

#endif // VIDEO_ENCODER_SVC_H

