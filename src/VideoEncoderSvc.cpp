#include "VideoEncoderSvc.h"
#include <iostream>
#include <cstring>

// TODO: 这里需要根据实际的编码库（如 x264, x265, 或硬件编码器）来实现
// 当前提供一个框架实现

VideoEncoderSvc::VideoEncoderSvc()
    : ServiceBase("VideoEncoderSvc") {
}

VideoEncoderSvc::~VideoEncoderSvc() {
    stop();
    join();
    cleanupEncoder();
}

void VideoEncoderSvc::setEncodeParams(const EncodeParams& params) {
    std::lock_guard<std::mutex> lock(m_paramsMutex);
    m_params = params;
    
    // 如果编码器已初始化，需要重新初始化
    if (m_encoderInitialized) {
        cleanupEncoder();
        initEncoder();
    }
}

void VideoEncoderSvc::setEncodeCallback(EncodeCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = callback;
}

void VideoEncoderSvc::inputFrame(const VideoFrame& frame) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_frameQueue.push(frame);
    }
    m_queueCv.notify_one();
}

void VideoEncoderSvc::run() {
    // 初始化编码器
    if (!initEncoder()) {
        std::cerr << "[" << m_name << "] Failed to initialize encoder" << std::endl;
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
            encodeFrame(frame);
        }
    }

    cleanupEncoder();
}

void VideoEncoderSvc::encodeFrame(const VideoFrame& frame) {
    // TODO: 实现实际的编码逻辑
    // 这里提供一个框架，实际需要调用编码库（x264/x265/硬件编码器）
    
    std::lock_guard<std::mutex> paramsLock(m_paramsMutex);
    
    // 示例：这里应该调用编码器进行编码
    // 当前只是示例，实际需要：
    // 1. 将 YUV 数据送入编码器
    // 2. 获取编码后的数据
    // 3. 封装成 EncodedFrame
    // 4. 调用回调
    
    std::cout << "[" << m_name << "] Encoding frame: " 
              << frame.width << "x" << frame.height 
              << ", timestamp: " << frame.timestamp << std::endl;

    // 示例：创建一个空的编码帧（实际应该从编码器获取）
    EncodedFrame encodedFrame;
    encodedFrame.size = 0;  // TODO: 实际编码后的数据大小
    encodedFrame.timestamp = frame.timestamp;
    encodedFrame.isKeyFrame = false;  // TODO: 判断是否为关键帧
    encodedFrame.width = frame.width;
    encodedFrame.height = frame.height;
    
    // 分配内存（实际应该从编码器获取）
    // encodedFrame.data = std::shared_ptr<uint8_t>(new uint8_t[encodedFrame.size], 
    //                                               [](uint8_t* p) { delete[] p; });

    // 调用回调
    {
        std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
        if (m_callback && encodedFrame.size > 0) {
            m_callback(encodedFrame);
        }
    }
}

bool VideoEncoderSvc::initEncoder() {
    // TODO: 初始化编码器
    // 例如：
    // - x264_encoder_open()
    // - 或硬件编码器初始化
    // - 设置编码参数
    
    std::lock_guard<std::mutex> lock(m_paramsMutex);
    std::cout << "[" << m_name << "] Initializing encoder: "
              << m_params.width << "x" << m_params.height
              << ", bitrate: " << m_params.bitrate
              << ", fps: " << m_params.fps
              << ", format: " << (m_params.useH265 ? "H265" : "H264") << std::endl;

    m_encoderInitialized = true;
    return true;
}

void VideoEncoderSvc::cleanupEncoder() {
    if (!m_encoderInitialized) {
        return;
    }

    // TODO: 清理编码器资源
    // 例如：
    // - x264_encoder_close()
    // - 或硬件编码器清理
    
    std::cout << "[" << m_name << "] Cleaning up encoder" << std::endl;
    m_encoderInitialized = false;
}

