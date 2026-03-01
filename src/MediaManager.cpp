#include "MediaManager.h"
#include <iostream>
#include <cstring>

// MPP 头文件
#include "rk_mpi_vi.h"
#include "rk_mpi_vpss.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_vo.h"
#include "rk_mpi_sys.h"
#include "rk_comm_vi.h"
#include "rk_comm_vpss.h"
#include "rk_comm_venc.h"
#include "rk_comm_vo.h"
#include "rk_common.h"

MediaManager::MediaManager()
    : m_viDevId(0),
      m_viPipeId(0),
      m_viChnId(0),
      m_vpssGrpId(0),
      m_vpssChnEnc(0),
      m_vpssChnVo(1),
      m_vpssChnYuv(2),
      m_vencChnId(0),
      m_voDevId(0),
      m_voLayerId(0),
      m_voChnId(0) {
}

MediaManager::~MediaManager() {
    deinit();
}

bool MediaManager::init(int viDevId, int viPipeId, int viChnId, const std::string& entityName) {
    if (m_initialized) {
        std::cerr << "[MediaManager] Already initialized" << std::endl;
        return false;
    }

    // 保存 VI 参数
    m_viDevId = viDevId;
    m_viPipeId = viPipeId;
    m_viChnId = viChnId;
    m_entityName = entityName;

    // 创建服务实例（但不启动，等待单独启动）
    m_encoderSvc = std::make_shared<VideoEncoderSvc>();
    m_outputSvc = std::make_shared<VideoOutputSvc>();
    m_yuvSvc = std::make_shared<YUVOutputSvc>();

    // 设置服务的 MPP 参数（让服务知道从哪里获取数据）
    m_encoderSvc->setMPPParams(m_vencChnId);  // 从 VENC 获取编码流
    m_outputSvc->setMPPParams(m_voDevId, m_voLayerId, m_voChnId);  // 绑定到 VO，自动显示
    m_yuvSvc->setMPPParams(m_vpssGrpId, m_vpssChnYuv);  // 从 VPSS 获取 YUV 数据

    // 注意：不在这里初始化VI和绑定，等待第一个服务启动时再初始化

    std::cout << "[MediaManager] Services created (not started yet)" << std::endl;

    m_initialized = true;
    return true;
}

void MediaManager::deinit() {
    if (!m_initialized) {
        return;
    }

    // stop() 会自动处理引用计数和清理
    stop();

    // 清理服务实例
    m_encoderSvc.reset();
    m_outputSvc.reset();
    m_yuvSvc.reset();

    m_initialized = false;
    std::cout << "[MediaManager] Services destroyed" << std::endl;
}

void MediaManager::start() {
    if (!m_initialized) {
        std::cerr << "[MediaManager] Not initialized" << std::endl;
        return;
    }

    // 启动所有服务
    startEncoderService();
    startOutputService();
    startYUVService();

    std::cout << "[MediaManager] All services started" << std::endl;
}

void MediaManager::startEncoderService() {
    if (m_encoderRunning) {
        std::cout << "[MediaManager] Encoder service already running" << std::endl;
        return;
    }

    incrementServiceRef();
    m_encoderRunning = true;
    m_encoderSvc->start();
    std::cout << "[MediaManager] Encoder service started" << std::endl;
}

void MediaManager::startOutputService() {
    if (m_outputRunning) {
        std::cout << "[MediaManager] Output service already running" << std::endl;
        return;
    }

    incrementServiceRef();
    m_outputRunning = true;
    m_outputSvc->start();
    std::cout << "[MediaManager] Output service started" << std::endl;
}

void MediaManager::startYUVService() {
    if (m_yuvRunning) {
        std::cout << "[MediaManager] YUV service already running" << std::endl;
        return;
    }

    incrementServiceRef();
    m_yuvRunning = true;
    m_yuvSvc->start();
    std::cout << "[MediaManager] YUV service started" << std::endl;
}

