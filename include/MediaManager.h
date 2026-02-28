#ifndef MEDIA_MANAGER_H
#define MEDIA_MANAGER_H

#include "CaptureThread.h"
#include "VideoEncoderSvc.h"
#include "VideoOutputSvc.h"
#include "YUVOutputSvc.h"
#include <memory>
#include <functional>

/**
 * @brief 媒体管理器
 * 
 * 职责：
 * - 创建和管理所有服务
 * - 协调数据流转
 * - 生命周期管理
 */
class MediaManager {
public:
    MediaManager();
    ~MediaManager();

    /**
     * @brief 初始化（创建所有服务）
     */
    bool init();

    /**
     * @brief 反初始化（清理所有服务）
     */
    void deinit();

    /**
     * @brief 启动所有服务
     */
    void start();

    /**
     * @brief 停止所有服务
     */
    void stop();

    /**
     * @brief 设置采集源
     */
    void setCaptureSource(std::shared_ptr<CaptureThread> capture);

    /**
     * @brief 获取服务实例
     */
    std::shared_ptr<VideoEncoderSvc> getEncoderService() { return m_encoderSvc; }
    std::shared_ptr<VideoOutputSvc> getOutputService() { return m_outputSvc; }
    std::shared_ptr<YUVOutputSvc> getYUVService() { return m_yuvSvc; }
    std::shared_ptr<CaptureThread> getCaptureSource() { return m_capture; }

private:
    /**
     * @brief 帧数据分发回调
     */
    void onFrameAvailable(const VideoFrame& frame);

    // 采集源
    std::shared_ptr<CaptureThread> m_capture;

    // 服务实例
    std::shared_ptr<VideoEncoderSvc> m_encoderSvc;
    std::shared_ptr<VideoOutputSvc> m_outputSvc;
    std::shared_ptr<YUVOutputSvc> m_yuvSvc;

    // 状态
    bool m_initialized = false;
};

#endif // MEDIA_MANAGER_H

