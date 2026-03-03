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

    // 第一个服务启动时，会在 incrementServiceRef 中初始化 VI/VPSS 并完成 VI→VPSS 绑定
    incrementServiceRef();

    // 如果 VI/VPSS 初始化失败，则不继续启动编码服务
    if (!m_viInitialized || !m_vpssInitialized) {
        std::cerr << "[MediaManager] startEncoderService: VI/VPSS not initialized, abort" << std::endl;
        return;
    }

    // 初始化 VENC 并绑定 VPSS_CHN0 → VENC
    std::cout << "[MediaManager] startEncoderService: initializeVENC() begin" << std::endl;
    if (!initializeVENC()) {
        std::cerr << "[MediaManager] startEncoderService: initializeVENC() failed" << std::endl;
        decrementServiceRef();
        return;
    }
    std::cout << "[MediaManager] startEncoderService: initializeVENC() ok" << std::endl;

    MPP_CHN_S stSrcChn, stDestChn;
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN0;

    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = m_vencChnId;

    RK_S32 s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    std::cout << "[MediaManager] startEncoderService: Bind VPSS_CHN0 -> VENC ret=" << s32Ret << std::endl;
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to bind VPSS_CHN0 to VENC: " << s32Ret << std::endl;
        cleanupVENC();
        decrementServiceRef();
        return;
    }
    std::cout << "[MediaManager] VPSS_CHN0 → VENC bound (encoder service)" << std::endl;
    m_encoderRunning = true;
    m_encoderSvc->start();
    std::cout << "[MediaManager] Encoder service started" << std::endl;
}

void MediaManager::startOutputService() {
    if (m_outputRunning) {
        std::cout << "[MediaManager] Output service already running" << std::endl;
        return;
    }

    // 引用计数 +1（可能不是第一个服务）
    incrementServiceRef();

    // 如果 VI/VPSS 初始化失败，则不继续启动显示服务
    if (!m_viInitialized || !m_vpssInitialized) {
        std::cerr << "[MediaManager] startOutputService: VI/VPSS not initialized, abort" << std::endl;
        return;
    }

    // 初始化 VO 并绑定 VPSS_CHN1 → VO
    std::cout << "[MediaManager] startOutputService: initializeVO() begin" << std::endl;
    if (!initializeVO()) {
        std::cerr << "[MediaManager] startOutputService: initializeVO() failed" << std::endl;
        decrementServiceRef();
        return;
    }
    std::cout << "[MediaManager] startOutputService: initializeVO() ok" << std::endl;

    MPP_CHN_S stSrcChn, stDestChn;
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN1;

    stDestChn.enModId = RK_ID_VO;
    stDestChn.s32DevId = m_voLayerId;
    stDestChn.s32ChnId = m_voChnId;

    RK_S32 s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    std::cout << "[MediaManager] startOutputService: Bind VPSS_CHN1 -> VO ret=" << s32Ret << std::endl;
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to bind VPSS_CHN1 to VO: " << s32Ret << std::endl;
        cleanupVO();
        decrementServiceRef();
        return;
    }
    // 启用 VO 通道
    s32Ret = RK_MPI_VO_EnableChn(m_voLayerId, m_voChnId);
    std::cout << "[MediaManager] startOutputService: Enable VO chn ret=" << s32Ret << std::endl;
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] Failed to enable VO channel: " << s32Ret << std::endl;
        RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
        cleanupVO();
        decrementServiceRef();
        return;
    }
    std::cout << "[MediaManager] VPSS_CHN1 → VO bound (output service)" << std::endl;
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

    // 先停止服务线程
    m_encoderSvc->stop();
    m_encoderSvc->join();
    m_encoderRunning = false;

    // 解绑 VPSS_CHN0 → VENC 并清理 VENC
    MPP_CHN_S stSrcChn, stDestChn;
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN0;

    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = m_vencChnId;

    RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    cleanupVENC();

    decrementServiceRef();
    std::cout << "[MediaManager] Encoder service stopped" << std::endl;
}

