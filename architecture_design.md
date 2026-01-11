# 车载多摄像头视频处理 + AI识别系统架构设计

## 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        应用层 (Application Layer)                        │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐     │
│  │   UI Controller   │  │  Scene Manager   │  │  Event Handler   │     │
│  │  (UI控制)         │  │  (场景管理)       │  │  (事件处理)       │     │
│  │                   │  │                  │  │                  │     │
│  │ - 布局切换        │  │ - 倒车模式        │  │ - 用户交互        │     │
│  │ - 参数设置        │  │ - 环视模式        │  │ - 系统事件        │     │
│  │ - 显示控制        │  │ - ADAS模式        │  │ - AI告警处理      │     │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘     │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        服务层 (Service Layer)                            │
│                                                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    VideoService (视频服务)                        │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │CameraService │  │ComposeService│  │OutputService  │          │   │
│  │  │(摄像头管理)   │  │(合成服务)     │  │(输出服务)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- 设备注册    │  │- 布局管理    │  │- Buffer管理   │          │   │
│  │  │- 采集控制    │  │- 合成策略    │  │- 输出控制     │          │   │
│  │  │- 格式配置    │  │- 同步控制    │  │              │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                      │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    AIService (AI推理服务)                        │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │ModelManager  │  │InferenceEngine│ │ResultProcessor│          │   │
│  │  │(模型管理)     │  │(推理引擎)     │  │(结果处理)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- 模型加载    │  │- 推理执行    │  │- 后处理      │          │   │
│  │  │- 模型卸载    │  │- 预处理      │  │- 结果缓存    │          │   │
│  │  │- 模型配置    │  │- 硬件加速    │  │- 结果匹配    │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                      │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │              ResultFusionService (结果融合服务)                   │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │FrameMatcher  │  │ResultFuser   │  │DrawEngine    │          │   │
│  │  │(帧匹配)       │  │(结果融合)     │  │(绘制引擎)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- 帧ID管理    │  │- 结果叠加    │  │- 检测框绘制  │          │   │
│  │  │- 时间匹配    │  │- 数据融合    │  │- 文字绘制    │          │   │
│  │  │- 缓存管理    │  │              │  │- 分割绘制    │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                      │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    DisplayService (显示服务)                      │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │RenderEngine  │  │WindowManager │  │DisplayControl │          │   │
│  │  │(渲染引擎)     │  │(窗口管理)     │  │(显示控制)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- 帧渲染      │  │- 图层管理    │  │- 显示配置    │          │   │
│  │  │- 硬件加速    │  │- 窗口控制    │  │- 输出控制    │          │   │
│  │  │- 格式转换    │  │              │  │              │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        处理层 (Processing Layer)                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │
│  │VideoProcessor│  │ComposeEngine │  │ImageProcessor│                 │
│  │(视频处理)     │  │(合成引擎)     │  │(图像处理)     │                 │
│  │              │  │              │  │              │                 │
│  │- 缩放        │  │- 分屏合成    │  │- 格式转换    │                 │
│  │- 裁剪        │  │- 画中画      │  │- 颜色空间    │                 │
│  │- 旋转        │  │- 拼接合成    │  │- 滤波        │                 │
│  │- 格式转换    │  │- 叠加合成    │  │              │                 │
│  └──────────────┘  └──────────────┘  └──────────────┘                 │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        抽象层 (Abstraction Layer)                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │
│  │CameraDevice  │  │OutputDevice  │  │ModelInterface│                 │
│  │(摄像头抽象)   │  │(输出抽象)     │  │(模型抽象)     │                 │
│  │              │  │              │  │              │                 │
│  │- V4L2Camera │  │- DisplayOut  │  │- RKNNModel   │                 │
│  │- USBCamera  │  │- EncoderOut   │  │- ONNXModel   │                 │
│  │- MIPICamera │  │- NetworkOut   │  │- TensorRT    │                 │
│  └──────────────┘  └──────────────┘  └──────────────┘                 │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        驱动层 (Driver Layer)                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │
│  │V4L2Driver    │  │HardwareDriver│  │DisplayDriver │                 │
│  │(V4L2驱动)    │  │(硬件加速驱动) │  │(显示驱动)     │                 │
│  │              │  │              │  │              │                 │
│  │- 设备操作    │  │- RGA驱动     │  │- DRM驱动     │                 │
│  │- Buffer管理  │  │- VPU驱动     │  │- FB驱动      │                 │
│  │- 格式设置    │  │- NPU驱动     │  │              │                 │
│  └──────────────┘  └──────────────┘  └──────────────┘                 │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        硬件层 (Hardware Layer)                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │
│  │  摄像头硬件   │  │   SoC芯片     │  │  显示设备     │                 │
│  │              │  │              │  │              │                 │
│  │- MIPI CSI    │  │- RK3588      │  │- HDMI        │                 │
│  │- USB Camera  │  │- RGA/RKNN    │  │- LVDS        │                 │
│  │              │  │- VPU        │  │- MIPI DSI    │                 │
│  └──────────────┘  └──────────────┘  └──────────────┘                 │
└─────────────────────────────────────────────────────────────────────────┘
```

## 数据流图

```
┌─────────┐
│摄像头硬件│
└────┬────┘
     │
     ↓
┌─────────────────────────────────────────────────────────────┐
│  VideoService                                                │
│  ┌──────────────┐                                           │
│  │CameraService │ → 采集帧 → Frame[frameId, timestamp, data]│
│  └──────────────┘                                           │
│         │                                                   │
│         ↓                                                   │
│  ┌──────────────┐                                           │
│  │ComposeService│ → 合成帧 → CompositeFrame                 │
│  └──────────────┘                                           │
│         │                                                   │
│         ├─────────────────┐                                 │
│         ↓                 ↓                                 │
│  [VideoFrameQueue]  [AIInputQueue]                          │
│         │                 │                                 │
└─────────┼─────────────────┼─────────────────────────────────┘
          │                 │
          │                 ↓
          │         ┌──────────────────────────────────────┐
          │         │  AIService                          │
          │         │  ┌──────────────┐                   │
          │         │  │InferenceEngine│ → AI推理          │
          │         │  └──────────────┘                   │
          │         │         │                           │
          │         │         ↓                           │
          │         │  [AIResultQueue]                    │
          │         │         │                           │
          │         └─────────┼───────────────────────────┘
          │                   │
          │                   ↓
          │         ┌──────────────────────────────────────┐
          │         │  ResultFusionService                 │
          │         │  ┌──────────────┐                   │
          │         │  │FrameMatcher  │ → 匹配帧和结果     │
          │         │  └──────────────┘                   │
          │         │         │                           │
          │         │         ↓                           │
          │         │  ┌──────────────┐                   │
          │         │  │ResultFuser   │ → 融合结果到帧     │
          │         │  └──────────────┘                   │
          │         │         │                           │
          │         │         ↓                           │
          │         │  [FusedFrameQueue]                   │
          │         └─────────┼───────────────────────────┘
          │                   │
          ↓                   ↓
          └───────────┬───────┘
                      │
                      ↓
          ┌──────────────────────────────────────┐
          │  DisplayService                      │
          │  ┌──────────────┐                   │
          │  │RenderEngine  │ → 渲染帧          │
          │  └──────────────┘                   │
          │         │                           │
          │         ↓                           │
          │  ┌──────────────┐                   │
          │  │DisplayControl│ → 输出到显示      │
          │  └──────────────┘                   │
          └──────────────────────────────────────┘
                      │
                      ↓
                ┌─────────┐
                │显示设备  │
                └─────────┘
