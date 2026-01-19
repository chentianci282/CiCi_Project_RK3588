/*
 * VI采集和VENC编码核心功能抽离
 * 从test_mpi_vi.cpp中提取VI采集和VENC编码相关代码
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <errno.h>

#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_vi.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_common.h"
#include "rk_comm_vi.h"
#include "rk_comm_venc.h"
#include "test_comm_utils.h"

// ============================================================================
// 数据结构定义
// ============================================================================

#define MAX_VENC_CHN 2

/**
 * VENC编码器配置
 */
typedef struct {
    RK_BOOL bOutDebugCfg;          // 是否输出调试文件
    VENC_CHN_ATTR_S stAttr;         // 编码器属性
    RK_CHAR dstFilePath[128];       // 输出文件路径
    RK_CHAR dstFileName[128];       // 输出文件名
    RK_S32 s32ChnId;                // 编码器通道ID
    FILE *fp;                       // 文件指针
    RK_S32 selectFd;                // 选择文件描述符
} VENC_CFG_S;

/**
 * VI采集和VENC编码上下文
 */
typedef struct {
    // VI相关参数
    RK_S32 devId;                   // VI设备ID
    RK_S32 pipeId;                  // VI管道ID
    RK_S32 channelId;               // VI通道ID
    RK_S32 width;                   // 图像宽度
    RK_S32 height;                  // 图像高度
    const char *aEntityName;        // 设备实体名称（如：/dev/video44 或 rkispp_scale0）
    COMPRESS_MODE_E enCompressMode; // 压缩模式
    PIXEL_FORMAT_E enPixelFormat;   // 像素格式
    
    // VI属性
    VI_DEV_ATTR_S stDevAttr;        // VI设备属性
    VI_DEV_BIND_PIPE_S stBindPipe;  // VI设备绑定管道
    VI_CHN_ATTR_S stChnAttr;        // VI通道属性
    
    // VENC相关参数
    RK_U32 u32VencChnCount;         // 编码器通道数量
    VENC_CFG_S stVencCfg[MAX_VENC_CHN]; // 编码器配置数组
    VENC_STREAM_S stVencStream[MAX_VENC_CHN]; // 编码流数组
    
    // 控制参数
    RK_S32 loopCountSet;            // 循环次数
    RK_BOOL bFreeze;                // 是否冻结
} VI_VENC_CTX_S;

// ============================================================================
// VI初始化相关函数
// ============================================================================

/**
 * 初始化VI设备属性（简化版，使用默认配置）
 */
static RK_S32 vi_init_dev_attr(VI_VENC_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    
    // 获取设备属性
    s32Ret = RK_MPI_VI_GetDevAttr(ctx->devId, &ctx->stDevAttr);
    if (s32Ret == RK_ERR_VI_NOT_CONFIG) {
        // 如果设备未配置，设置默认属性
        memset(&ctx->stDevAttr, 0, sizeof(VI_DEV_ATTR_S));
        ctx->stDevAttr.enIntfMode = VI_MODE_DIGITAL_CAMERA;
        ctx->stDevAttr.enWorkMode = VI_WORK_MODE_1Multiplex;
        ctx->stDevAttr.stMaxSize.u32Width = ctx->width;
        ctx->stDevAttr.stMaxSize.u32Height = ctx->height;
        ctx->stDevAttr.enPixFmt = ctx->enPixelFormat;
        ctx->stDevAttr.enBufType = VI_V4L2_MEMORY_TYPE_DMABUF;
        ctx->stDevAttr.u32BufCount = 3;
        
        s32Ret = RK_MPI_VI_SetDevAttr(ctx->devId, &ctx->stDevAttr);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_SetDevAttr failed: 0x%x", s32Ret);
            return s32Ret;
        }
    }
    
    return RK_SUCCESS;
}

/**
 * 初始化VI通道属性
 */
