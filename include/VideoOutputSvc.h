#ifndef VIDEO_OUTPUT_SVC_H
#define VIDEO_OUTPUT_SVC_H

#include "ServiceBase.h"
#include "VideoFrame.h"

/**
 * @brief 显示参数
 */
struct DisplayParams {
    int displayWidth = 1920;   // 显示宽度
    int displayHeight = 1080;  // 显示高度
    int x = 0;                 // 显示位置 X
    int y = 0;                 // 显示位置 Y
    int layer = 0;             // 显示层
    bool enable = true;        // 是否启用
};

/**
 * @brief 视频输出服务
 * 
 * 职责：
 * - 接收 YUV 数据
 * - 输出到 VO 硬件（显示）
 * - 管理显示窗口
 */
class VideoOutputSvc : public ServiceBase {
public:
    VideoOutputSvc();
    virtual ~VideoOutputSvc();

    /**
     * @brief 设置显示参数
     */
    void setDisplayParams(const DisplayParams& params);

    /**
     * @brief 输入 YUV 数据帧
     */
    void inputFrame(const VideoFrame& frame);

    /**
     * @brief 显示控制
     */
    void show();
    void hide();

protected:
    void run() override;

private:
    /**
     * @brief 显示一帧数据
     */
    void displayFrame(const VideoFrame& frame);

    /**
     * @brief 初始化 VO 硬件
     */
    bool initVO();

    /**
     * @brief 清理 VO 硬件
     */
    void cleanupVO();

    // 显示参数
    DisplayParams m_params;
    std::mutex m_paramsMutex;

    // VO 状态
    bool m_voInitialized = false;
    bool m_visible = true;

    // 帧队列
    std::queue<VideoFrame> m_frameQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
};

#endif // VIDEO_OUTPUT_SVC_H