void MediaManager::stopEncoderService() {
    if (!m_encoderRunning) {
        return;
    }

    m_encoderSvc->stop();
    m_encoderSvc->join();
    m_encoderRunning = false;
    decrementServiceRef();
    std::cout << "[MediaManager] Encoder service stopped" << std::endl;
}

void MediaManager::stopOutputService() {
    if (!m_outputRunning) {
        return;
    }

    m_outputSvc->stop();
    m_outputSvc->join();
    m_outputRunning = false;
    decrementServiceRef();
    std::cout << "[MediaManager] Output service stopped" << std::endl;
}

void MediaManager::stopYUVService() {
    if (!m_yuvRunning) {
        return;
    }

    m_yuvSvc->stop();
    m_yuvSvc->join();
    m_yuvRunning = false;
    decrementServiceRef();
    std::cout << "[MediaManager] YUV service stopped" << std::endl;
}

void MediaManager::stop() {
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

bool MediaManager::setupBindings() {
    RK_S32 s32Ret = RK_FAILURE;
    MPP_CHN_S stSrcChn, stDestChn;

    // ========== 1. 初始化 VPSS ==========
    VPSS_GRP_ATTR_S stGrpAttr;
    memset(&stGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
    stGrpAttr.u32MaxW = 4096;
    stGrpAttr.u32MaxH = 4096;
    stGrpAttr.enPixelFormat = RK_FMT_YUV420SP;
    stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stGrpAttr.enCompressMode = COMPRESS_MODE_NONE;

    s32Ret = RK_MPI_VPSS_CreateGrp(m_vpssGrpId, &stGrpAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to create VPSS group: " << s32Ret << std::endl;
        return false;
    }

    // 配置 VPSS 通道0（用于编码）
    VPSS_CHN_ATTR_S stChnAttr;
    memset(&stChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));
    stChnAttr.enChnMode = VPSS_CHN_MODE_PASSTHROUGH;
    stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
    stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    stChnAttr.stFrameRate.s32DstFrameRate = -1;
    stChnAttr.u32Width = 1920;
    stChnAttr.u32Height = 1080;
    stChnAttr.enCompressMode = COMPRESS_MODE_NONE;

    s32Ret = RK_MPI_VPSS_SetChnAttr(m_vpssGrpId, VPSS_CHN0, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VPSS channel 0: " << s32Ret << std::endl;
        return false;
    }
    RK_MPI_VPSS_EnableChn(m_vpssGrpId, VPSS_CHN0);

    // 配置 VPSS 通道1（用于显示）
    s32Ret = RK_MPI_VPSS_SetChnAttr(m_vpssGrpId, VPSS_CHN1, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VPSS channel 1: " << s32Ret << std::endl;
        return false;
    }
    RK_MPI_VPSS_EnableChn(m_vpssGrpId, VPSS_CHN1);

    // 配置 VPSS 通道2（用于 YUV 输出）
    s32Ret = RK_MPI_VPSS_SetChnAttr(m_vpssGrpId, VPSS_CHN2, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VPSS channel 2: " << s32Ret << std::endl;
        return false;
    }
    RK_MPI_VPSS_EnableChn(m_vpssGrpId, VPSS_CHN2);

    // 启动 VPSS 组
    s32Ret = RK_MPI_VPSS_StartGrp(m_vpssGrpId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to start VPSS group: " << s32Ret << std::endl;
        return false;
    }

    // ========== 2. 绑定 VI → VPSS ==========
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = m_viDevId;
    stSrcChn.s32ChnId = m_viChnId;

    stDestChn.enModId = RK_ID_VPSS;
    stDestChn.s32DevId = m_vpssGrpId;
    stDestChn.s32ChnId = 0;  // VPSS 组输入

    s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to bind VI to VPSS: " << s32Ret << std::endl;
        return false;
    }
    std::cout << "[MediaManager] VI → VPSS bound" << std::endl;

    // ========== 3. 初始化 VENC ==========
    VENC_CHN_ATTR_S stVencAttr;
    memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
    stVencAttr.stVencAttr.enType = RK_VIDEO_ID_AVC;  // H264
    stVencAttr.stVencAttr.u32PicWidth = 1920;
    stVencAttr.stVencAttr.u32PicHeight = 1080;
    stVencAttr.stVencAttr.u32VirWidth = 1920;
    stVencAttr.stVencAttr.u32VirHeight = 1080;
    stVencAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
    stVencAttr.stVencAttr.enType = RK_VIDEO_ID_AVC;
    stVencAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
    stVencAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    stVencAttr.stRcAttr.stH264Cbr.u32Gop = 30;
    stVencAttr.stRcAttr.stH264Cbr.u32BitRate = 2000;  // 2Mbps (单位是kbps)
    stVencAttr.stRcAttr.stH264Cbr.u32StatTime = 1;
    stVencAttr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    stVencAttr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
    stVencAttr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
    stVencAttr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;

    s32Ret = RK_MPI_VENC_CreateChn(m_vencChnId, &stVencAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to create VENC channel: " << s32Ret << std::endl;
        return false;
    }

    // ========== 4. 绑定 VPSS_CHN0 → VENC ==========
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN0;

    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = m_vencChnId;

    s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to bind VPSS to VENC: " << s32Ret << std::endl;
        return false;
    }
    std::cout << "[MediaManager] VPSS_CHN0 → VENC bound" << std::endl;

    // ========== 5. 初始化 VO ==========
    VO_PUB_ATTR_S stPubAttr;
    memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
    stPubAttr.enIntfType = VO_INTF_HDMI;
    stPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    s32Ret = RK_MPI_VO_SetPubAttr(m_voDevId, &stPubAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VO pub attr: " << s32Ret << std::endl;
        return false;
    }

    s32Ret = RK_MPI_VO_Enable(m_voDevId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to enable VO: " << s32Ret << std::endl;
        return false;
    }

    s32Ret = RK_MPI_VO_BindLayer(m_voLayerId, m_voDevId, VO_LAYER_MODE_VIDEO);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to bind VO layer: " << s32Ret << std::endl;
        return false;
    }

    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    memset(&stLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.stDispRect.u32Width = 1920;
    stLayerAttr.stDispRect.u32Height = 1080;
    stLayerAttr.enPixFormat = RK_FMT_YUV420SP;
    s32Ret = RK_MPI_VO_SetLayerAttr(m_voLayerId, &stLayerAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VO layer attr: " << s32Ret << std::endl;
        return false;
    }

    s32Ret = RK_MPI_VO_EnableLayer(m_voLayerId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to enable VO layer: " << s32Ret << std::endl;
        return false;
    }

    // ========== 6. 绑定 VPSS_CHN1 → VO ==========
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN1;

    stDestChn.enModId = RK_ID_VO;
    stDestChn.s32DevId = m_voLayerId;
    stDestChn.s32ChnId = m_voChnId;

    s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to bind VPSS to VO: " << s32Ret << std::endl;
        return false;
    }
    std::cout << "[MediaManager] VPSS_CHN1 → VO bound" << std::endl;

    s32Ret = RK_MPI_VO_EnableChn(m_voLayerId, m_voChnId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to enable VO channel: " << s32Ret << std::endl;
        return false;
    }

    m_bindingsSetup = true;
    return true;
}