void MediaManager::stopOutputService() {
    if (!m_outputRunning) {
        return;
    }

    // 先停止服务线程
    m_outputSvc->stop();
    m_outputSvc->join();
    m_outputRunning = false;

    // 解绑 VPSS_CHN1 → VO 并清理 VO
    MPP_CHN_S stSrcChn, stDestChn;
    stSrcChn.enModId = RK_ID_VPSS;
    stSrcChn.s32DevId = m_vpssGrpId;
    stSrcChn.s32ChnId = VPSS_CHN1;

    stDestChn.enModId = RK_ID_VO;
    stDestChn.s32DevId = m_voLayerId;
    stDestChn.s32ChnId = m_voChnId;

    RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    RK_MPI_VO_DisableChn(m_voLayerId, m_voChnId);
    cleanupVO();

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

    std::cout << "[MediaManager] Service ref count: "
              << oldCount << " -> " << (oldCount + 1) << std::endl;

    // 如果是第一个服务启动，初始化VI和VPSS，并完成 VI→VPSS 绑定
    if (oldCount == 0) {
        std::cout << "[MediaManager] incrementServiceRef: first service, initialize VI/VPSS" << std::endl;

        std::cout << "[MediaManager] initializeVI() begin" << std::endl;
        if (!initializeVI()) {
            std::cerr << "[MediaManager] initializeVI() failed" << std::endl;
            m_serviceRefCount.fetch_sub(1);  // 回滚
            return;
        }
        std::cout << "[MediaManager] initializeVI() ok" << std::endl;

        std::cout << "[MediaManager] initializeVPSS() begin" << std::endl;
        if (!initializeVPSS()) {
            std::cerr << "[MediaManager] initializeVPSS() failed" << std::endl;
            cleanupVI();
            m_serviceRefCount.fetch_sub(1);  // 回滚
            return;
        }
        std::cout << "[MediaManager] initializeVPSS() ok" << std::endl;

        std::cout << "[MediaManager] bind VI->VPSS begin" << std::endl;
        MPP_CHN_S stSrcChn, stDestChn;
        stSrcChn.enModId = RK_ID_VI;
        stSrcChn.s32DevId = m_viDevId;
        stSrcChn.s32ChnId = m_viChnId;

        stDestChn.enModId = RK_ID_VPSS;
        stDestChn.s32DevId = m_vpssGrpId;
        stDestChn.s32ChnId = 0;  // VPSS 组输入

        RK_S32 s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        std::cout << "[MediaManager] RK_MPI_SYS_Bind(VI->VPSS) ret=" << s32Ret << std::endl;
        if (s32Ret != RK_SUCCESS) {
            std::cerr << "[MediaManager] Failed to bind VI to VPSS: " << s32Ret << std::endl;
            cleanupVPSS();
            cleanupVI();
            m_serviceRefCount.fetch_sub(1);  // 回滚
            return;
        }

        m_bindingsSetup = true;
        std::cout << "[MediaManager] VI → VPSS bound (first service path done)" << std::endl;
    }

    // 当前版本中，VENC/VO 的绑定在各自的 startXxxService 中完成
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
    std::cout << "[MediaManager] initializeVI: start, viDev=" << m_viDevId
              << " pipe=" << m_viPipeId
              << " chn=" << m_viChnId
              << " entity=" << m_entityName << std::endl;
    
    // 1. 获取/检查 VI 设备属性（参考 test_mpi_vi：如未配置则用当前结构进行一次 SetDevAttr）
    VI_DEV_ATTR_S stDevAttr;
    memset(&stDevAttr, 0, sizeof(VI_DEV_ATTR_S));

    s32Ret = RK_MPI_VI_GetDevAttr(m_viDevId, &stDevAttr);
    if (s32Ret == RK_ERR_VI_NOT_CONFIG) {
        // 与 test_mpi_vi 一致：如果未配置，则使用当前 stDevAttr 调用一次 SetDevAttr
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
    
    // 4. 配置通道属性（参考 test_mpi_vi：只设置必要字段，避免与 ISP 默认配置冲突）
    VI_CHN_ATTR_S stChnAttr;
    memset(&stChnAttr, 0, sizeof(VI_CHN_ATTR_S));
    stChnAttr.stSize.u32Width  = 3840;
    stChnAttr.stSize.u32Height = 2160;
    stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
    stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stChnAttr.enVideoFormat = VIDEO_FORMAT_LINEAR;
    stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
    stChnAttr.bMirror = RK_FALSE;
    stChnAttr.bFlip = RK_FALSE;
    // 对齐 test_mpi_vi 在绑定 VENC 模式下的配置：u32Depth = 0，由绑定模块控制缓冲
    stChnAttr.u32Depth = 0;
    stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    stChnAttr.stFrameRate.s32DstFrameRate = -1;
    stChnAttr.enAllocBufType = VI_ALLOC_BUF_TYPE_INTERNAL;

    // 设置 entity 名称（与 test_mpi_vi 一致）
    if (!m_entityName.empty()) {
        strncpy(stChnAttr.stIspOpt.aEntityName, m_entityName.c_str(), MAX_VI_ENTITY_NAME_LEN - 1);
        stChnAttr.stIspOpt.aEntityName[MAX_VI_ENTITY_NAME_LEN - 1] = '\0';
    }
    // 与 test_mpi_vi 参数对应：
    // - enMemoryType 由 -t 4 指定为 DMABUF
    // - enCaptureType 保持为 VIDEO_CAPTURE（单平面），不要强行使用 MPLANE
    stChnAttr.stIspOpt.enMemoryType  = VI_V4L2_MEMORY_TYPE_DMABUF;
    stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;
    // 与 demo 命令 --buf_count 6 对应
    stChnAttr.stIspOpt.u32BufCount   = 6;
    stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_FALSE;
    
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
    // 说明：在 rockit 官方 demo（test_mpi_vi）中，并未显式调用 RK_MPI_VI_StartPipe，
    // 实际使用中由驱动/ISP 内部完成，我们这里也不再手动 StartPipe，避免无效调用。
    m_viInitialized = true;
    std::cout << "[MediaManager] VI initialized" << std::endl;

    // 8. 读取实际通道属性，记录真实宽高，用于后续 VPSS/VENC
    VI_CHN_ATTR_S stChnAttrGet;
    memset(&stChnAttrGet, 0, sizeof(VI_CHN_ATTR_S));
    s32Ret = RK_MPI_VI_GetChnAttr(m_viPipeId, m_viChnId, &stChnAttrGet);
    if (s32Ret == RK_SUCCESS) {
        m_imgWidth  = stChnAttrGet.stSize.u32Width;
        m_imgHeight = stChnAttrGet.stSize.u32Height;
    } else {
        // 回退到期望值
        m_imgWidth  = 3840;
        m_imgHeight = 2160;
    }
    std::cout << "[MediaManager] VI actual size: "
              << m_imgWidth << "x" << m_imgHeight << std::endl;

    return true;
}

void MediaManager::cleanupVI() {
    if (!m_viInitialized) {
        return;
    }
    
    RK_MPI_VI_DisableChn(m_viPipeId, m_viChnId);
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
    // 使用实际图像宽高（来自 VI 通道）
    RK_U32 vpssW = m_imgWidth  > 0 ? static_cast<RK_U32>(m_imgWidth)  : 3840;
    RK_U32 vpssH = m_imgHeight > 0 ? static_cast<RK_U32>(m_imgHeight) : 2160;

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
    stChnAttr.u32Width  = vpssW;
    stChnAttr.u32Height = vpssH;
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

    // 使用实际图像宽高（与 VPSS 一致）
    RK_U32 vencW = m_imgWidth  > 0 ? static_cast<RK_U32>(m_imgWidth)  : 3840;
    RK_U32 vencH = m_imgHeight > 0 ? static_cast<RK_U32>(m_imgHeight) : 2160;
    stVencAttr.stVencAttr.u32PicWidth  = vencW;
    stVencAttr.stVencAttr.u32PicHeight = vencH;
    stVencAttr.stVencAttr.u32VirWidth  = vencW;
    stVencAttr.stVencAttr.u32VirHeight = vencH;
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

    // 启动接收帧（-1 表示不限帧数）
    VENC_RECV_PIC_PARAM_S stRecvParam;
    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    stRecvParam.s32RecvPicNum = -1;

    s32Ret = RK_MPI_VENC_StartRecvFrame(m_vencChnId, &stRecvParam);
    if (s32Ret != RK_SUCCESS) {
        std::cerr << "[MediaManager] RK_MPI_VENC_StartRecvFrame failed: " << s32Ret << std::endl;
        RK_MPI_VENC_DestroyChn(m_vencChnId);
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