```

## 线程模型

```
┌─────────────────────────────────────────────────────────────┐
│  主线程 (Main Thread)                                       │
│  - 服务初始化                                               │
│  - 服务管理                                                 │
│  - 事件处理                                                 │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  视频采集线程 (Camera Threads)                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │Camera 0  │  │Camera 1  │  │Camera 2  │  │Camera 3  │  │
│  │Thread    │  │Thread    │  │Thread    │  │Thread    │  │
│  │          │  │          │  │          │  │          │  │
│  │采集 → 队列│  │采集 → 队列│  │采集 → 队列│  │采集 → 队列│  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
└─────────────────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  视频合成线程 (Compose Thread)                               │
│  - 从4个队列取帧                                             │
│  - 合成处理                                                  │
│  - 推送到VideoFrameQueue和AIInputQueue                       │
└─────────────────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  AI推理线程 (AI Inference Thread)                           │
│  - 从AIInputQueue取帧                                        │
│  - 预处理                                                    │
│  - 模型推理                                                  │
│  - 后处理                                                    │
│  - 推送到AIResultQueue                                       │
└─────────────────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  结果融合线程 (Fusion Thread)                                │
│  - 从VideoFrameQueue取帧                                     │
│  - 从AIResultQueue取结果                                     │
│  - 帧匹配                                                    │
│  - 结果融合                                                  │
│  - 推送到FusedFrameQueue                                     │
└─────────────────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────────────────┐
│  显示线程 (Display Thread)                                   │
│  - 从FusedFrameQueue取帧                                     │
│  - 渲染处理                                                  │
│  - 输出到显示设备                                            │
└─────────────────────────────────────────────────────────────┘
```

## 功能配置与场景支持

### 配置系统设计

框架支持通过配置灵活切换不同工作模式，核心配置结构：

```cpp
// 工作模式配置
enum WorkMode {
    MODE_4WAY_COMPOSE,      // 四路拼接输出
    MODE_4WAY_SEPARATE,     // 四路单独输出
    MODE_SINGLE_AI,         // 单路+AI识别
    MODE_MULTI_AI,          // 多路+AI识别
    MODE_CUSTOM             // 自定义模式
};

// 输出配置
struct OutputConfig {
    WorkMode mode;                          // 工作模式
    std::vector<int> activeCameras;         // 激活的摄像头列表
    ComposeStrategy composeStrategy;        // 合成策略
    bool enableAI;                          // 是否启用AI
    std::vector<int> aiCameras;             // AI处理的摄像头列表
    std::vector<OutputTarget> outputTargets; // 输出目标列表
    DisplayLayout displayLayout;            // 显示布局
};

// 合成策略配置
struct ComposeConfig {
    ComposeType type;                       // 合成类型：拼接/分屏/画中画
    std::vector<Rect> regions;              // 各路视频区域
    bool enableSync;                        // 是否同步
};

// 显示布局配置
struct DisplayLayout {
    int windowCount;                        // 窗口数量
    std::vector<WindowConfig> windows;      // 窗口配置
};

struct WindowConfig {
    int windowId;                           // 窗口ID
    Rect position;                          // 窗口位置
    int sourceCameraId;                     // 数据源摄像头ID（-1表示合成帧）
    bool enableAIOverlay;                   // 是否显示AI结果
};
```

### 场景1：四路视频拼接输出

**配置示例：**
```cpp
OutputConfig config;
config.mode = MODE_4WAY_COMPOSE;
config.activeCameras = {0, 1, 2, 3};
config.composeStrategy.type = COMPOSE_STITCH;  // 拼接模式
config.composeStrategy.regions = {
    Rect(0, 0, 960, 540),    // 摄像头0区域
    Rect(960, 0, 960, 540),  // 摄像头1区域
    Rect(0, 540, 960, 540),  // 摄像头2区域
    Rect(960, 540, 960, 540) // 摄像头3区域
};
config.enableAI = false;
config.outputTargets = {OUTPUT_DISPLAY};
config.displayLayout.windowCount = 1;
config.displayLayout.windows = {
    {0, Rect(0, 0, 1920, 1080), -1, false}  // 单窗口显示合成帧
};
```

**数据流向：**
```
摄像头0 ─┐
摄像头1 ─┤
摄像头2 ─┼→ CameraService → [FrameQueue0-3]
摄像头3 ─┘
              ↓
        ComposeService (拼接策略)
              ↓
        [ComposedFrameQueue] (1920x1080合成帧)
              ↓
        OutputService
              ↓
        DisplayService (单窗口渲染)
              ↓
        显示设备 (单屏显示)
```

---

### 场景2：四路单独输出

**配置示例：**
```cpp
OutputConfig config;
config.mode = MODE_4WAY_SEPARATE;
config.activeCameras = {0, 1, 2, 3};
config.composeStrategy.type = COMPOSE_NONE;  // 不合成
config.enableAI = false;
config.outputTargets = {OUTPUT_DISPLAY};
config.displayLayout.windowCount = 4;
config.displayLayout.windows = {
    {0, Rect(0, 0, 960, 540), 0, false},     // 窗口0显示摄像头0
    {1, Rect(960, 0, 960, 540), 1, false},   // 窗口1显示摄像头1
    {2, Rect(0, 540, 960, 540), 2, false},   // 窗口2显示摄像头2
    {3, Rect(960, 540, 960, 540), 3, false}  // 窗口3显示摄像头3
};
```

**数据流向：**
```
摄像头0 → CameraService → [FrameQueue0] ──┐
摄像头1 → CameraService → [FrameQueue1] ──┤
摄像头2 → CameraService → [FrameQueue2] ──┼→ OutputService (多路输出)
摄像头3 → CameraService → [FrameQueue3] ──┘
                                              ↓
                                    DisplayService (多窗口管理)
                                              ↓
                                    ┌─────────┼─────────┐
                                    ↓         ↓         ↓
                                窗口0      窗口1      窗口2      窗口3
                                    ↓         ↓         ↓         ↓
                                显示设备 (4分屏显示)
```

---

### 场景3：单路+AI识别输出

**配置示例：**
```cpp
OutputConfig config;
config.mode = MODE_SINGLE_AI;
config.activeCameras = {0};                  // 只激活摄像头0
config.composeStrategy.type = COMPOSE_NONE;
config.enableAI = true;
config.aiCameras = {0};                      // AI处理摄像头0
config.outputTargets = {OUTPUT_DISPLAY};
config.displayLayout.windowCount = 1;
config.displayLayout.windows = {
    {0, Rect(0, 0, 1920, 1080), 0, true}     // 单窗口显示摄像头0+AI结果
};
```

**数据流向：**
```
摄像头0 → CameraService → [FrameQueue0]
              │
              ├─→ [VideoFrameQueue] ──┐
              │                        │
              └─→ [AIInputQueue]       │
                    ↓                  │
              AIService (推理)         │
                    ↓                  │
              [AIResultQueue]          │
                    ↓                  │
              ResultFusionService ────┘ (匹配+融合)
                    ↓
              [FusedFrameQueue] (带AI结果的帧)
                    ↓
              DisplayService (单窗口渲染+AI叠加)
                    ↓
              显示设备 (显示摄像头0+AI识别结果)