void MediaManager::teardownBindings() {
    if (!m_bindingsSetup) {
        return;
    }

    MPP_CHN_S stSrcChn, stDestChn;

    // 解绑 VPSS → VO
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN1;
    stDestChn.enModId = RK_ID_VO;
    stDestChn.s32DevId = m_voLayerId;
    stDestChn.s32ChnId = m_voChnId;
    RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    RK_MPI_VO_DisableChn(m_voLayerId, m_voChnId);
    RK_MPI_VO_DisableLayer(m_voLayerId);
    RK_MPI_VO_Disable(m_voDevId);

    // 解绑 VPSS → VENC
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN0;
    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = m_vencChnId;
    RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    RK_MPI_VENC_DestroyChn(m_vencChnId);

    // 解绑 VI → VPSS
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = m_viDevId;
    stSrcChn.s32ChnId = m_viChnId;
    stDestChn.enModId = RK_ID_VPSS;
    stDestChn.s32DevId = m_vpssGrpId;
    stDestChn.s32ChnId = 0;
    RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);

    // 停止并销毁 VPSS
    RK_MPI_VPSS_StopGrp(m_vpssGrpId);
    RK_MPI_VPSS_DestroyGrp(m_vpssGrpId);

    m_bindingsSetup = false;
    std::cout << "[MediaManager] All bindings teardown" << std::endl;
}