static RK_S32 vi_init_chn_attr(VI_VENC_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    
    // 配置通道属性
    memset(&ctx->stChnAttr, 0, sizeof(VI_CHN_ATTR_S));
    ctx->stChnAttr.stSize.u32Width = ctx->width;
    ctx->stChnAttr.stSize.u32Height = ctx->height;
    ctx->stChnAttr.enPixelFormat = ctx->enPixelFormat;
    ctx->stChnAttr.enCompressMode = ctx->enCompressMode;
    ctx->stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    ctx->stChnAttr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;
    ctx->stChnAttr.stIspOpt.u32BufCount = 3;
    ctx->stChnAttr.u32Depth = 0;  // 绑定模式下depth设为0
    ctx->stChnAttr.stFrameRate.s32SrcFrameRate = -1;
    ctx->stChnAttr.stFrameRate.s32DstFrameRate = -1;
    
    // 设置设备实体名称
    if (ctx->aEntityName != RK_NULL) {
        strncpy(ctx->stChnAttr.stIspOpt.aEntityName, ctx->aEntityName, 
                MAX_VI_ENTITY_NAME_LEN - 1);
        ctx->stChnAttr.stIspOpt.aEntityName[MAX_VI_ENTITY_NAME_LEN - 1] = '\0';
    }
    
    // 设置通道属性
    s32Ret = RK_MPI_VI_SetChnAttr(ctx->pipeId, ctx->channelId, &ctx->stChnAttr);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VI_SetChnAttr failed: 0x%x", s32Ret);
        return s32Ret;
    }
    
    return RK_SUCCESS;
}

/**
 * VI完整初始化
 */
static RK_S32 vi_init(VI_VENC_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    
    // 1. 初始化设备属性
    s32Ret = vi_init_dev_attr(ctx);
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }
    
    // 2. 检查并启用设备
    s32Ret = RK_MPI_VI_GetDevIsEnable(ctx->devId);
    if (s32Ret != RK_SUCCESS) {
        s32Ret = RK_MPI_VI_EnableDev(ctx->devId);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_EnableDev failed: 0x%x", s32Ret);
            return s32Ret;
        }
        
        // 绑定设备到管道
        ctx->stBindPipe.u32Num = 1;
        ctx->stBindPipe.PipeId[0] = ctx->pipeId;
        s32Ret = RK_MPI_VI_SetDevBindPipe(ctx->devId, &ctx->stBindPipe);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VI_SetDevBindPipe failed: 0x%x", s32Ret);
            return s32Ret;
        }
    }
    
    // 3. 初始化通道属性
    s32Ret = vi_init_chn_attr(ctx);
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }
    
    // 4. 启用通道
    s32Ret = RK_MPI_VI_EnableChn(ctx->pipeId, ctx->channelId);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VI_EnableChn failed: 0x%x", s32Ret);
        return s32Ret;
    }
    
    RK_LOGI("VI初始化成功: devId=%d, pipeId=%d, channelId=%d, entityName=%s",
            ctx->devId, ctx->pipeId, ctx->channelId, 
            ctx->aEntityName ? ctx->aEntityName : "NULL");
    
    return RK_SUCCESS;
}

/**
 * VI反初始化
 */
static RK_S32 vi_deinit(VI_VENC_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    
    // 禁用通道
    s32Ret = RK_MPI_VI_DisableChn(ctx->pipeId, ctx->channelId);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VI_DisableChn failed: 0x%x", s32Ret);
    }
    
    // 禁用设备
    s32Ret = RK_MPI_VI_DisableDev(ctx->devId);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VI_DisableDev failed: 0x%x", s32Ret);
    }
    
    RK_LOGI("VI反初始化完成");
    return RK_SUCCESS;
}

// ============================================================================
// VENC编码相关函数
// ============================================================================

/**
 * 初始化VENC编码器配置
 */