```

---

### 场景4：多路+AI识别输出（扩展场景）

**配置示例：**
```cpp
OutputConfig config;
config.mode = MODE_MULTI_AI;
config.activeCameras = {0, 1, 2, 3};
config.composeStrategy.type = COMPOSE_SPLIT_SCREEN;  // 分屏模式
config.enableAI = true;
config.aiCameras = {0, 2};                            // AI只处理摄像头0和2
config.outputTargets = {OUTPUT_DISPLAY};
config.displayLayout.windowCount = 4;
config.displayLayout.windows = {
    {0, Rect(0, 0, 960, 540), 0, true},      // 窗口0：摄像头0+AI
    {1, Rect(960, 0, 960, 540), 1, false},   // 窗口1：摄像头1（无AI）
    {2, Rect(0, 540, 960, 540), 2, true},    // 窗口2：摄像头2+AI
    {3, Rect(960, 540, 960, 540), 3, false}  // 窗口3：摄像头3（无AI）
};
```

**数据流向：**
```
摄像头0 ─┐
摄像头1 ─┤
摄像头2 ─┼→ CameraService → [FrameQueue0-3]
摄像头3 ─┘
              │
              ├─→ ComposeService (分屏合成) → [ComposedFrameQueue]
              │
              ├─→ [VideoFrameQueue0] ──┐
              ├─→ [VideoFrameQueue1]   │
              ├─→ [VideoFrameQueue2] ──┤
              └─→ [VideoFrameQueue3]   │
                                       │
              [AIInputQueue0] ──┐      │
              [AIInputQueue2] ──┤      │
                    ↓           ↓      │
              AIService (推理摄像头0和2)│
                    ↓           ↓      │
              [AIResultQueue0]  │      │
              [AIResultQueue2]  │      │
                    ↓           ↓      │
              ResultFusionService ────┘ (分别融合)
                    ↓
              [FusedFrameQueue0-3]
                    ↓
              DisplayService (4窗口渲染)
                    ↓
              显示设备 (4分屏，其中2个带AI结果)
```

---

## 配置管理服务

### ConfigManager设计

```cpp
class ConfigManager {
public:
    // 加载配置
    bool loadConfig(const char* configFile);
    bool loadConfig(OutputConfig& config);
    
    // 保存配置
    bool saveConfig(const char* configFile);
    
    // 动态切换配置
    bool switchMode(WorkMode mode);
    bool updateConfig(OutputConfig& config);
    
    // 获取当前配置
    OutputConfig& getCurrentConfig();
    
    // 配置验证
    bool validateConfig(OutputConfig& config);
};
```

### 动态切换流程

```cpp
// 切换工作模式
bool ServiceManager::switchMode(WorkMode newMode) {
    // 1. 停止当前服务
    stopAllServices();
    
    // 2. 加载新配置
    OutputConfig newConfig = configManager->getConfig(newMode);
    
    // 3. 重新配置服务
    videoService->reconfigure(newConfig);
    if (newConfig.enableAI) {
        aiService->reconfigure(newConfig);
        fusionService->reconfigure(newConfig);
    }
    displayService->reconfigure(newConfig); 
    
    // 4. 重启服务
    startAllServices();
    
    return true;
}
```

---

## 框架支持能力总结

### ✅ 支持的功能

1. **灵活配置**：通过配置结构支持多种工作模式
2. **多路输出**：支持单路/多路/合成输出
3. **AI可选**：AI功能可配置启用/禁用，可指定处理哪些摄像头
4. **多窗口显示**：DisplayService支持多窗口独立显示
5. **动态切换**：运行时可以切换工作模式
6. **资源管理**：根据配置动态分配资源

### 核心设计要点

1. **ComposeService策略化**：支持拼接/分屏/画中画/不合成等多种策略
2. **OutputService多路输出**：支持将不同数据源输出到不同目标
3. **DisplayService多窗口**：支持多窗口管理，每个窗口可独立配置数据源
4. **AIService选择性处理**：可配置处理哪些摄像头，不处理的直接跳过
5. **ResultFusionService按需融合**：只对需要AI的窗口进行融合

---

## 配置示例文件（JSON格式）

```json
{
    "workMode": "MODE_4WAY_COMPOSE",
    "activeCameras": [0, 1, 2, 3],
    "composeStrategy": {
        "type": "COMPOSE_STITCH",
        "regions": [
            {"x": 0, "y": 0, "width": 960, "height": 540},
            {"x": 960, "y": 0, "width": 960, "height": 540},
            {"x": 0, "y": 540, "width": 960, "height": 540},
            {"x": 960, "y": 540, "width": 960, "height": 540}
        ],
        "enableSync": true
    },
    "ai": {
        "enable": false,
        "cameras": [],
        "modelPath": "",
        "inferenceConfig": {}
    },
    "output": {
        "targets": ["OUTPUT_DISPLAY"],
        "displayLayout": {
            "windowCount": 1,
            "windows": [
                {
                    "windowId": 0,
                    "position": {"x": 0, "y": 0, "width": 1920, "height": 1080},
                    "sourceCameraId": -1,
                    "enableAIOverlay": false
                }
            ]
        }
    }
}
```

---

**总结**：当前框架通过配置系统支持你提到的三种场景，核心是通过配置控制ComposeService的合成策略、OutputService的输出路由、DisplayService的窗口管理，以及AIService的选择性处理。数据流会根据配置动态调整。

---

## ADAS场景处理

### ADAS系统特点

ADAS（Advanced Driver Assistance System，高级驾驶辅助系统）是车载领域的核心应用，具有以下特点：

1. **多路摄像头协同**：前视、后视、侧视摄像头
2. **实时AI识别**：车道检测、车辆检测、行人检测、交通标志识别等
3. **低延迟要求**：识别结果需要实时反馈，延迟要求<100ms
4. **告警系统**：检测到危险情况需要及时告警
5. **多模型处理**：可能需要同时运行多个AI模型（检测+分割+分类）
6. **数据记录**：可能需要记录视频和识别结果用于分析
7. **系统集成**：可能需要与车辆控制系统交互

---

### ADAS架构设计

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        应用层 (Application Layer)                        │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐     │
│  │  ADAS Controller │  │  Alert Manager   │  │  Data Recorder   │     │
│  │  (ADAS控制)      │  │  (告警管理)      │  │  (数据记录)      │     │
│  │                  │  │                  │  │                  │     │
│  │ - 场景管理      │  │ - 碰撞预警      │  │ - 视频录制        │     │
│  │ - 模式切换      │  │ - 车道偏离告警  │  │ - 事件记录        │     │
│  │ - 参数配置      │  │ - 行人检测告警  │  │ - 数据回放        │     │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘     │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        服务层 (Service Layer)                            │
│                                                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    VideoService (视频服务)                        │   │
│  │  - 前视摄像头 (Front Camera)                                     │   │
│  │  - 后视摄像头 (Rear Camera)                                      │   │
│  │  - 侧视摄像头 (Side Cameras)                                     │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                      │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │              MultiModelAIService (多模型AI服务)                   │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │ObjectDetector│  │LaneDetector  │  │SignRecognizer│          │   │
│  │  │(目标检测)     │  │(车道检测)     │  │(标志识别)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- 车辆检测    │  │- 车道线检测  │  │- 交通标志    │          │   │
│  │  │- 行人检测    │  │- 车道偏离    │  │- 限速标志    │          │   │
│  │  │- 障碍物检测  │  │- 车道保持    │  │              │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                      │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │              ADASFusionService (ADAS融合服务)                      │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │ResultFuser   │  │AlertAnalyzer │  │TrajectoryEst  │          │   │
│  │  │(结果融合)     │  │(告警分析)     │  │(轨迹估计)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- 多模型融合  │  │- 风险评估    │  │- 碰撞预测    │          │   │
│  │  │- 结果叠加    │  │- 告警生成    │  │- 路径规划    │          │   │
│  │  │- 3D融合      │  │- 优先级管理  │  │              │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                      │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    DisplayService (显示服务)                      │   │
│  │  - 主窗口：前视+AI结果                                            │   │
│  │  - 小窗口：后视/侧视                                              │   │
│  │  - OSD：告警信息、速度、状态等                                    │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

### ADAS配置示例

```cpp
// ADAS工作模式配置
struct ADASConfig {
    // 摄像头配置
    struct {
        int frontCameraId = 0;      // 前视摄像头
        int rearCameraId = 1;        // 后视摄像头
        std::vector<int> sideCameraIds = {2, 3};  // 侧视摄像头
    } cameras;
    