void MediaManager::incrementServiceRef() {
    std::lock_guard<std::mutex> lock(m_refCountMutex);
    int oldCount = m_serviceRefCount.fetch_add(1);
    
    std::cout << "[MediaManager] Service ref count: " << oldCount << " -> " << (oldCount + 1) << std::endl;
    
    // 如果是第一个服务启动，初始化VI和VPSS
    if (oldCount == 0) {
        if (!initializeVI()) {
            std::cerr << "[MediaManager] Failed to initialize VI" << std::endl;
            m_serviceRefCount.fetch_sub(1);  // 回滚
            return;
        }
        
        if (!initializeVPSS()) {
            std::cerr << "[MediaManager] Failed to initialize VPSS" << std::endl;
            cleanupVI();
            m_serviceRefCount.fetch_sub(1);  // 回滚
            return;
        }
        
        // 绑定 VI → VPSS
        MPP_CHN_S stSrcChn, stDestChn;
        stSrcChn.enModId = RK_ID_VI;
        stSrcChn.s32DevId = m_viDevId;
        stSrcChn.s32ChnId = m_viChnId;
        
        stDestChn.enModId = RK_ID_VPSS;
        stDestChn.s32DevId = m_vpssGrpId;
        stDestChn.s32ChnId = 0;  // VPSS 组输入
        
        RK_S32 s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        if (s32Ret != RK_SUCCESS) {
            std::cerr << "[MediaManager] Failed to bind VI to VPSS: " << s32Ret << std::endl;
            cleanupVPSS();
            cleanupVI();
            m_serviceRefCount.fetch_sub(1);  // 回滚
            return;
        }
        
        m_bindingsSetup = true;
        std::cout << "[MediaManager] VI → VPSS bound" << std::endl;
    }
    
    // 根据当前服务状态更新绑定
    updateBindings();
}

int MediaManager::decrementServiceRef() {
    std::lock_guard<std::mutex> lock(m_refCountMutex);
    int newCount = m_serviceRefCount.fetch_sub(1) - 1;
    
    std::cout << "[MediaManager] Service ref count: " << (newCount + 1) << " -> " << newCount << std::endl;
    
    // 如果是最后一个服务停止，清理VI和VPSS
    if (newCount == 0) {
        // 解绑所有连接
        teardownBindings();
        
        // 清理VPSS和VI
        cleanupVPSS();
        cleanupVI();
        
        m_bindingsSetup = false;
        std::cout << "[MediaManager] All services stopped, VI and VPSS cleaned up" << std::endl;
    } else {
        // 更新绑定（解绑不需要的服务）
        updateBindings();
    }
    
    return newCount;
}

