#ifndef MEDIA_MANAGER_H
#define MEDIA_MANAGER_H

#include "VideoEncoderSvc.h"
#include "VideoOutputSvc.h"
#include "YUVOutputSvc.h"
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

/**
 * @brief 媒体管理器
 * 
 * 职责：
 * - 创建和管理所有服务
 * - 执行 MPP 绑定操作（VI → VPSS → VENC/VO）
 * - 协调数据流转
 * - 生命周期管理
 */
class MediaManager {
public:
    MediaManager();
    ~MediaManager();

    /**
     * @brief 初始化（创建所有服务，执行绑定操作）
     * 
     * @param viDevId VI 设备ID
     * @param viPipeId VI 管道ID
     * @param viChnId VI 通道ID
     * @param entityName 设备节点或entity名称
     */
    bool init(int viDevId, int viPipeId, int viChnId, const std::string& entityName);

    /**
     * @brief 反初始化（解绑，清理所有服务）
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
     * @brief 启动单个服务（支持独立控制）
     */
    void startEncoderService();
    void startOutputService();
    void startYUVService();

    /**
     * @brief 停止单个服务（支持独立控制）
     */
    void stopEncoderService();
    void stopOutputService();
    void stopYUVService();

    /**
     * @brief 获取服务实例
     */
    std::shared_ptr<VideoEncoderSvc> getEncoderService() { return m_encoderSvc; }
    std::shared_ptr<VideoOutputSvc> getOutputService() { return m_outputSvc; }
    std::shared_ptr<YUVOutputSvc> getYUVService() { return m_yuvSvc; }

private:
    /**
     * @brief 初始化 VI 模块
     */
    bool initializeVI();

    /**
     * @brief 清理 VI 模块
     */
    void cleanupVI();

    /**
     * @brief 初始化 VPSS 模块
     */
    bool initializeVPSS();

    /**
     * @brief 清理 VPSS 模块
     */
    void cleanupVPSS();

    /**
     * @brief 初始化 VENC 模块
     */
    bool initializeVENC();

    /**
     * @brief 清理 VENC 模块
     */
    void cleanupVENC();

    /**
     * @brief 初始化 VO 模块
     */
    bool initializeVO();

    /**
     * @brief 清理 VO 模块
     */
    void cleanupVO();

    /**
     * @brief 执行 MPP 绑定操作（根据服务状态动态绑定）
     * 
     * 绑定拓扑：
     * VI → VPSS → ┬→ VENC (编码)
     *              ├→ VO (显示)
     *              └→ VPSS_CHN (YUV输出)
     */
    bool setupBindings();

    /**
     * @brief 解绑所有连接
     */
    void teardownBindings();

    /**
     * @brief 增加服务引用计数（服务启动时调用）
     */
    void incrementServiceRef();

    /**
     * @brief 减少服务引用计数（服务停止时调用）
     * @return 返回当前引用计数
     */
    int decrementServiceRef();

    /**
     * @brief 根据服务状态动态绑定/解绑
     */
    void updateBindings();

    // VI 参数
    int m_viDevId;
    int m_viPipeId;
    int m_viChnId;
    std::string m_entityName;

    // VPSS 参数
    int m_vpssGrpId;
    int m_vpssChnEnc;   // 编码用的 VPSS 通道
    int m_vpssChnVo;    // 显示用的 VPSS 通道
    int m_vpssChnYuv;   // YUV输出用的 VPSS 通道

    // VENC 参数
    int m_vencChnId;

    // VO 参数
    int m_voDevId;
    int m_voLayerId;
    int m_voChnId;

    // 服务实例
    std::shared_ptr<VideoEncoderSvc> m_encoderSvc;
    std::shared_ptr<VideoOutputSvc> m_outputSvc;
    std::shared_ptr<YUVOutputSvc> m_yuvSvc;

    // 状态
    bool m_initialized = false;
    bool m_bindingsSetup = false;

    // VI/VPSS 初始化状态
    bool m_viInitialized = false;
    bool m_vpssInitialized = false;

    // 服务引用计数（用于管理VI生命周期）
    std::atomic<int> m_serviceRefCount{0};
    std::mutex m_refCountMutex;

    // 服务运行状态
    bool m_encoderRunning = false;
    bool m_outputRunning = false;
    bool m_yuvRunning = false;
};

#endif // MEDIA_MANAGER_H

