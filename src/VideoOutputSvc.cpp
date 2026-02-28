#include "VideoOutputSvc.h"
#include <iostream>

// TODO: 这里需要根据实际的 VO 硬件接口（如 libdrm, Rockchip VO）来实现
// 当前提供一个框架实现

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

void VideoOutputSvc::inputFrame(const VideoFrame& frame) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_frameQueue.push(frame);
    }
    m_queueCv.notify_one();
}

void VideoOutputSvc::show() {
    post([this]() {
        m_visible = true;
        std::cout << "[" << m_name << "] Show display" << std::endl;
    });
}

void VideoOutputSvc::hide() {
    post([this]() {
        m_visible = false;
        std::cout << "[" << m_name << "] Hide display" << std::endl;
    });
}

void VideoOutputSvc::run() {
    // 初始化 VO
    if (!initVO()) {
        std::cerr << "[" << m_name << "] Failed to initialize VO" << std::endl;
        return;
    }

    while (m_running.load()) {
        // 处理任务队列
        processTasks();

        // 处理帧队列
        VideoFrame frame;
        bool hasFrame = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait_for(lock, std::chrono::milliseconds(33), [this] {
                return !m_frameQueue.empty() || !m_running.load();
            });

            if (!m_frameQueue.empty()) {
                frame = m_frameQueue.front();
                m_frameQueue.pop();
                hasFrame = true;
            }
        }

        if (hasFrame && m_visible) {
            displayFrame(frame);
        }
    }

    cleanupVO();
}

void VideoOutputSvc::displayFrame(const VideoFrame& frame) {
    // TODO: 实现实际的显示逻辑
    // 这里提供一个框架，实际需要：
    // 1. 格式转换（如果需要）
    // 2. 缩放（如果需要）
    // 3. 输出到 VO 硬件
    
    std::lock_guard<std::mutex> paramsLock(m_paramsMutex);
    
    std::cout << "[" << m_name << "] Displaying frame: "
              << frame.width << "x" << frame.height
              << ", timestamp: " << frame.timestamp << std::endl;

    // TODO: 实际显示操作
    // 例如：
    // - drmModeSetPlane() (使用 libdrm)
    // - 或 Rockchip VO API
    // - 或其他显示接口
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