void MediaManager::updateBindings() {
    // 根据服务运行状态动态绑定/解绑
    MPP_CHN_S stSrcChn, stDestChn;
    RK_S32 s32Ret = RK_FAILURE;
    
    // 编码服务绑定
    if (m_encoderRunning && !m_bindingsSetup) {
        // 确保VENC已初始化
        if (!initializeVENC()) {
            std::cerr << "[MediaManager] Failed to initialize VENC" << std::endl;
            return;
        }
        
        // 绑定 VPSS_CHN0 → VENC
        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = m_vpssGrpId;
        stSrcChn.s32ChnId = VPSS_CHN0;
        
        stDestChn.enModId = RK_ID_VENC;
        stDestChn.s32DevId = 0;
        stDestChn.s32ChnId = m_vencChnId;
        
        s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        if (s32Ret == RK_SUCCESS) {
            std::cout << "[MediaManager] VPSS_CHN0 → VENC bound" << std::endl;
        }
    } else if (!m_encoderRunning && m_bindingsSetup) {
        // 解绑 VPSS → VENC
        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = m_vpssGrpId;
        stSrcChn.s32ChnId = VPSS_CHN0;
        
        stDestChn.enModId = RK_ID_VENC;
        stDestChn.s32DevId = 0;
        stDestChn.s32ChnId = m_vencChnId;
        
        RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        cleanupVENC();
        std::cout << "[MediaManager] VPSS_CHN0 → VENC unbound" << std::endl;
    }
    
    // 显示服务绑定
    if (m_outputRunning && !m_bindingsSetup) {
        // 确保VO已初始化
        if (!initializeVO()) {
            std::cerr << "[MediaManager] Failed to initialize VO" << std::endl;
            return;
        }
        
        // 绑定 VPSS_CHN1 → VO
        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = m_vpssGrpId;
        stSrcChn.s32ChnId = VPSS_CHN1;
        
        stDestChn.enModId = RK_ID_VO;
        stDestChn.s32DevId = m_voLayerId;
        stDestChn.s32ChnId = m_voChnId;
        
        s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        if (s32Ret == RK_SUCCESS) {
            RK_MPI_VO_EnableChn(m_voLayerId, m_voChnId);
            std::cout << "[MediaManager] VPSS_CHN1 → VO bound" << std::endl;
        }
    } else if (!m_outputRunning && m_bindingsSetup) {
        // 解绑 VPSS → VO
        stSrcChn.enModId = RK_ID_VPSS;
        stSrcChn.s32DevId = m_vpssGrpId;
        stSrcChn.s32ChnId = VPSS_CHN1;
        
        stDestChn.enModId = RK_ID_VO;
        stDestChn.s32DevId = m_voLayerId;
        stDestChn.s32ChnId = m_voChnId;
        
        RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        RK_MPI_VO_DisableChn(m_voLayerId, m_voChnId);
        cleanupVO();
        std::cout << "[MediaManager] VPSS_CHN1 → VO unbound" << std::endl;
    }
    
    // YUV服务不需要额外绑定，直接从VPSS_CHN2获取数据
}