static RK_S32 venc_init_cfg(VI_VENC_CTX_S *ctx, RK_U32 u32Ch, RK_CODEC_ID_E enType) {
    if (u32Ch >= MAX_VENC_CHN) {
        RK_LOGE("VENC通道ID超出范围: %d", u32Ch);
        return RK_ERR_VENC_ILLEGAL_PARAM;
    }
    
    VENC_CFG_S *pVencCfg = &ctx->stVencCfg[u32Ch];
    
    // 初始化编码器属性
    memset(&pVencCfg->stAttr, 0, sizeof(VENC_CHN_ATTR_S));
    
    // 基本属性
    pVencCfg->stAttr.stVencAttr.enType = enType;
    pVencCfg->stAttr.stVencAttr.enPixelFormat = ctx->enPixelFormat;
    pVencCfg->stAttr.stVencAttr.u32PicWidth = ctx->width;
    pVencCfg->stAttr.stVencAttr.u32PicHeight = ctx->height;
    pVencCfg->stAttr.stVencAttr.u32VirWidth = ctx->width;
    pVencCfg->stAttr.stVencAttr.u32VirHeight = ctx->height;
    pVencCfg->stAttr.stVencAttr.u32StreamBufCnt = 5;
    pVencCfg->stAttr.stVencAttr.u32BufSize = ctx->width * ctx->height * 3 / 2;
    
    // 设置Profile
    if (enType == RK_VIDEO_ID_AVC) {
        pVencCfg->stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
    } else if (enType == RK_VIDEO_ID_HEVC) {
        pVencCfg->stAttr.stVencAttr.u32Profile = H265E_PROFILE_MAIN;
    }
    
    // 码率控制配置（CBR模式）
    pVencCfg->stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    pVencCfg->stAttr.stRcAttr.stH264Cbr.u32Gop = 60;
    pVencCfg->stAttr.stRcAttr.stH264Cbr.u32BitRate = ctx->width * ctx->height / 1000; // 简单计算码率
    pVencCfg->stAttr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
    pVencCfg->stAttr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    pVencCfg->stAttr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
    pVencCfg->stAttr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    
    pVencCfg->s32ChnId = u32Ch;
    
    RK_LOGI("VENC配置初始化: chnId=%d, type=%d, size=%dx%d", 
            u32Ch, enType, ctx->width, ctx->height);
    
    return RK_SUCCESS;
}

/**
 * 创建VENC编码器
 */
static RK_S32 venc_create(VI_VENC_CTX_S *ctx, RK_U32 u32Ch) {
    RK_S32 s32Ret = RK_SUCCESS;
    VENC_RECV_PIC_PARAM_S stRecvParam;
    
    if (u32Ch >= MAX_VENC_CHN) {
        RK_LOGE("VENC通道ID超出范围: %d", u32Ch);
        return RK_ERR_VENC_ILLEGAL_PARAM;
    }
    
    VENC_CFG_S *pVencCfg = &ctx->stVencCfg[u32Ch];
    
    // 创建编码器通道
    s32Ret = RK_MPI_VENC_CreateChn(pVencCfg->s32ChnId, &pVencCfg->stAttr);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_CreateChn failed: chnId=%d, ret=0x%x", 
                pVencCfg->s32ChnId, s32Ret);
        return s32Ret;
    }
    
    // 开始接收帧
    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    stRecvParam.s32RecvPicNum = ctx->loopCountSet;
    s32Ret = RK_MPI_VENC_StartRecvFrame(pVencCfg->s32ChnId, &stRecvParam);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_StartRecvFrame failed: chnId=%d, ret=0x%x", 
                pVencCfg->s32ChnId, s32Ret);
        RK_MPI_VENC_DestroyChn(pVencCfg->s32ChnId);
        return s32Ret;
    }
    
    // 分配流包内存
    ctx->stVencStream[u32Ch].pstPack = 
        reinterpret_cast<VENC_PACK_S *>(malloc(sizeof(VENC_PACK_S)));
    if (ctx->stVencStream[u32Ch].pstPack == RK_NULL) {
        RK_LOGE("malloc VENC_PACK_S failed");
        RK_MPI_VENC_StopRecvFrame(pVencCfg->s32ChnId);
        RK_MPI_VENC_DestroyChn(pVencCfg->s32ChnId);
        return RK_ERR_VENC_NULL_PTR;
    }
    
    // 如果启用调试输出，打开文件
    if (pVencCfg->bOutDebugCfg) {
        char filePath[256];
        snprintf(filePath, sizeof(filePath), "/%s/%s", 
                 pVencCfg->dstFilePath, pVencCfg->dstFileName);
        pVencCfg->fp = fopen(filePath, "wb");
        if (pVencCfg->fp == RK_NULL) {
            RK_LOGE("open file failed: %s", filePath);
        } else {
            RK_LOGI("打开编码输出文件: %s", filePath);
        }
    }
    
    RK_LOGI("VENC编码器创建成功: chnId=%d", pVencCfg->s32ChnId);
    
    return RK_SUCCESS;
}

