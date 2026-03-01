# 瑞芯微 VI、VPSS、VO 概念说明

## 1. 核心概念

### 1.1 VI (Video Input) - 视频输入

**定义：** 视频输入模块，负责从摄像头采集视频数据

**功能：**
- 从摄像头硬件采集原始视频数据
- 支持多路输入（多个摄像头）
- 输出 YUV 格式数据

**关键 API：**
```cpp
// 初始化 VI 设备
RK_MPI_VI_SetDevAttr(devId, &stDevAttr);
RK_MPI_VI_EnableDev(devId);

// 创建 VI 通道
RK_MPI_VI_SetChnAttr(pipeId, chnId, &stChnAttr);
RK_MPI_VI_EnableChn(pipeId, chnId);

// 获取视频帧
RK_MPI_VI_GetChnFrame(pipeId, chnId, &stFrame, timeout);
RK_MPI_VI_ReleaseChnFrame(pipeId, chnId, &stFrame);
```

**数据结构：**
```cpp
typedef struct {
    RK_S32 devId;      // 设备ID
    RK_S32 pipeId;     // 管道ID
    RK_S32 channelId;  // 通道ID
    VI_DEV_ATTR_S stDevAttr;   // 设备属性
    VI_CHN_ATTR_S stChnAttr;   // 通道属性
} VI_CTX;
```

### 1.2 VPSS (Video Process Sub-System) - 视频处理子系统

**定义：** 视频处理子系统，负责对视频数据进行处理

**功能：**
- **缩放（Scale）**：改变视频分辨率
- **裁剪（Crop）**：裁剪视频区域
- **格式转换**：YUV 格式转换
- **多路输出**：一个输入可以输出到多个通道（不同分辨率）

**关键 API：**
```cpp
// 创建 VPSS 组
RK_MPI_VPSS_CreateGrp(grpId, &stGrpAttr);

// 设置通道属性（可以设置不同的输出分辨率）
RK_MPI_VPSS_SetChnAttr(grpId, chnId, &stChnAttr);
RK_MPI_VPSS_EnableChn(grpId, chnId);

// 启动 VPSS 组
RK_MPI_VPSS_StartGrp(grpId);
```

**数据结构：**
```cpp
typedef struct {
    RK_S32 s32GrpId;           // 组ID
    RK_S32 s32ChnId;           // 通道ID
    VPSS_GRP_ATTR_S stGrpAttr; // 组属性
    VPSS_CHN_ATTR_S stChnAttr; // 通道属性
} VPSS_CTX;
```

**典型应用场景：**
- 主码流：1920x1080 → 编码
- 子码流：640x480 → 编码（低码率）
- 算法喂帧：1920x1080 → 算法处理
- 显示：1920x1080 → 显示

### 1.3 VO (Video Output) - 视频输出

**定义：** 视频输出模块，负责将视频数据输出到显示设备

**功能：**
- 输出到 HDMI/LCD 等显示设备
- 支持多图层叠加
- 支持多路输出

**关键 API：**
```cpp
// 初始化 VO 设备
RK_MPI_VO_SetPubAttr(devId, &stPubAttr);
RK_MPI_VO_Enable(devId);

// 创建显示层
RK_MPI_VO_BindLayer(layerId, devId, VO_LAYER_MODE_VIDEO);
RK_MPI_VO_SetLayerAttr(layerId, &stLayerAttr);
RK_MPI_VO_EnableLayer(layerId);

// 启用通道
RK_MPI_VO_EnableChn(layerId, chnId);
```

**数据结构：**
```cpp
typedef struct {
    VO_DEV s32VoDev;      // VO 设备ID
    VO_LAYER s32VoLayer;  // 显示层ID
    RK_S32 s32ChnId;      // 通道ID
} VO_CTX;
```

### 1.4 VENC (Video Encoder) - 视频编码器

**定义：** 视频编码模块，负责将 YUV 数据编码为 H264/H265

**功能：**
- H264/H265 硬件编码
- 支持多路编码
- 输出编码后的码流

## 2. 绑定机制

### 2.1 绑定概念

**绑定（Bind）**：将两个模块的通道连接起来，数据自动从源通道流向目标通道

**绑定方式：**
```cpp
// 绑定函数
RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);

// 解绑函数
RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
```