bool MediaManager::initializeVI() {
    if (m_viInitialized) {
        return true;
    }
    
    RK_S32 s32Ret = RK_FAILURE;
    
    // 1. 配置 VI 设备属性
    VI_DEV_ATTR_S stDevAttr;
    memset(&stDevAttr, 0, sizeof(VI_DEV_ATTR_S));
    
    stDevAttr.enIntfMode = VI_MODE_MIPI;
    stDevAttr.enWorkMode = VI_WORK_MODE_1Multiplex;
    stDevAttr.enDataSeq = VI_DATA_SEQ_UYVY;
    stDevAttr.enInputDataType = VI_DATA_TYPE_YUV;
    stDevAttr.stMaxSize.u32Width = 3840;
    stDevAttr.stMaxSize.u32Height = 2160;
    stDevAttr.enDataRate = DATA_RATE_X1;
    stDevAttr.enPixFmt = RK_FMT_YUV420SP;
    stDevAttr.enMemMode = VI_RAW_MEM_COMPACT;
    stDevAttr.enBufType = VI_V4L2_MEMORY_TYPE_DMABUF;
    stDevAttr.u32BufCount = 3;
    stDevAttr.enHdrMode = VI_MODE_NORMAL;
    
    s32Ret = RK_MPI_VI_GetDevAttr(m_viDevId, &stDevAttr);
    if (s32Ret == RK_ERR_VI_NOT_CONFIG) {
        s32Ret = RK_MPI_VI_SetDevAttr(m_viDevId, &stDevAttr);
        if (s32Ret != RK_SUCCESS) {
            std::cerr << "[MediaManager] RK_MPI_VI_SetDevAttr failed: " << s32Ret << std::endl;
            return false;
        }
    }
    
    // 2. 启用设备
    s32Ret = RK_MPI_VI_GetDevIsEnable(m_viDevId);
    if (s32Ret != RK_SUCCESS) {
        s32Ret = RK_MPI_VI_EnableDev(m_viDevId);
        if (s32Ret != RK_SUCCESS) {
            std::cerr << "[MediaManager] RK_MPI_VI_EnableDev failed: " << s32Ret << std::endl;
            return false;
        }
        
        // 3. 绑定设备到管道
        VI_DEV_BIND_PIPE_S stBindPipe;
        memset(&stBindPipe, 0, sizeof(VI_DEV_BIND_PIPE_S));
        stBindPipe.u32Num = 1;
        stBindPipe.PipeId[0] = m_viPipeId;
        stBindPipe.bDataOffline = RK_FALSE;
        stBindPipe.bUserStartPipe[0] = RK_FALSE;
        
        s32Ret = RK_MPI_VI_SetDevBindPipe(m_viDevId, &stBindPipe);
        if (s32Ret != RK_SUCCESS) {
            std::cerr << "[MediaManager] RK_MPI_VI_SetDevBindPipe failed: " << s32Ret << std::endl;
            return false;
        }
    }
    
    // 4. 创建管道
    VI_PIPE_ATTR_S stPipeAttr;
    memset(&stPipeAttr, 0, sizeof(VI_PIPE_ATTR_S));
    stPipeAttr.bIspBypass = RK_FALSE;
    stPipeAttr.u32MaxW = 3840;
    stPipeAttr.u32MaxH = 2160;
    stPipeAttr.enPixFmt = RK_FMT_YUV420SP;
    stPipeAttr.enCompressMode = COMPRESS_MODE_NONE;
    stPipeAttr.enBitWidth = DATA_BITWIDTH_8;
    stPipeAttr.stFrameRate.s32SrcFrameRate = -1;
    stPipeAttr.stFrameRate.s32DstFrameRate = -1;
    stPipeAttr.enMemMode = VI_RAW_MEM_COMPACT;
    stPipeAttr.enHdrMode = VI_MODE_NORMAL;
    
    s32Ret = RK_MPI_VI_CreatePipe(m_viPipeId, &stPipeAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] RK_MPI_VI_CreatePipe failed: " << s32Ret << std::endl;
        return false;
    }
    
    // 5. 配置通道属性
    VI_CHN_ATTR_S stChnAttr;
    memset(&stChnAttr, 0, sizeof(VI_CHN_ATTR_S));
    stChnAttr.stSize.u32Width = 3840;
    stChnAttr.stSize.u32Height = 2160;
    stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
    stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stChnAttr.enVideoFormat = VIDEO_FORMAT_LINEAR;
    stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
    stChnAttr.bMirror = RK_FALSE;
    stChnAttr.bFlip = RK_FALSE;
    stChnAttr.u32Depth = 3;
    stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    stChnAttr.stFrameRate.s32DstFrameRate = -1;
    stChnAttr.enAllocBufType = VI_ALLOC_BUF_TYPE_INTERNAL;
    
    // 设置 entity 名称
    if (!m_entityName.empty()) {
        strncpy(stChnAttr.stIspOpt.aEntityName, m_entityName.c_str(), MAX_VI_ENTITY_NAME_LEN - 1);
        stChnAttr.stIspOpt.aEntityName[MAX_VI_ENTITY_NAME_LEN - 1] = '\0';
    }
    stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE_MPLANE;
    stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_FALSE;
    stChnAttr.stIspOpt.stMaxSize.u32Width = 3840;
    stChnAttr.stIspOpt.stMaxSize.u32Height = 2160;
    
    s32Ret = RK_MPI_VI_SetChnAttr(m_viPipeId, m_viChnId, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] RK_MPI_VI_SetChnAttr failed: " << s32Ret << std::endl;
        return false;
    }
    
    // 6. 启用通道
    s32Ret = RK_MPI_VI_EnableChn(m_viPipeId, m_viChnId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] RK_MPI_VI_EnableChn failed: " << s32Ret << std::endl;
        return false;
    }
    
    // 7. 启动管道
    s32Ret = RK_MPI_VI_StartPipe(m_viPipeId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] RK_MPI_VI_StartPipe failed: " << s32Ret << std::endl;
        return false;
    }
    
    m_viInitialized = true;
    std::cout << "[MediaManager] VI initialized" << std::endl;
    return true;
}

