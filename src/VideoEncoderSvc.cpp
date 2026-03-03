#include "VideoEncoderSvc.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

// MPP 头文件
#include "rk_mpi_venc.h"
#include "rk_mpi_mb.h"
#include "rk_comm_venc.h"
#include "rk_common.h"

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

void VideoEncoderSvc::setMPPParams(int vencChnId) {
    m_vencChnId = vencChnId;
    m_useBindingMode = (vencChnId >= 0);
    std::cout << "[" << m_name << "] Set MPP params: vencChnId=" << vencChnId 
              << ", bindingMode=" << m_useBindingMode << std::endl;
}

void VideoEncoderSvc::run() {
    if (m_useBindingMode) {
        // 绑定模式：从 VENC 循环获取编码流
        while (m_running.load()) {
            processTasks();
            
            if (!getEncodedStream()) {
                usleep(10 * 1000);  // 10ms
            }
        }
    } else {
        // 非绑定模式：需要手动编码（当前未实现）
        std::cerr << "[" << m_name << "] Non-binding mode not implemented" << std::endl;
    }
}

bool VideoEncoderSvc::getEncodedStream() {
    VENC_STREAM_S stStream;
    memset(&stStream, 0, sizeof(VENC_STREAM_S));
    
    // 从 VENC 获取编码流
    RK_S32 s32Ret = RK_MPI_VENC_GetStream(m_vencChnId, &stStream, 100);  // 100ms 超时
    if (s32Ret != RK_SUCCESS) {
        if (s32Ret != RK_ERR_VENC_BUF_EMPTY) {
            // 不是空缓冲区错误，记录日志，方便排查
            std::cerr << "[VideoEncoderSvc] RK_MPI_VENC_GetStream failed: " << s32Ret
                      << " (chn=" << m_vencChnId << ")" << std::endl;
        }
        return false;
    }
    
    // 封装编码后的数据
    EncodedFrame encodedFrame;
    encodedFrame.size = stStream.pstPack->u32Len;
    encodedFrame.timestamp = stStream.pstPack->u64PTS;
    encodedFrame.isKeyFrame = (stStream.pstPack->DataType.enH264EType == H264E_NALU_ISLICE);
    
    // 获取数据指针
    RK_VOID* pData = RK_MPI_MB_Handle2VirAddr(stStream.pstPack->pMbBlk);
    if (pData) {
        // 分配内存并拷贝数据（因为 ReleaseStream 后数据会失效）
        encodedFrame.data = std::shared_ptr<uint8_t>(new uint8_t[encodedFrame.size], 
                                                      [](uint8_t* p) { delete[] p; });
        memcpy(encodedFrame.data.get(), pData, encodedFrame.size);
    }
    
    // 调用回调
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_callback && encodedFrame.size > 0) {
            m_callback(encodedFrame);
        }
    }
    
    // 释放流（重要：必须释放）
    RK_MPI_VENC_ReleaseStream(m_vencChnId, &stStream);
    
    return true;
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