    // AI模型配置
    struct {
        bool enableObjectDetection = true;   // 目标检测
        bool enableLaneDetection = true;     // 车道检测
        bool enableSignRecognition = true;   // 标志识别
        bool enableSegmentation = false;     // 语义分割（可选）
        
        // 模型路径
        std::string objectDetectorModel = "/models/yolo.rknn";
        std::string laneDetectorModel = "/models/lane.rknn";
        std::string signRecognizerModel = "/models/sign.rknn";
        
        // 推理配置
        InferenceConfig objectDetectorConfig;
        InferenceConfig laneDetectorConfig;
        InferenceConfig signRecognizerConfig;
    } ai;
    
    // 告警配置
    struct {
        bool enableCollisionWarning = true;      // 碰撞预警
        bool enableLaneDepartureWarning = true;  // 车道偏离告警
        bool enablePedestrianWarning = true;     // 行人告警
        bool enableSpeedWarning = true;          // 超速告警
        
        float collisionWarningDistance = 50.0f;  // 碰撞预警距离(米)
        float laneDepartureThreshold = 0.3f;      // 车道偏离阈值
        float speedLimit = 120.0f;                // 限速(km/h)
    } alert;
    
    // 显示配置
    struct {
        DisplayLayout mainLayout;    // 主窗口布局（前视+AI）
        DisplayLayout subLayout;     // 副窗口布局（后视/侧视）
        bool showOSD = true;         // 显示OSD信息
        OSDConfig osdConfig;         // OSD配置
    } display;
    
    // 数据记录配置
    struct {
        bool enableRecording = false;     // 是否启用录制
        std::string recordPath = "/data/adas/";
        int recordDuration = 60;          // 录制时长(秒)
        bool recordOnEvent = true;        // 事件触发录制
    } recording;
};
```

---

### ADAS数据流向

#### 场景1：前视ADAS（主要场景）

```
前视摄像头 → CameraService → [FrameQueue0]
              │
              ├─→ [VideoFrameQueue] ────────────────┐
              │                                      │
              └─→ [AIInputQueue]                    │
                    │                               │
                    ├─→ ObjectDetector (车辆/行人)   │
                    ├─→ LaneDetector (车道线)        │
                    └─→ SignRecognizer (交通标志)    │
                          │                         │
                    [AIResultQueue]                 │
                          │                         │
                    ADASFusionService ──────────────┘
                    (多模型结果融合 + 告警分析)
                          │
                    [FusedFrameQueue] (融合后的帧+告警信息)
                          │
                    DisplayService
                    (主窗口：前视+检测框+车道线+告警)
                          │
                    显示设备
```

#### 场景2：多路ADAS（前视+后视+侧视）

```
前视摄像头 → CameraService → [FrameQueue0] ──┐
后视摄像头 → CameraService → [FrameQueue1] ──┤
侧视摄像头 → CameraService → [FrameQueue2-3] ┘
              │
              ├─→ ComposeService (分屏合成)
              │         │
              │         ├─→ [ComposedFrameQueue] (合成帧)
              │         │
              │         └─→ [VideoFrameQueue] (用于显示)
              │
              └─→ [AIInputQueue0] (前视AI处理)
                    │
                    ├─→ ObjectDetector
                    ├─→ LaneDetector
                    └─→ SignRecognizer
                          │
                    [AIResultQueue]
                          │
                    ADASFusionService
                    (融合 + 告警分析)
                          │
                    [FusedFrameQueue]
                          │
                    DisplayService
                    (主窗口：前视+AI | 小窗口：后视/侧视)
                          │
                    显示设备
```

---

### ADAS核心服务实现

#### 1. MultiModelAIService（多模型AI服务）

```cpp
class MultiModelAIService {
private:
    // 多个模型实例
    std::unique_ptr<ModelInterface> objectDetector;   // 目标检测模型
    std::unique_ptr<ModelInterface> laneDetector;     // 车道检测模型
    std::unique_ptr<ModelInterface> signRecognizer;   // 标志识别模型
    
    // 推理线程池
    ThreadPool inferenceThreadPool;
    
    // 结果队列
    AIResultQueue objectResultQueue;
    AIResultQueue laneResultQueue;
    AIResultQueue signResultQueue;
    
public:
    // 初始化所有模型
    bool initialize(ADASConfig& config);
    
    // 并行推理（多个模型同时处理）
    void inferenceParallel(Frame* frame, int64_t frameId);
    
    // 获取所有结果
    ADASResult getAllResults(int64_t frameId);
};

// ADAS综合结果
struct ADASResult {
    int64_t frameId;
    int64_t timestamp;
    
    // 目标检测结果
    std::vector<Detection> objects;      // 车辆、行人、障碍物
    
    // 车道检测结果
    LaneDetectionResult lanes;           // 车道线、车道偏离
    
    // 标志识别结果
    std::vector<SignDetection> signs;    // 交通标志、限速标志
    
    // 融合后的3D信息
    Scene3DInfo scene3D;                 // 3D场景信息
};
```

#### 2. ADASFusionService（ADAS融合服务）

```cpp
class ADASFusionService {
private:
    AlertAnalyzer alertAnalyzer;        // 告警分析器
    TrajectoryEstimator trajEstimator;   // 轨迹估计器
    ResultFuser resultFuser;             // 结果融合器
    
public:
    // 融合多模型结果
    FusedADASResult fuseResults(ADASResult& adasResult, Frame* frame);
    