void MediaManager::cleanupVI() {
    if (!m_viInitialized) {
        return;
    }
    
    RK_MPI_VI_StopPipe(m_viPipeId);
    RK_MPI_VI_DisableChn(m_viPipeId, m_viChnId);
    RK_MPI_VI_DestroyPipe(m_viPipeId);
    RK_MPI_VI_DisableDev(m_viDevId);
    
    m_viInitialized = false;
    std::cout << "[MediaManager] VI cleaned up" << std::endl;
}

bool MediaManager::initializeVPSS() {
    if (m_vpssInitialized) {
        return true;
    }
    
    RK_S32 s32Ret = RK_FAILURE;

    // ========== 初始化 VPSS ==========
    VPSS_GRP_ATTR_S stGrpAttr;
    memset(&stGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
    stGrpAttr.u32MaxW = 4096;
    stGrpAttr.u32MaxH = 4096;
    stGrpAttr.enPixelFormat = RK_FMT_YUV420SP;
    stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stGrpAttr.enCompressMode = COMPRESS_MODE_NONE;

    s32Ret = RK_MPI_VPSS_CreateGrp(m_vpssGrpId, &stGrpAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to create VPSS group: " << s32Ret << std::endl;
        return false;
    }

    // 配置 VPSS 通道0（用于编码）
    VPSS_CHN_ATTR_S stChnAttr;
    memset(&stChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));
    stChnAttr.enChnMode = VPSS_CHN_MODE_PASSTHROUGH;
    stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
    stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    stChnAttr.stFrameRate.s32DstFrameRate = -1;
    stChnAttr.u32Width = 3840;
    stChnAttr.u32Height = 2160;
    stChnAttr.enCompressMode = COMPRESS_MODE_NONE;

    s32Ret = RK_MPI_VPSS_SetChnAttr(m_vpssGrpId, VPSS_CHN0, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VPSS channel 0: " << s32Ret << std::endl;
        return false;
    }
    RK_MPI_VPSS_EnableChn(m_vpssGrpId, VPSS_CHN0);

    // 配置 VPSS 通道1（用于显示）
    s32Ret = RK_MPI_VPSS_SetChnAttr(m_vpssGrpId, VPSS_CHN1, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VPSS channel 1: " << s32Ret << std::endl;
        return false;
    }
    RK_MPI_VPSS_EnableChn(m_vpssGrpId, VPSS_CHN1);

    // 配置 VPSS 通道2（用于 YUV 输出）
    s32Ret = RK_MPI_VPSS_SetChnAttr(m_vpssGrpId, VPSS_CHN2, &stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VPSS channel 2: " << s32Ret << std::endl;
        return false;
    }
    RK_MPI_VPSS_EnableChn(m_vpssGrpId, VPSS_CHN2);

    // 启动 VPSS 组
    s32Ret = RK_MPI_VPSS_StartGrp(m_vpssGrpId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to start VPSS group: " << s32Ret << std::endl;
        return false;
    }

    m_vpssInitialized = true;
    std::cout << "[MediaManager] VPSS initialized" << std::endl;
    return true;
}