**通道结构：**
```cpp
typedef struct {
    MPP_MOD_ID_E enModId;  // 模块ID (RK_ID_VI, RK_ID_VPSS, RK_ID_VO, RK_ID_VENC)
    RK_S32 s32DevId;       // 设备ID
    RK_S32 s32ChnId;       // 通道ID
} MPP_CHN_S;
```

### 2.2 绑定优势

1. **零拷贝**：数据在硬件内部流转，不需要 CPU 参与
2. **高性能**：硬件加速，延迟低
3. **低功耗**：不需要 CPU 拷贝数据
4. **简化编程**：不需要手动获取和发送数据

### 2.3 常见绑定模式

#### 模式 1：VI → VO（直接显示）

```
摄像头 → VI → VO → 屏幕
```

**代码示例：**
```cpp
// 1. 初始化 VI
RK_MPI_VI_SetDevAttr(devId, &stDevAttr);
RK_MPI_VI_EnableDev(devId);
RK_MPI_VI_SetChnAttr(pipeId, chnId, &stChnAttr);
RK_MPI_VI_EnableChn(pipeId, chnId);

// 2. 初始化 VO
RK_MPI_VO_SetPubAttr(voDev, &stPubAttr);
RK_MPI_VO_Enable(voDev);
RK_MPI_VO_BindLayer(layerId, voDev, VO_LAYER_MODE_VIDEO);
RK_MPI_VO_EnableLayer(layerId);
RK_MPI_VO_EnableChn(layerId, 0);

// 3. 绑定 VI 到 VO
MPP_CHN_S stSrcChn, stDestChn;
stSrcChn.enModId = RK_ID_VI;
stSrcChn.s32DevId = devId;
stSrcChn.s32ChnId = chnId;

stDestChn.enModId = RK_ID_VO;
stDestChn.s32DevId = layerId;
stDestChn.s32ChnId = 0;

RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
// 数据自动从 VI 流向 VO，无需手动获取和发送
```

#### 模式 2：VI → VPSS → VENC（编码）

```
摄像头 → VI → VPSS → VENC → 编码流
```

**代码示例：**
```cpp
// 1. 初始化 VI（同上）

// 2. 初始化 VPSS
RK_MPI_VPSS_CreateGrp(grpId, &stGrpAttr);
RK_MPI_VPSS_SetChnAttr(grpId, chnId, &stChnAttr);
RK_MPI_VPSS_EnableChn(grpId, chnId);
RK_MPI_VPSS_StartGrp(grpId);

// 3. 初始化 VENC
RK_MPI_VENC_CreateChn(vencChnId, &stVencAttr);
RK_MPI_VENC_StartRecvFrame(vencChnId, &stRecvParam);

// 4. 绑定 VI → VPSS
MPP_CHN_S stViChn, stVpssChn;
stViChn.enModId = RK_ID_VI;
stViChn.s32DevId = devId;
stViChn.s32ChnId = chnId;

stVpssChn.enModId = RK_ID_VPSS;
stVpssChn.s32DevId = grpId;
stVpssChn.s32ChnId = chnId;

RK_MPI_SYS_Bind(&stViChn, &stVpssChn);

// 5. 绑定 VPSS → VENC
MPP_CHN_S stVencChn;
stVencChn.enModId = RK_ID_VENC;
stVencChn.s32DevId = 0;
stVencChn.s32ChnId = vencChnId;

RK_MPI_SYS_Bind(&stVpssChn, &stVencChn);

// 6. 获取编码流
VENC_STREAM_S stStream;
RK_MPI_VENC_GetStream(vencChnId, &stStream, timeout);
// 处理编码流...
RK_MPI_VENC_ReleaseStream(vencChnId, &stStream);
```

#### 模式 3：VI → VPSS → 多路输出

```
摄像头 → VI → VPSS ┬→ VENC (主码流 1920x1080)
                   ├→ VENC (子码流 640x480)
                   ├→ VO (显示 1920x1080)
                   └→ 算法 (YUV 1920x1080)
```