    // 分析告警
    AlertInfo analyzeAlert(FusedADASResult& result);
    
    // 估计碰撞风险
    CollisionRisk estimateCollision(FusedADASResult& result);
};

// 融合后的ADAS结果
struct FusedADASResult {
    ADASResult adasResult;
    
    // 告警信息
    std::vector<AlertInfo> alerts;
    
    // 碰撞风险
    CollisionRisk collisionRisk;
    
    // 建议操作
    SuggestedAction suggestedAction;
};

// 告警信息
struct AlertInfo {
    AlertType type;              // 告警类型
    AlertLevel level;            // 告警级别（低/中/高）
    std::string message;         // 告警消息
    Rect region;                 // 告警区域
    float confidence;            // 置信度
    int64_t timestamp;
};

enum AlertType {
    ALERT_COLLISION_WARNING,     // 碰撞预警
    ALERT_LANE_DEPARTURE,        // 车道偏离
    ALERT_PEDESTRIAN_WARNING,    // 行人预警
    ALERT_SPEED_WARNING,         // 超速告警
    ALERT_SIGN_VIOLATION         // 标志违规
};
```

#### 3. AlertManager（告警管理器）

```cpp
class AlertManager {
private:
    std::vector<AlertInfo> activeAlerts;
    AlertHistory alertHistory;
    
public:
    // 处理告警
    void processAlert(AlertInfo& alert);
    
    // 告警优先级管理
    void prioritizeAlerts();
    
    // 告警显示
    void displayAlerts(DisplayService* display);
    
    // 告警声音/震动
    void triggerAlertFeedback(AlertInfo& alert);
    
    // 告警记录
    void recordAlert(AlertInfo& alert);
};
```

---

### ADAS显示布局

#### 典型ADAS显示布局：

```
┌─────────────────────────────────────────────────────┐
│  [主窗口：前视摄像头 + AI结果]                       │
│  ┌───────────────────────────────────────────────┐ │
│  │                                               │ │
│  │  前视画面                                     │ │
│  │  + 车辆检测框                                 │ │
│  │  + 车道线                                     │ │
│  │  + 行人检测                                   │ │
│  │  + 交通标志识别                               │ │
│  │                                               │ │
│  └───────────────────────────────────────────────┘ │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐          │
│  │后视  │  │左侧  │  │右侧  │  │OSD   │          │
│  │小窗  │  │小窗  │  │小窗  │  │信息  │          │
│  └──────┘  └──────┘  └──────┘  └──────┘          │
│                                                      │
│  [告警信息栏]                                        │
│  ⚠️ 前方车辆过近！距离：25米                         │
│  ⚠️ 车道偏离警告！                                    │
└─────────────────────────────────────────────────────┘
```

---

### ADAS完整数据流（详细）

```
┌─────────────────────────────────────────────────────────────┐
│  前视摄像头采集                                               │
└───────────────────────┬─────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────────┐
│  CameraService                                               │
│  - 采集1920x1080@30fps                                       │
│  - 输出Frame[frameId, timestamp, data]                       │
└───────────────────────┬─────────────────────────────────────┘
                        ↓
        ┌───────────────┴───────────────┐
        │                               │
        ↓                               ↓
┌───────────────┐           ┌───────────────────────┐
│VideoFrameQueue│           │   AIInputQueue         │
│(用于显示)      │           │(用于AI推理)            │
└───────────────┘           └───────────┬───────────┘
                                         ↓
                        ┌────────────────────────────────────┐
                        │  MultiModelAIService               │
                        │  ┌──────────┐  ┌──────────┐      │
                        │  │ObjectDet │  │LaneDet   │      │
                        │  │(并行推理)│  │(并行推理) │      │
                        │  └──────────┘  └──────────┘      │
                        │  ┌──────────┐                     │
                        │  │SignRecog │                     │
                        │  │(并行推理)│                     │
                        │  └──────────┘                     │
                        └───────────┬───────────────────────┘
                                    ↓
                        ┌───────────────────────┐
                        │  [AIResultQueue]     │
                        │  - ObjectResults     │
                        │  - LaneResults       │
                        │  - SignResults       │
                        └───────────┬───────────┘
                                    ↓
                        ┌────────────────────────────────────┐
                        │  ADASFusionService                 │
                        │  ┌──────────────┐                 │
                        │  │ResultFuser   │ → 融合多模型结果 │
                        │  └──────────────┘                 │
                        │  ┌──────────────┐                 │
                        │  │AlertAnalyzer │ → 分析告警      │
                        │  └──────────────┘                 │
                        │  ┌──────────────┐                 │
                        │  │TrajEstimator │ → 碰撞预测      │
                        │  └──────────────┘                 │
                        └───────────┬───────────────────────┘
                                    ↓
                        ┌───────────────────────┐
                        │  [FusedADASQueue]     │
                        │  (融合结果+告警)      │
                        └───────────┬───────────┘
                                    ↓
                        ┌────────────────────────────────────┐
                        │  ResultFusionService                │
                        │  - 匹配帧和AI结果                   │
                        │  - 绘制检测框、车道线、标志         │
                        │  - 叠加告警信息                     │
                        └───────────┬───────────────────────┘
                                    ↓
                        ┌───────────────────────┐
                        │  [FusedFrameQueue]    │
                        │  (带AI结果的视频帧)   │
                        └───────────┬───────────┘
                                    ↓
                        ┌────────────────────────────────────┐
                        │  DisplayService                    │
                        │  ┌──────────────┐                 │
                        │  │RenderEngine  │ → 渲染帧        │
                        │  └──────────────┘                 │
                        │  ┌──────────────┐                 │
                        │  │OSDRenderer   │ → 渲染OSD       │
                        │  └──────────────┘                 │
                        │  ┌──────────────┐                 │
                        │  │AlertRenderer │ → 渲染告警      │
                        │  └──────────────┘                 │
                        └───────────┬───────────────────────┘
                                    ↓
                        ┌────────────────────────────────────┐
                        │  AlertManager                      │
                        │  - 告警声音/震动                   │
                        │  - 告警记录                        │
                        └───────────┬───────────────────────┘
                                    ↓
                        ┌────────────────────────────────────┐
                        │  显示设备                           │
                        │  (主窗口+小窗口+OSD+告警)          │
                        └────────────────────────────────────┘
```

---

### ADAS性能优化

#### 1. 多模型并行推理

```cpp
// 使用线程池并行执行多个模型
void MultiModelAIService::inferenceParallel(Frame* frame, int64_t frameId) {
    // 并行执行三个模型
    auto future1 = threadPool.enqueue([this, frame]() {
        return objectDetector->inference(frame);
    });
    
    auto future2 = threadPool.enqueue([this, frame]() {
        return laneDetector->inference(frame);
    });
    
    auto future3 = threadPool.enqueue([this, frame]() {
        return signRecognizer->inference(frame);
    });
    
    // 等待所有结果
    auto objResult = future1.get();
    auto laneResult = future2.get();
    auto signResult = future3.get();
    
    // 合并结果
    ADASResult result;
    result.objects = objResult.detections;
    result.lanes = laneResult;
    result.signs = signResult;
    
    resultQueue.push(result);
}
```

#### 2. 帧率控制（AI推理不需要每帧都处理）

```cpp
// AI推理帧率控制（10fps足够）
class FrameRateController {
    int targetFps = 10;  // AI推理10fps
    int64_t lastInferenceTime = 0;
    int64_t frameInterval = 100;  // 100ms间隔
    