void MediaManager::cleanupVPSS() {
    if (!m_vpssInitialized) {
        return;
    }
    
    RK_MPI_VPSS_StopGrp(m_vpssGrpId);
    RK_MPI_VPSS_DestroyGrp(m_vpssGrpId);
    
    m_vpssInitialized = false;
    std::cout << "[MediaManager] VPSS cleaned up" << std::endl;
}

bool MediaManager::initializeVENC() {
    RK_S32 s32Ret = RK_FAILURE;
    
    VENC_CHN_ATTR_S stVencAttr;
    memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
    stVencAttr.stVencAttr.enType = RK_VIDEO_ID_AVC;  // H264
    stVencAttr.stVencAttr.u32PicWidth = 3840;
    stVencAttr.stVencAttr.u32PicHeight = 2160;
    stVencAttr.stVencAttr.u32VirWidth = 3840;
    stVencAttr.stVencAttr.u32VirHeight = 2160;
    stVencAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
    stVencAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
    stVencAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    stVencAttr.stRcAttr.stH264Cbr.u32Gop = 30;
    stVencAttr.stRcAttr.stH264Cbr.u32BitRate = 2000;  // 2Mbps (单位是kbps)
    stVencAttr.stRcAttr.stH264Cbr.u32StatTime = 1;
    stVencAttr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    stVencAttr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
    stVencAttr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
    stVencAttr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;

    s32Ret = RK_MPI_VENC_CreateChn(m_vencChnId, &stVencAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to create VENC channel: " << s32Ret << std::endl;
        return false;
    }
    
    std::cout << "[MediaManager] VENC initialized" << std::endl;
    return true;
}

void MediaManager::cleanupVENC() {
    RK_MPI_VENC_DestroyChn(m_vencChnId);
    std::cout << "[MediaManager] VENC cleaned up" << std::endl;
}

bool MediaManager::initializeVO() {
    RK_S32 s32Ret = RK_FAILURE;
    
    VO_PUB_ATTR_S stPubAttr;
    memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
    stPubAttr.enIntfType = VO_INTF_HDMI;
    stPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    s32Ret = RK_MPI_VO_SetPubAttr(m_voDevId, &stPubAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VO pub attr: " << s32Ret << std::endl;
        return false;
    }

    s32Ret = RK_MPI_VO_Enable(m_voDevId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to enable VO: " << s32Ret << std::endl;
        return false;
    }

    s32Ret = RK_MPI_VO_BindLayer(m_voLayerId, m_voDevId, VO_LAYER_MODE_VIDEO);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to bind VO layer: " << s32Ret << std::endl;
        return false;
    }

    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    memset(&stLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.stDispRect.u32Width = 1920;
    stLayerAttr.stDispRect.u32Height = 1080;
    stLayerAttr.enPixFormat = RK_FMT_YUV420SP;
    s32Ret = RK_MPI_VO_SetLayerAttr(m_voLayerId, &stLayerAttr);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to set VO layer attr: " << s32Ret << std::endl;
        return false;
    }

    s32Ret = RK_MPI_VO_EnableLayer(m_voLayerId);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to enable VO layer: " << s32Ret << std::endl;
        return false;
    }
    
    std::cout << "[MediaManager] VO initialized" << std::endl;
    return true;
}

void MediaManager::cleanupVO() {
    RK_MPI_VO_DisableLayer(m_voLayerId);
    RK_MPI_VO_Disable(m_voDevId);
    std::cout << "[MediaManager] VO cleaned up" << std::endl;
}

