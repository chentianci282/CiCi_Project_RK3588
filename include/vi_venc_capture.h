/*
 * VI采集和VENC编码核心功能头文件
 * 从test_mpi_vi.cpp中提取VI采集和VENC编码相关接口
 */

#ifndef VI_VENC_CAPTURE_H
#define VI_VENC_CAPTURE_H

#include "rk_common.h"
#include "rk_comm_vi.h"
#include "rk_comm_venc.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 数据结构定义（前向声明，完整定义在.c文件中）
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
    
    // VI属性（内部使用）
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
// 主要接口函数
// ============================================================================

/**
 * 初始化VI_VENC_CTX_S上下文（设置默认值）
 * @param ctx: 上下文指针
 */
void vi_venc_ctx_init(VI_VENC_CTX_S *ctx);

/**
 * VI采集和VENC编码完整流程
 * 这是核心函数，展示了VI采集->VENC编码的完整流程
 * 
 * 流程说明：
 * 1. 初始化MPI系统
 * 2. 初始化VI设备（设置设备属性、启用设备、绑定管道、配置通道、启用通道）
 * 3. 创建VENC编码器（配置编码参数、创建通道、开始接收帧）
 * 4. 绑定VI到VENC（通过RK_MPI_SYS_Bind实现数据自动传输）
 * 5. 循环获取编码流数据
 * 6. 清理资源（解绑、销毁编码器、反初始化VI）
 * 
 * @param ctx: VI和VENC上下文
 * @return: RK_SUCCESS成功，其他失败
 */
RK_S32 vi_venc_capture_process(VI_VENC_CTX_S *ctx);

#ifdef __cplusplus
}
#endif

#endif // VI_VENC_CAPTURE_H

