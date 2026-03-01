#include "VideoOutputSvc.h"
#include <iostream>
#include <unistd.h>

// MPP 头文件（绑定模式下可能不需要，因为数据自动流转）
#include "rk_mpi_vo.h"
#include "rk_comm_vo.h"
#include "rk_common.h"

VideoOutputSvc::VideoOutputSvc()
    : ServiceBase("VideoOutputSvc") {
}

VideoOutputSvc::~VideoOutputSvc() {
    stop();
    join();
    cleanupVO();
}

void VideoOutputSvc::setDisplayParams(const DisplayParams& params) {
    std::lock_guard<std::mutex> lock(m_paramsMutex);
    m_params = params;
    
    // 如果 VO 已初始化，需要重新配置
    if (m_voInitialized) {
        // TODO: 重新配置 VO
    }
}

void VideoOutputSvc::setMPPParams(int voDevId, int voLayerId, int voChnId) {
    m_voDevId = voDevId;
    m_voLayerId = voLayerId;
    m_voChnId = voChnId;
    m_useBindingMode = (voDevId >= 0 && voLayerId >= 0);
    std::cout << "[" << m_name << "] Set MPP params: voDevId=" << voDevId 
              << ", voLayerId=" << voLayerId 
              << ", voChnId=" << voChnId
              << ", bindingMode=" << m_useBindingMode << std::endl;
}

void VideoOutputSvc::show() {
    post([this]() {
        m_visible = true;
        std::cout << "[" << m_name << "] Show display" << std::endl;
        // 绑定模式下，数据自动显示，这里可以设置 VO 层可见性
    });
}

void VideoOutputSvc::hide() {
    post([this]() {
        m_visible = false;
        std::cout << "[" << m_name << "] Hide display" << std::endl;
        // 绑定模式下，可以禁用 VO 层
    });
}

void VideoOutputSvc::run() {
    if (m_useBindingMode) {
        // 绑定模式：数据自动从 VPSS 流转到 VO，不需要手动操作
        // 服务线程只需要处理任务队列（如 show/hide 控制）
        std::cout << "[" << m_name << "] Running in binding mode, data flows automatically" << std::endl;
        
        while (m_running.load()) {
            processTasks();
            usleep(100 * 1000);  // 100ms，主要是处理任务队列
        }
    } else {
        // 非绑定模式：需要手动发送帧（当前未实现）
        std::cerr << "[" << m_name << "] Non-binding mode not implemented" << std::endl;
    }
}

bool VideoOutputSvc::initVO() {
    // TODO: 初始化 VO 硬件
    // 例如：
    // - 打开 /dev/dri/card0
    // - 创建显示平面
    // - 配置显示参数
    
    std::lock_guard<std::mutex> lock(m_paramsMutex);
    std::cout << "[" << m_name << "] Initializing VO: "
              << m_params.displayWidth << "x" << m_params.displayHeight
              << ", position: (" << m_params.x << ", " << m_params.y << ")"
              << ", layer: " << m_params.layer << std::endl;

    m_voInitialized = true;
    return true;
}

void VideoOutputSvc::cleanupVO() {
    if (!m_voInitialized) {
        return;
    }

    // TODO: 清理 VO 资源
    // 例如：
    // - 关闭显示平面
    // - 关闭设备
    
    std::cout << "[" << m_name << "] Cleaning up VO" << std::endl;
    m_voInitialized = false;
}

