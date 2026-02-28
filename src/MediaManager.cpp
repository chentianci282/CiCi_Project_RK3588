#include "MediaManager.h"
#include <iostream>

MediaManager::MediaManager() {
}

MediaManager::~MediaManager() {
    deinit();
}

bool MediaManager::init() {
    if (m_initialized) {
        std::cerr << "[MediaManager] Already initialized" << std::endl;
        return false;
    }

    // 创建服务实例
    m_encoderSvc = std::make_shared<VideoEncoderSvc>();
    m_outputSvc = std::make_shared<VideoOutputSvc>();
    m_yuvSvc = std::make_shared<YUVOutputSvc>();

    std::cout << "[MediaManager] Services created" << std::endl;

    m_initialized = true;
    return true;
}

void MediaManager::deinit() {
    if (!m_initialized) {
        return;
    }

    stop();

    // 清理服务
    m_encoderSvc.reset();
    m_outputSvc.reset();
    m_yuvSvc.reset();
    m_capture.reset();

    m_initialized = false;
    std::cout << "[MediaManager] Services destroyed" << std::endl;
}

void MediaManager::start() {
    if (!m_initialized) {
        std::cerr << "[MediaManager] Not initialized" << std::endl;
        return;
    }

    // 启动所有服务
    m_encoderSvc->start();
    m_outputSvc->start();
    m_yuvSvc->start();

    // 启动采集
    if (m_capture) {
        m_capture->start();
    }

    std::cout << "[MediaManager] All services started" << std::endl;
}

void MediaManager::stop() {
    // 停止采集
    if (m_capture) {
        m_capture->stop();
    }

    // 停止所有服务
    if (m_encoderSvc) {
        m_encoderSvc->stop();
        m_encoderSvc->join();
    }

    if (m_outputSvc) {
        m_outputSvc->stop();
        m_outputSvc->join();
    }

    if (m_yuvSvc) {
        m_yuvSvc->stop();
        m_yuvSvc->join();
    }

    std::cout << "[MediaManager] All services stopped" << std::endl;
}

void MediaManager::setCaptureSource(std::shared_ptr<CaptureThread> capture) {
    m_capture = capture;

    // 设置帧回调，将数据分发到三个服务
    if (m_capture) {
        m_capture->setFrameCallback([this](const VideoFrame& frame) {
            onFrameAvailable(frame);
        });
    }
    
    std::cout << "[MediaManager] Capture source set" << std::endl;
}

void MediaManager::onFrameAvailable(const VideoFrame& frame) {
    // 分发帧数据到三个服务
    if (m_encoderSvc) {
        m_encoderSvc->inputFrame(frame);
    }

    if (m_outputSvc) {
        m_outputSvc->inputFrame(frame);
    }

    if (m_yuvSvc) {
        m_yuvSvc->inputFrame(frame);
    }
}