/**
 * 销毁VENC编码器
 */
static RK_S32 venc_destroy(VI_VENC_CTX_S *ctx, RK_U32 u32Ch) {
    RK_S32 s32Ret = RK_SUCCESS;
    
    if (u32Ch >= MAX_VENC_CHN) {
        return RK_ERR_VENC_ILLEGAL_PARAM;
    }
    
    VENC_CFG_S *pVencCfg = &ctx->stVencCfg[u32Ch];
    
    // 停止接收帧
    s32Ret = RK_MPI_VENC_StopRecvFrame(pVencCfg->s32ChnId);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_StopRecvFrame failed: 0x%x", s32Ret);
    }
    
    // 销毁编码器通道
    s32Ret = RK_MPI_VENC_DestroyChn(pVencCfg->s32ChnId);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_DestroyChn failed: 0x%x", s32Ret);
    }
    
    // 释放流包内存
    if (ctx->stVencStream[u32Ch].pstPack) {
        free(ctx->stVencStream[u32Ch].pstPack);
        ctx->stVencStream[u32Ch].pstPack = RK_NULL;
    }
    
    // 关闭文件
    if (pVencCfg->fp) {
        fclose(pVencCfg->fp);
        pVencCfg->fp = RK_NULL;
    }
    
    RK_LOGI("VENC编码器销毁完成: chnId=%d", pVencCfg->s32ChnId);
    
    return RK_SUCCESS;
}

// ============================================================================
// VI和VENC绑定相关函数
// ============================================================================

/**
 * 绑定VI到VENC
 */
static RK_S32 vi_bind_venc(VI_VENC_CTX_S *ctx, RK_U32 u32VencChn) {
    RK_S32 s32Ret = RK_SUCCESS;
    MPP_CHN_S stSrcChn, stDestChn;
    
    if (u32VencChn >= MAX_VENC_CHN) {
        RK_LOGE("VENC通道ID超出范围: %d", u32VencChn);
        return RK_ERR_VENC_ILLEGAL_PARAM;
    }
    
    // 配置源通道（VI）
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = ctx->devId;
    stSrcChn.s32ChnId = ctx->channelId;
    
    // 配置目标通道（VENC）
    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32DevId = u32VencChn;
    stDestChn.s32ChnId = ctx->stVencCfg[u32VencChn].s32ChnId;
    
    // 执行绑定
    s32Ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_SYS_Bind failed: VI[%d:%d] -> VENC[%d:%d], ret=0x%x",
                stSrcChn.s32DevId, stSrcChn.s32ChnId,
                stDestChn.s32DevId, stDestChn.s32ChnId, s32Ret);
        return s32Ret;
    }
    
    RK_LOGI("VI绑定VENC成功: VI[%d:%d] -> VENC[%d:%d]",
            stSrcChn.s32DevId, stSrcChn.s32ChnId,
            stDestChn.s32DevId, stDestChn.s32ChnId);
    
    return RK_SUCCESS;
}