    bool shouldInference(int64_t currentTime) {
        if (currentTime - lastInferenceTime >= frameInterval) {
            lastInferenceTime = currentTime;
            return true;
        }
        return false;
    }
};
```

#### 3. 结果缓存和预测

```cpp
// 当AI推理慢时，使用上一帧结果
class ResultPredictor {
    ADASResult lastResult;
    int64_t lastResultTime;
    
    ADASResult getResult(int64_t currentTime) {
        // 如果当前没有新结果，使用上一帧结果（在200ms内有效）
        if (currentTime - lastResultTime < 200) {
            return lastResult;
        }
        return ADASResult();  // 返回空结果
    }
};
```

---

### ADAS告警处理流程

```
AI识别结果
    ↓
ADASFusionService分析
    ↓
┌─────────────────┐
│ 风险评估         │
│ - 距离计算       │
│ - 速度估计       │
│ - 碰撞时间计算   │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 告警生成         │
│ - 告警类型       │
│ - 告警级别       │
│ - 告警消息       │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 告警管理         │
│ - 优先级排序     │
│ - 去重处理       │
│ - 告警显示       │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 告警反馈         │
│ - 声音告警       │
│ - 震动告警       │
│ - 视觉告警       │
└────────┬────────┘
         ↓
┌─────────────────┐
│ 告警记录         │
│ - 事件记录       │
│ - 视频录制       │
│ - 数据分析       │
└─────────────────┘
```

---

### ADAS配置示例（JSON）

```json
{
    "workMode": "MODE_ADAS",
    "cameras": {
        "frontCameraId": 0,
        "rearCameraId": 1,
        "sideCameraIds": [2, 3]
    },
    "ai": {
        "enableObjectDetection": true,
        "enableLaneDetection": true,
        "enableSignRecognition": true,
        "objectDetectorModel": "/models/yolo_v8.rknn",
        "laneDetectorModel": "/models/lane_det.rknn",
        "signRecognizerModel": "/models/sign_rec.rknn",
        "inferenceFps": 10
    },
    "alert": {
        "enableCollisionWarning": true,
        "enableLaneDepartureWarning": true,
        "enablePedestrianWarning": true,
        "collisionWarningDistance": 50.0,
        "laneDepartureThreshold": 0.3,
        "speedLimit": 120.0
    },
    "display": {
        "mainLayout": {
            "windowId": 0,
            "position": {"x": 0, "y": 0, "width": 1920, "height": 1080},
            "sourceCameraId": 0,
            "enableAIOverlay": true
        },
        "subLayout": {
            "windows": [
                {"windowId": 1, "position": {"x": 1600, "y": 0, "width": 320, "height": 240}, "sourceCameraId": 1},
                {"windowId": 2, "position": {"x": 1600, "y": 240, "width": 320, "height": 240}, "sourceCameraId": 2},
                {"windowId": 3, "position": {"x": 1600, "y": 480, "width": 320, "height": 240}, "sourceCameraId": 3}
            ]
        },
        "showOSD": true,
        "osdConfig": {
            "showSpeed": true,
            "showTime": true,
            "showGPS": true
        }
    },
    "recording": {
        "enableRecording": true,
        "recordPath": "/data/adas/",
        "recordDuration": 60,
        "recordOnEvent": true
    }
}
```

---

### ADAS总结

ADAS场景的核心特点：

1. **多模型并行**：同时运行目标检测、车道检测、标志识别等多个模型
2. **实时融合**：多模型结果实时融合，生成综合ADAS结果
3. **告警系统**：基于识别结果进行风险评估和告警
4. **低延迟**：整个处理链路延迟<100ms
5. **多窗口显示**：主窗口显示前视+AI结果，小窗口显示其他视角
6. **数据记录**：支持事件触发录制，用于后续分析

框架通过MultiModelAIService、ADASFusionService、AlertManager等组件，完整支持ADAS场景的需求。

---

## 摄像头参数配置分层设计

### 摄像头参数类型

摄像头参数主要包括：

1. **基础参数**
   - 分辨率（width, height）
   - 帧率（fps）
   - 像素格式（pixel format：NV12、YUYV、RGB等）
   - 数据格式（MIPI、USB等）

2. **图像质量参数**
   - 曝光（exposure）
   - 增益（gain）
   - 白平衡（white balance）
   - 亮度（brightness）
   - 对比度（contrast）
   - 饱和度（saturation）
   - 锐度（sharpness）

3. **图像处理参数**
   - 镜像（mirror）
   - 翻转（flip）
   - 旋转（rotation）
   - 裁剪区域（crop region）

4. **高级参数**
   - 自动曝光（AE）
   - 自动增益（AGC）
   - 自动白平衡（AWB）
   - 3A算法参数（曝光、增益、白平衡的自动控制）

---

### 参数配置分层架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        应用层 (Application Layer)                        │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │              CameraConfigUI (参数配置界面)                       │   │
│  │  - 参数设置界面                                                  │   │
│  │  - 参数预览                                                      │   │
│  │  - 参数保存/加载                                                 │   │
│  │  - 参数预设管理                                                  │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        服务层 (Service Layer)                            │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │         CameraConfigService (摄像头参数配置服务)                  │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │ParamValidator │  │ParamManager │  │ParamApply    │          │   │
│  │  │(参数验证)      │  │(参数管理)     │  │(参数应用)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- 参数范围检查│  │- 参数存储    │  │- 参数下发    │          │   │
│  │  │- 参数兼容性  │  │- 参数缓存    │  │- 参数同步    │          │   │
│  │  │- 参数预设    │  │- 参数历史    │  │- 参数生效    │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                    ↓                                      │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    VideoService (视频服务)                        │   │
│  │  - CameraService (调用参数配置接口)                              │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        抽象层 (Abstraction Layer)                        │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │              CameraDevice (摄像头设备抽象)                        │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │ParamInterface│  │FormatControl │  │ControlControl│          │   │
│  │  │(参数接口)     │  │(格式控制)     │  │(控制接口)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- setParam    │  │- setFormat   │  │- setExposure │          │   │
│  │  │- getParam    │  │- getFormat   │  │- setGain     │          │   │
│  │  │- getParamRange│ │- enumFormats │  │- setWB       │          │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        驱动层 (Driver Layer)                             │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    V4L2Driver (V4L2驱动)                         │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │FormatOps     │  │ControlOps    │  │QueryOps      │          │   │
│  │  │(格式操作)     │  │(控制操作)     │  │(查询操作)     │          │   │
│  │  │              │  │              │  │              │          │   │
│  │  │- set_fmt     │  │- s_ctrl      │  │- querycap    │          │   │
│  │  │- get_fmt     │  │- g_ctrl      │  │- enum_fmt    │          │   │
│  │  │- enum_fmt    │  │- queryctrl   │  │- enum_framesizes│      │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘          │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↓
┌─────────────────────────────────────────────────────────────────────────┐
│                        硬件层 (Hardware Layer)                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    摄像头硬件                                     │   │
│  │  - MIPI CSI                                                      │   │
│  │  - USB Camera                                                   │   │
│  │  - ISP (图像信号处理)                                            │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

### 各层职责说明

#### 1. 应用层 (Application Layer)

**职责**：参数配置的用户界面和交互

```cpp
class CameraConfigUI {
public:
    // 显示参数配置界面
    void showConfigDialog(int cameraId);
    
