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
     * @brief 设置 MPP 参数（绑定模式下使用）
     * 
     * @param vpssGrpId VPSS 组ID
     * @param vpssChnId VPSS 通道ID
     */
    void setMPPParams(int vpssGrpId, int vpssChnId);

protected:
    void run() override;

private:
    /**
     * @brief 从 VPSS 获取 YUV 帧（绑定模式下）
     */
    bool getYUVFrame();

    /**
     * @brief 处理一帧数据
     */
    void processFrame(const VideoFrame& frame);

    // 回调函数
    YUVCallback m_callback;
    std::mutex m_callbackMutex;

    // MPP 参数（绑定模式）
    int m_vpssGrpId = -1;
    int m_vpssChnId = -1;
    bool m_useBindingMode = false;  // 是否使用绑定模式
};

#endif // YUV_OUTPUT_SVC_H