/**
 * 解绑VI和VENC
 */
static RK_S32 vi_unbind_venc(VI_VENC_CTX_S *ctx, RK_U32 u32VencChn) {
    RK_S32 s32Ret = RK_SUCCESS;
    MPP_CHN_S stSrcChn, stDestChn;
    
    if (u32VencChn >= MAX_VENC_CHN) {
        return RK_ERR_VENC_ILLEGAL_PARAM;
    }
    
    // 配置源通道和目标通道
    stSrcChn.enModId = RK_ID_VI;
    stSrcChn.s32DevId = ctx->devId;
    stSrcChn.s32ChnId = ctx->channelId;
    
    stDestChn.enModId = RK_ID_VENC;
    stDestChn.s32DevId = u32VencChn;
    stDestChn.s32ChnId = ctx->stVencCfg[u32VencChn].s32ChnId;
    
    // 执行解绑
    s32Ret = RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_SYS_UnBind failed: 0x%x", s32Ret);
    }
    
    RK_LOGI("VI解绑VENC完成: VENC[%d:%d]", stDestChn.s32DevId, stDestChn.s32ChnId);
    
    return s32Ret;
}

// ============================================================================
// 编码流获取和处理
// ============================================================================

/**
 * 获取编码流数据
 * @param ctx: 上下文
 * @param u32VencChn: VENC通道ID
 * @param timeoutMs: 超时时间（毫秒），-1表示阻塞等待
 * @return: RK_SUCCESS成功，其他失败
 */
static RK_S32 venc_get_stream(VI_VENC_CTX_S *ctx, RK_U32 u32VencChn, RK_S32 timeoutMs) {
    RK_S32 s32Ret = RK_SUCCESS;
    void *pData = RK_NULL;
    
    if (u32VencChn >= MAX_VENC_CHN) {
        return RK_ERR_VENC_ILLEGAL_PARAM;
    }
    
    VENC_CFG_S *pVencCfg = &ctx->stVencCfg[u32VencChn];
    VENC_STREAM_S *pStream = &ctx->stVencStream[u32VencChn];
    
    // 获取编码流
    s32Ret = RK_MPI_VENC_GetStream(pVencCfg->s32ChnId, pStream, timeoutMs);
    if (s32Ret == RK_SUCCESS) {
        // 如果启用调试输出，保存到文件
        if (pVencCfg->bOutDebugCfg && pVencCfg->fp) {
            pData = RK_MPI_MB_Handle2VirAddr(pStream->pstPack->pMbBlk);
            RK_MPI_SYS_MmzFlushCache(pStream->pstPack->pMbBlk, RK_TRUE);
            fwrite(pData, 1, pStream->pstPack->u32Len, pVencCfg->fp);
            fflush(pVencCfg->fp);
        }
        
        // 检查是否到达流结束
        if (pStream->pstPack->bStreamEnd == RK_TRUE) {
            RK_LOGI("VENC通道%d到达流结束，序列号=%d", 
                    pVencCfg->s32ChnId, pStream->u32Seq);
        }
        
        // 释放流（必须在处理完数据后释放）
        s32Ret = RK_MPI_VENC_ReleaseStream(pVencCfg->s32ChnId, pStream);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("RK_MPI_VENC_ReleaseStream failed: 0x%x", s32Ret);
        }
    }
    
    return s32Ret;
}

// ============================================================================
// 主要流程函数
// ============================================================================

/**
 * VI采集和VENC编码完整流程
 * 这是核心函数，展示了VI采集->VENC编码的完整流程
 */