    // 参数设置
    void setResolution(int cameraId, int width, int height);
    void setFrameRate(int cameraId, int fps);
    void setExposure(int cameraId, int exposure);
    void setGain(int cameraId, int gain);
    void setWhiteBalance(int cameraId, int wb);
    
    // 参数预览
    void previewConfig(int cameraId, CameraParams& params);
    
    // 参数保存/加载
    void savePreset(const char* presetName, CameraParams& params);
    void loadPreset(const char* presetName);
    
    // 参数预设管理
    void listPresets();
    void deletePreset(const char* presetName);
};
```

**特点**：
- 提供用户友好的配置界面
- 参数预设管理（白天/夜晚/室内/室外等）
- 参数预览功能
- 参数保存/加载

---

#### 2. 服务层 (Service Layer)

**职责**：参数管理、验证、应用

```cpp
// 摄像头参数结构
struct CameraParams {
    // 基础参数
    struct {
        int width;
        int height;
        int fps;
        PixelFormat format;
    } basic;
    
    // 图像质量参数
    struct {
        int exposure;           // 曝光值
        int gain;              // 增益值
        int whiteBalance;       // 白平衡
        int brightness;        // 亮度
        int contrast;          // 对比度
        int saturation;        // 饱和度
        int sharpness;         // 锐度
    } image;
    
    // 图像处理参数
    struct {
        bool mirror;           // 镜像
        bool flip;            // 翻转
        int rotation;         // 旋转角度
        Rect cropRegion;      // 裁剪区域
    } process;
    
    // 自动控制参数
    struct {
        bool autoExposure;     // 自动曝光
        bool autoGain;        // 自动增益
        bool autoWhiteBalance; // 自动白平衡
    } autoControl;
};

// 参数配置服务
class CameraConfigService {
private:
    ParamValidator validator;      // 参数验证器
    ParamManager paramManager;      // 参数管理器
    ParamApply paramApply;          // 参数应用器
    
public:
    // 设置参数
    bool setParams(int cameraId, CameraParams& params);
    
    // 获取参数
    bool getParams(int cameraId, CameraParams& params);
    
    // 获取参数范围
    ParamRange getParamRange(int cameraId, ParamType type);
    
    // 参数验证
    bool validateParams(int cameraId, CameraParams& params);
    
    // 参数预设
    bool savePreset(const char* name, CameraParams& params);
    bool loadPreset(const char* name, CameraParams& params);
    
    // 参数同步（多摄像头同步参数）
    bool syncParams(int sourceCameraId, std::vector<int>& targetCameraIds);
};

// 参数验证器
class ParamValidator {
public:
    // 验证参数范围
    bool validateRange(int cameraId, ParamType type, int value);
    
    // 验证参数兼容性
    bool validateCompatibility(int cameraId, CameraParams& params);
    
    // 验证参数组合
    bool validateCombination(int cameraId, CameraParams& params);
};

// 参数管理器
class ParamManager {
private:
    std::map<int, CameraParams> cameraParams;  // 每个摄像头的参数
    std::map<std::string, CameraParams> presets; // 参数预设
    
public:
    // 存储参数
    void storeParams(int cameraId, CameraParams& params);
    
    // 获取参数
    CameraParams& getParams(int cameraId);
    
    // 参数预设管理
    void savePreset(const char* name, CameraParams& params);
    CameraParams& loadPreset(const char* name);
    
    // 参数历史记录
    void saveHistory(int cameraId, CameraParams& params);
    std::vector<CameraParams> getHistory(int cameraId);
};

// 参数应用器
class ParamApply {
public:
    // 应用参数到设备
    bool applyParams(int cameraId, CameraParams& params);
    
    // 批量应用参数
    bool applyParamsBatch(std::map<int, CameraParams>& paramsMap);
    
    // 参数生效检查
    bool verifyParams(int cameraId, CameraParams& params);
};
```

**特点**：
- 参数验证：检查参数范围和兼容性
- 参数管理：存储、缓存、历史记录
- 参数应用：将参数下发到设备
- 参数预设：保存常用配置
- 参数同步：多摄像头参数同步

---

#### 3. 抽象层 (Abstraction Layer)

**职责**：统一参数接口，屏蔽硬件差异

```cpp
// 参数接口
class CameraParamInterface {
public:
    // 基础参数
    virtual bool setFormat(int width, int height, PixelFormat format, int fps) = 0;
    virtual bool getFormat(int& width, int& height, PixelFormat& format, int& fps) = 0;
    virtual bool enumFormats(std::vector<FormatInfo>& formats) = 0;
    
    // 图像质量参数
    virtual bool setExposure(int exposure) = 0;
    virtual bool getExposure(int& exposure) = 0;
    virtual bool getExposureRange(int& min, int& max, int& step) = 0;
    
    virtual bool setGain(int gain) = 0;
    virtual bool getGain(int& gain) = 0;
    virtual bool getGainRange(int& min, int& max, int& step) = 0;
    
    virtual bool setWhiteBalance(int wb) = 0;
    virtual bool getWhiteBalance(int& wb) = 0;
    virtual bool getWhiteBalanceRange(int& min, int& max, int& step) = 0;
    
    // 图像处理参数
    virtual bool setMirror(bool enable) = 0;
    virtual bool setFlip(bool enable) = 0;
    virtual bool setRotation(int angle) = 0;
    virtual bool setCropRegion(Rect& region) = 0;
    
    // 自动控制
    virtual bool setAutoExposure(bool enable) = 0;
    virtual bool setAutoGain(bool enable) = 0;
    virtual bool setAutoWhiteBalance(bool enable) = 0;
};

// V4L2实现
class V4L2Camera : public CameraDevice, public CameraParamInterface {
private:
    int fd;  // V4L2设备文件描述符
    
public:
    bool setFormat(int width, int height, PixelFormat format, int fps) override {
        struct v4l2_format fmt;
        // V4L2格式设置
        ioctl(fd, VIDIOC_S_FMT, &fmt);
        return true;
    }
    
    bool setExposure(int exposure) override {
        struct v4l2_control ctrl;
        ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
        ctrl.value = exposure;
        ioctl(fd, VIDIOC_S_CTRL, &ctrl);
        return true;
    }
    
    // ... 其他参数设置
};

