#include "YUVOutputSvc.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

// MPP 头文件
#include "rk_mpi_vpss.h"
#include "rk_mpi_mb.h"
#include "rk_comm_vpss.h"
#include "rk_common.h"

YUVOutputSvc::YUVOutputSvc()
    : ServiceBase("YUVOutputSvc") {
}

YUVOutputSvc::~YUVOutputSvc() {
    stop();
    join();
}

void YUVOutputSvc::setMPPParams(int vpssGrpId, int vpssChnId) {
    m_vpssGrpId = vpssGrpId;
    m_vpssChnId = vpssChnId;
    m_useBindingMode = (vpssGrpId >= 0 && vpssChnId >= 0);
    std::cout << "[" << m_name << "] Set MPP params: vpssGrpId=" << vpssGrpId 
              << ", vpssChnId=" << vpssChnId
              << ", bindingMode=" << m_useBindingMode << std::endl;
}

void YUVOutputSvc::setYUVCallback(YUVCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_callback = callback;
}

void YUVOutputSvc::run() {
    if (m_useBindingMode) {
        // 绑定模式：从 VPSS 循环获取 YUV 帧
        while (m_running.load()) {
            processTasks();
            
            if (!getYUVFrame()) {
                usleep(10 * 1000);  // 10ms
            }
        }
    } else {
        // 非绑定模式：需要手动输入帧（当前未实现）
        std::cerr << "[" << m_name << "] Non-binding mode not implemented" << std::endl;
    }
}

bool YUVOutputSvc::getYUVFrame() {
    VIDEO_FRAME_INFO_S stFrame;
    memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
    
    // 从 VPSS 获取帧
    RK_S32 s32Ret = RK_MPI_VPSS_GetChnFrame(m_vpssGrpId, m_vpssChnId, &stFrame, 100);  // 100ms 超时
    if (s32Ret != RK_SUCCESS) {
        if (s32Ret != RK_ERR_VPSS_BUF_EMPTY) {
            // 不是空缓冲区错误，记录日志
        }
        return false;
    }
    
    // 转换为 VideoFrame
    VideoFrame frame;
    frame.width = stFrame.stVFrame.u32Width;
    frame.height = stFrame.stVFrame.u32Height;
    frame.pixelFormat = stFrame.stVFrame.enPixelFormat;
    frame.timestamp = stFrame.stVFrame.u64PTS;
    
    // 获取数据指针（注意：ReleaseChnFrame 后数据会失效，需要拷贝或立即处理）
    MB_BLK mbBlk = stFrame.stVFrame.pMbBlk;
    if (mbBlk) {
        frame.data = static_cast<uint8_t*>(RK_MPI_MB_Handle2VirAddr(mbBlk));
        frame.size = RK_MPI_MB_GetSize(mbBlk);
    }
    
    // 处理帧（调用回调）
    processFrame(frame);
    
    // 释放帧（重要：必须释放）
    RK_MPI_VPSS_ReleaseChnFrame(m_vpssGrpId, m_vpssChnId, &stFrame);
    
    return true;
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