RK_S32 vi_venc_capture_process(VI_VENC_CTX_S *ctx) {
    RK_S32 s32Ret = RK_SUCCESS;
    RK_U32 i;
    RK_S32 loopCount = 0;
    
    // 1. 初始化MPI系统
    s32Ret = RK_MPI_SYS_Init();
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_SYS_Init failed: 0x%x", s32Ret);
        return s32Ret;
    }
    
    // 2. 初始化VI
    s32Ret = vi_init(ctx);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("VI初始化失败: 0x%x", s32Ret);
        goto __FAILED;
    }
    
    // 3. 创建并初始化所有VENC编码器
    for (i = 0; i < ctx->u32VencChnCount; i++) {
        // 初始化编码器配置（使用H264编码）
        s32Ret = venc_init_cfg(ctx, i, RK_VIDEO_ID_AVC);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("VENC配置初始化失败: chn=%d, ret=0x%x", i, s32Ret);
            goto __FAILED;
        }
        
        // 创建编码器
        s32Ret = venc_create(ctx, i);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("VENC创建失败: chn=%d, ret=0x%x", i, s32Ret);
            goto __FAILED;
        }
        
        // 绑定VI到VENC
        s32Ret = vi_bind_venc(ctx, i);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("VI绑定VENC失败: chn=%d, ret=0x%x", i, s32Ret);
            goto __FAILED;
        }
    }
    
    RK_LOGI("VI采集和VENC编码初始化完成，开始循环处理...");
    
    // 4. 主循环：获取编码流数据
    while (loopCount < ctx->loopCountSet) {
        for (i = 0; i < ctx->u32VencChnCount; i++) {
            // 设置冻结状态（如果需要）
            if (ctx->bFreeze) {
                RK_MPI_VI_SetChnFreeze(ctx->pipeId, ctx->channelId, RK_TRUE);
            }
            
            // 获取编码流（阻塞等待，直到有数据）
            s32Ret = venc_get_stream(ctx, i, -1);
            if (s32Ret == RK_SUCCESS) {
                RK_LOGD("获取编码流成功: chn=%d, loop=%d, seq=%d, len=%d",
                        i, loopCount, 
                        ctx->stVencStream[i].u32Seq,
                        ctx->stVencStream[i].pstPack->u32Len);
                loopCount++;
            } else {
                RK_LOGE("获取编码流失败: chn=%d, ret=0x%x", i, s32Ret);
            }
        }
        
        usleep(10 * 1000); // 10ms延迟
    }
    
    RK_LOGI("循环处理完成，共处理%d次", loopCount);
    
__FAILED:
    // 5. 清理资源
    // 解绑VI和VENC
    for (i = 0; i < ctx->u32VencChnCount; i++) {
        vi_unbind_venc(ctx, i);
    }
    
    // 销毁VENC编码器
    for (i = 0; i < ctx->u32VencChnCount; i++) {
        venc_destroy(ctx, i);
    }
    
    // 反初始化VI
    vi_deinit(ctx);
    
    // 退出MPI系统
    RK_MPI_SYS_Exit();
    
    return s32Ret;
}

// ============================================================================
// 辅助函数：初始化上下文
// ============================================================================

/**
 * 初始化VI_VENC_CTX_S上下文（设置默认值）
 */
void vi_venc_ctx_init(VI_VENC_CTX_S *ctx) {
    if (ctx == RK_NULL) {
        return;
    }
    
    memset(ctx, 0, sizeof(VI_VENC_CTX_S));
    
    // VI默认参数
    ctx->devId = 0;
    ctx->pipeId = 0;
    ctx->channelId = 1;
    ctx->width = 1920;
    ctx->height = 1080;
    ctx->aEntityName = RK_NULL;
    ctx->enCompressMode = COMPRESS_MODE_NONE;
    ctx->enPixelFormat = RK_FMT_YUV420SP;
    
    // VENC默认参数
    ctx->u32VencChnCount = 1;
    
    // 控制参数
    ctx->loopCountSet = 100;
    ctx->bFreeze = RK_FALSE;
    
    // 初始化VENC配置
    for (RK_U32 i = 0; i < MAX_VENC_CHN; i++) {
        ctx->stVencCfg[i].bOutDebugCfg = RK_FALSE;
        ctx->stVencCfg[i].s32ChnId = i;
        strcpy(ctx->stVencCfg[i].dstFilePath, "data");
        snprintf(ctx->stVencCfg[i].dstFileName, sizeof(ctx->stVencCfg[i].dstFileName),
                 "venc_%d.bin", i);
    }
}

