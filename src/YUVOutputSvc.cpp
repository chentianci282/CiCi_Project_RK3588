#include "YUVOutputSvc.h"
#include <iostream>

YUVOutputSvc::YUVOutputSvc()
    : ServiceBase("YUVOutputSvc") {
}

YUVOutputSvc::~YUVOutputSvc() {
    stop();
    join();
}

void YUVOutputSvc::setYUVCallback(YUVCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = callback;
}

void YUVOutputSvc::inputFrame(const VideoFrame& frame) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_frameQueue.push(frame);
    }
    m_queueCv.notify_one();
}

void YUVOutputSvc::run() {
    while (m_running.load()) {
        // 处理任务队列
        processTasks();

        // 处理帧队列
        VideoFrame frame;
        bool hasFrame = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !m_frameQueue.empty() || !m_running.load();
            });

            if (!m_frameQueue.empty()) {
                frame = m_frameQueue.front();
                m_frameQueue.pop();
                hasFrame = true;
            }
        }

        if (hasFrame) {
            processFrame(frame);
        }
    }
}

void YUVOutputSvc::processFrame(const VideoFrame& frame) {
    // 调用回调，将 YUV 数据传递给应用层（算法处理）
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    
    if (m_callback) {
        try {
            m_callback(frame);
        } catch (const std::exception& e) {
            std::cerr << "[" << m_name << "] Callback exception: " << e.what() << std::endl;
        }
    }
}

