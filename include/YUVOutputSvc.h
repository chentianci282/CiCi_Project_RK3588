#ifndef YUV_OUTPUT_SVC_H
#define YUV_OUTPUT_SVC_H

#include "ServiceBase.h"
#include "VideoFrame.h"
#include <functional>

/**
 * @brief YUV 数据输出服务
 * 
 * 职责：
 * - 接收 YUV 数据
 * - 回调给应用层（算法处理）
 */
class YUVOutputSvc : public ServiceBase {
public:
    using YUVCallback = std::function<void(const VideoFrame&)>;

    YUVOutputSvc();
    virtual ~YUVOutputSvc();

    /**
     * @brief 设置 YUV 回调函数
     */
    void setYUVCallback(YUVCallback callback);

    /**
     * @brief 输入 YUV 数据帧
     */
    void inputFrame(const VideoFrame& frame);

protected:
    void run() override;

private:
    /**
     * @brief 处理一帧数据
     */
    void processFrame(const VideoFrame& frame);

    // 回调函数
    YUVCallback m_callback;
    std::mutex m_callbackMutex;

    // 帧队列
    std::queue<VideoFrame> m_frameQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
};

#endif // YUV_OUTPUT_SVC_H