// ============================================================================
// 主函数：简单的采集+编码示例
// ============================================================================

int main(int argc, char **argv) {
    VI_VENC_CTX_S ctx;
    RK_S32 s32Ret;
    
    RK_LOGI("==========================================");
    RK_LOGI("  VI采集和VENC编码测试程序");
    RK_LOGI("==========================================");
    
    // 初始化上下文
    vi_venc_ctx_init(&ctx);
    
    // 配置VI参数
    ctx.devId = 0;              // VI设备ID
    ctx.pipeId = 0;             // VI管道ID
    ctx.channelId = 1;          // VI通道ID
    ctx.width = 1920;           // 图像宽度
    ctx.height = 1080;          // 图像高度
    
    // 指定设备实体名称（RK3588上ISP0主路径）
    // 可以使用实体名称或设备节点路径：
    // - "rkispp_scale0" : ISP缩放通道0
    // - "/dev/video44"  : ISP0主路径设备节点
    ctx.aEntityName = "rkispp_scale0";  // 使用ISP缩放通道0
    
    ctx.enPixelFormat = RK_FMT_YUV420SP;  // 像素格式：YUV420半平面
    ctx.enCompressMode = COMPRESS_MODE_NONE; // 压缩模式：无压缩
    
    // 配置VENC参数
    ctx.u32VencChnCount = 1;    // 使用1个编码器通道
    ctx.stVencCfg[0].bOutDebugCfg = RK_TRUE;  // 启用调试输出，保存到文件
    ctx.stVencCfg[0].s32ChnId = 0;
    strcpy(ctx.stVencCfg[0].dstFilePath, "data");
    strcpy(ctx.stVencCfg[0].dstFileName, "test_capture.h264");
    
    // 控制参数
    ctx.loopCountSet = 300;     // 处理300帧（约10秒@30fps）
    ctx.bFreeze = RK_FALSE;     // 不冻结
    
    RK_LOGI("配置信息:");
    RK_LOGI("  VI设备: devId=%d, pipeId=%d, channelId=%d", 
            ctx.devId, ctx.pipeId, ctx.channelId);
    RK_LOGI("  分辨率: %dx%d", ctx.width, ctx.height);
    RK_LOGI("  设备实体: %s", ctx.aEntityName);
    RK_LOGI("  像素格式: YUV420SP");
    RK_LOGI("  编码器通道数: %d", ctx.u32VencChnCount);
    RK_LOGI("  输出文件: /%s/%s", 
            ctx.stVencCfg[0].dstFilePath, ctx.stVencCfg[0].dstFileName);
    RK_LOGI("  处理帧数: %d", ctx.loopCountSet);
    RK_LOGI("");
    
    // 执行采集和编码流程
    RK_LOGI("开始VI采集和VENC编码...");
    s32Ret = vi_venc_capture_process(&ctx);
    
    if (s32Ret == RK_SUCCESS) {
        RK_LOGI("==========================================");
        RK_LOGI("  VI采集和VENC编码完成！");
        RK_LOGI("  编码文件保存在: /%s/%s", 
                ctx.stVencCfg[0].dstFilePath, ctx.stVencCfg[0].dstFileName);
        RK_LOGI("==========================================");
        return 0;
    } else {
        RK_LOGE("==========================================");
        RK_LOGE("  VI采集和VENC编码失败: 0x%x", s32Ret);
        RK_LOGE("==========================================");
        return -1;
    }
}