// USB Camera实现
class USBCamera : public CameraDevice, public CameraParamInterface {
    // USB Camera参数设置实现
};
```

**特点**：
- 统一接口：不同硬件使用相同接口
- 参数范围查询：获取参数的有效范围
- 硬件抽象：屏蔽V4L2、USB等底层差异

---

#### 4. 驱动层 (Driver Layer)

**职责**：底层硬件参数设置

```cpp
class V4L2Driver {
public:
    // 格式操作
    int setFormat(int fd, v4l2_format* fmt);
    int getFormat(int fd, v4l2_format* fmt);
    int enumFormats(int fd, v4l2_fmtdesc* fmtdesc);
    int enumFrameSizes(int fd, v4l2_frmsizeenum* frmSize);
    
    // 控制操作
    int setControl(int fd, v4l2_control* ctrl);
    int getControl(int fd, v4l2_control* ctrl);
    int queryControl(int fd, v4l2_queryctrl* qctrl);
    int enumControls(int fd, v4l2_queryctrl* qctrl);
    
    // 流控制
    int startStreaming(int fd);
    int stopStreaming(int fd);
    int requestBuffers(int fd, v4l2_requestbuffers* req);
};
```

**特点**：
- 直接调用V4L2系统调用
- 封装底层硬件操作
- 提供基础的参数设置接口

---

### 参数配置流程

#### 流程1：用户设置参数

```
用户界面 (CameraConfigUI)
    ↓ 设置参数
CameraConfigService
    ↓ 验证参数
ParamValidator.validateParams()
    ↓ 参数有效
ParamManager.storeParams()
    ↓ 应用参数
ParamApply.applyParams()
    ↓ 调用设备接口
CameraDevice.setParams()
    ↓ V4L2系统调用
V4L2Driver.setControl()
    ↓ 硬件设置
摄像头硬件
```

#### 流程2：参数预设加载

```
用户选择预设 (CameraConfigUI)
    ↓
CameraConfigService.loadPreset()
    ↓
ParamManager.loadPreset()
    ↓ 获取预设参数
CameraParams params = presets[name]
    ↓ 应用参数
ParamApply.applyParams()
    ↓
CameraDevice.setParams()
    ↓
摄像头硬件
```

#### 流程3：参数同步（多摄像头）

```
用户设置主摄像头参数 (CameraConfigUI)
    ↓
CameraConfigService.setParams(cameraId=0, params)
    ↓ 应用主摄像头
ParamApply.applyParams(0, params)
    ↓ 同步到其他摄像头
CameraConfigService.syncParams(0, {1,2,3})
    ↓
ParamApply.applyParamsBatch({1:params, 2:params, 3:params})
    ↓
CameraDevice[1,2,3].setParams(params)
    ↓
摄像头硬件[1,2,3]
```

---

### 参数配置示例

#### 示例1：设置摄像头分辨率

```cpp
// 应用层
void CameraConfigUI::setResolution(int cameraId, int width, int height) {
    CameraParams params = configService->getParams(cameraId);
    params.basic.width = width;
    params.basic.height = height;
    
    // 验证并应用
    if (configService->validateParams(cameraId, params)) {
        configService->setParams(cameraId, params);
    }
}

// 服务层
bool CameraConfigService::setParams(int cameraId, CameraParams& params) {
    // 1. 验证参数
    if (!validator.validateParams(cameraId, params)) {
        return false;
    }
    
    // 2. 存储参数
    paramManager.storeParams(cameraId, params);
    
    // 3. 应用参数
    return paramApply.applyParams(cameraId, params);
}

// 抽象层
bool V4L2Camera::setFormat(int width, int height, PixelFormat format, int fps) {
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = convertPixelFormat(format);
    
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        return false;
    }
    
    // 设置帧率
    struct v4l2_streamparm parm;
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = fps;
    ioctl(fd, VIDIOC_S_PARM, &parm);
    
    return true;
}
```

#### 示例2：设置曝光参数

```cpp
// 应用层
void CameraConfigUI::setExposure(int cameraId, int exposure) {
    CameraParams params = configService->getParams(cameraId);
    params.image.exposure = exposure;
    params.image.autoExposure = false;  // 关闭自动曝光
    
    configService->setParams(cameraId, params);
}

// 抽象层
bool V4L2Camera::setExposure(int exposure) {
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ctrl.value = exposure;
    
    if (ioctl(fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        return false;
    }
    
    // 关闭自动曝光
    ctrl.id = V4L2_CID_EXPOSURE_AUTO;
    ctrl.value = V4L2_EXPOSURE_MANUAL;
    ioctl(fd, VIDIOC_S_CTRL, &ctrl);
    
    return true;
}
```

---

### 参数配置配置文件（JSON）

```json
{
    "cameras": [
        {
            "cameraId": 0,
            "name": "前视摄像头",
            "params": {
                "basic": {
                    "width": 1920,
                    "height": 1080,
                    "fps": 30,
                    "format": "NV12"
                },
                "image": {
                    "exposure": 100,
                    "gain": 50,
                    "whiteBalance": 5000,
                    "brightness": 128,
                    "contrast": 128,
                    "saturation": 128,
                    "sharpness": 128
                },
                "process": {
                    "mirror": false,
                    "flip": false,
                    "rotation": 0,
                    "cropRegion": {"x": 0, "y": 0, "width": 1920, "height": 1080}
                },
                "autoControl": {
                    "autoExposure": true,
                    "autoGain": true,
                    "autoWhiteBalance": true
                }
            }
        },
        {
            "cameraId": 1,
            "name": "后视摄像头",
            "params": {
                "basic": {
                    "width": 1280,
                    "height": 720,
                    "fps": 30,
                    "format": "NV12"
                },
                "image": {
                    "exposure": 80,
                    "gain": 40,
                    "whiteBalance": 5000,
                    "brightness": 120,
                    "contrast": 120,
                    "saturation": 120,
                    "sharpness": 120
                },
                "process": {
                    "mirror": true,
                    "flip": false,
                    "rotation": 0
                },
                "autoControl": {
                    "autoExposure": true,
                    "autoGain": true,
                    "autoWhiteBalance": true
                }
            }
        }
    ],
    "presets": {
        "daytime": {
            "exposure": 100,
            "gain": 50,
            "whiteBalance": 5500,
            "brightness": 128
        },
        "night": {
            "exposure": 200,
            "gain": 100,
            "whiteBalance": 4000,
            "brightness": 150
        },
        "indoor": {
            "exposure": 150,
            "gain": 70,
            "whiteBalance": 5000,
            "brightness": 140
        }
    }
}
```

---

### 参数配置总结

**分层职责**：

1. **应用层**：用户界面，参数设置交互
2. **服务层**：参数管理、验证、应用（核心层）
3. **抽象层**：统一参数接口，屏蔽硬件差异
4. **驱动层**：底层硬件参数设置（V4L2系统调用）

**关键设计**：

1. **参数验证**：服务层验证参数范围和兼容性
2. **参数管理**：服务层管理参数存储、缓存、预设
3. **参数应用**：服务层负责将参数应用到设备
4. **统一接口**：抽象层提供统一的参数接口
5. **硬件抽象**：抽象层屏蔽不同硬件的差异

**参数配置的核心在服务层**，通过CameraConfigService统一管理所有摄像头的参数配置。
```