**代码示例：**
```cpp
// 1. 初始化 VI（同上）

// 2. 初始化 VPSS，配置多个输出通道
RK_MPI_VPSS_CreateGrp(grpId, &stGrpAttr);

// 通道 0：主码流 1920x1080
stChnAttr[0].u32Width = 1920;
stChnAttr[0].u32Height = 1080;
RK_MPI_VPSS_SetChnAttr(grpId, VPSS_CHN0, &stChnAttr[0]);
RK_MPI_VPSS_EnableChn(grpId, VPSS_CHN0);

// 通道 1：子码流 640x480
stChnAttr[1].u32Width = 640;
stChnAttr[1].u32Height = 480;
RK_MPI_VPSS_SetChnAttr(grpId, VPSS_CHN1, &stChnAttr[1]);
RK_MPI_VPSS_EnableChn(grpId, VPSS_CHN1);

// 通道 2：算法喂帧 1920x1080
stChnAttr[2].u32Width = 1920;
stChnAttr[2].u32Height = 1080;
RK_MPI_VPSS_SetChnAttr(grpId, VPSS_CHN2, &stChnAttr[2]);
RK_MPI_VPSS_EnableChn(grpId, VPSS_CHN2);

RK_MPI_VPSS_StartGrp(grpId);

// 3. 绑定 VI → VPSS
RK_MPI_SYS_Bind(&stViChn, &stVpssChn);

// 4. 绑定 VPSS 各通道到不同目标
// VPSS_CHN0 → VENC (主码流)
RK_MPI_SYS_Bind(&stVpssChn0, &stVencChn0);

// VPSS_CHN1 → VENC (子码流)
RK_MPI_SYS_Bind(&stVpssChn1, &stVencChn1);

// VPSS_CHN2 → 算法（需要手动获取帧）
RK_MPI_VPSS_GetChnFrame(grpId, VPSS_CHN2, &stFrame, timeout);
// 处理算法...
RK_MPI_VPSS_ReleaseChnFrame(grpId, VPSS_CHN2, &stFrame);
```

## 3. 绑定 vs 非绑定模式

### 3.1 绑定模式（推荐）

**特点：**
- 数据自动流转，无需手动获取和发送
- 零拷贝，性能高
- 代码简单

**适用场景：**
- VI → VO（直接显示）
- VI → VPSS → VENC（编码）
- 数据流固定，不需要额外处理

### 3.2 非绑定模式（手动模式）

**特点：**
- 需要手动获取帧数据
- 可以处理数据（算法、水印等）
- 需要手动发送到下一级

**适用场景：**
- 需要算法处理
- 需要软件水印叠加
- 需要数据拷贝或转换

**代码示例：**
```cpp
// 从 VI 获取帧
VIDEO_FRAME_INFO_S stFrame;
RK_MPI_VI_GetChnFrame(pipeId, chnId, &stFrame, timeout);

// 处理数据（算法、水印等）
processFrame(&stFrame);

// 发送到 VPSS
RK_MPI_VPSS_SendFrame(grpId, VPSS_CHN0, &stFrame, timeout);

// 释放 VI 帧
RK_MPI_VI_ReleaseChnFrame(pipeId, chnId, &stFrame);
```

## 4. 与我们架构的对应关系

### 4.1 当前架构（基于 V4L2）

```
CaptureThread (V4L2) → MediaManager → 三个服务
```

### 4.2 瑞芯微架构（基于 MPP）

```
VI → VPSS → ┬→ VENC (编码)
            ├→ VO (显示)
            └→ 算法 (YUV输出)
```

### 4.3 对应关系

| 我们的架构 | 瑞芯微架构 | 说明 |
|-----------|-----------|------|
| CaptureThread | VI | 视频采集 |
| VideoEncoderSvc | VENC | 视频编码 |
| VideoOutputSvc | VO | 视频输出 |
| YUVOutputSvc | VPSS 输出通道 | YUV 数据输出 |
| MediaManager | 绑定管理 | 数据流转协调 |

### 4.4 改进建议

如果要使用瑞芯微的 MPP 架构，可以：

1. **替换 CaptureThread**：使用 VI 替代 V4L2
2. **使用绑定模式**：VI → VPSS → VENC/VO
3. **保留服务架构**：仍然使用服务线程模式，但底层使用 MPP

**优势：**
- 硬件加速，性能更好
- 零拷贝，延迟更低
- 支持多路输出（VPSS）

**劣势：**
- 平台相关，只能用于瑞芯微平台
- 需要学习 MPP API

## 5. 总结

1. **VI、VPSS、VO 是瑞芯微 MPP 的核心模块**
2. **它们之间可以通过 `RK_MPI_SYS_Bind()` 进行绑定**
3. **绑定模式可以实现零拷贝、高性能的数据流转**
4. **VPSS 是关键，可以实现一路输入、多路输出（不同分辨率）**
5. **我们的架构可以基于 MPP 进行优化，使用绑定模式提高性能**

