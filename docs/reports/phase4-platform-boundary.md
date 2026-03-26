# Skia Phase 4 Platform Boundary

## 日期

- 2026-03-26

## 目标

- 明确当前 OHOS 路线中，哪些能力已经属于 `Skia` 本体适配
- 明确哪些能力仍然由应用侧承接
- 为后续 `Phase 5` 划清源码级平台适配边界

## 当前已经进入 Skia 适配范围的内容

### 1. 构建层平台识别

当前已经进入 `Skia` 适配范围：

- `skia_use_ohos`
- `skia_use_ohos_native_window`
- `skia_use_ohos_egl`
- `skia_use_ohos_gles`

相关位置：

- `libs/Skia/gn/skia.gni`
- `libs/Skia/BUILD.gn`

说明：

- 这些内容虽然主要还在构建系统层，但已经属于 `Skia` 对 OHOS 平台的正式识别入口

### 2. OHOS smoke tests

当前已经进入 `Skia` 适配范围：

- `ohos_egl_smoke`
- `ohos_text_smoke`
- `ohos_shaper_smoke`

相关位置：

- `libs/Skia/tools/ohos_egl_smoke.cpp`
- `libs/Skia/tools/ohos_text_smoke.cpp`
- `libs/Skia/tools/ohos_shaper_smoke.cpp`

说明：

- 这些入口虽然是测试目标，但它们已经构成当前 OHOS 平台能力的最小验证面

### 3. 文本能力恢复

当前已经进入 `Skia` 适配范围：

- `freetype`
- `harfbuzz`
- `icu`

说明：

- 这是当前 OHOS 文本链路成立的基础
- 但目前仍然主要是“用现有跨平台能力在 OHOS 上跑通”
- 还不是 `OHOS` 专用字体平台实现

## 当前仍由应用侧承接的内容

### 1. 窗口承载

当前主要由应用侧 `skia_OHOS` 承接：

- `XComponent`
- `OH_NativeXComponent`
- `NativeWindow`

说明：

- 当前 `Skia` 本体并没有自己的 `OHOS window context`
- 这些窗口和生命周期入口目前都在 ArkTS/native 应用层

### 2. EGL 上下文创建与 surface 生命周期

当前主要由应用侧 `skia_OHOS` 承接：

- `EGLDisplay`
- `EGLContext`
- `EGLSurface`
- `OnSurfaceCreated / Changed / Destroyed`

说明：

- 当前 `Skia` 只是消费已经建立好的 GPU render target
- 还没有把这部分抽回 `Skia src/` 形成正式平台实现

### 3. HAP 打包、安装和可视化验证

当前主要由应用侧承接：

- `hvigor assembleHap`
- HAP 安装
- HAP 拉起
- 界面级文本和图形可视化验证

说明：

- 这些属于验证工程能力
- 不属于 `Skia` 本体平台代码

## 当前还没有进入 Skia 本体的关键能力

以下内容是后续真正会进入 `Skia src/` 的候选点：

### 1. OHOS 专用 font manager

当前状态：

- 尚未实现

原因：

- 当前仍依赖 `/system/fonts` 和文件字体加载
- 还没有把 OHOS 系统字体发现、fallback、配置解析做成平台实现

### 2. OHOS 专用 window / surface context

当前状态：

- 尚未实现

原因：

- 当前 `XComponent + NativeWindow + EGL` 逻辑都在应用侧
- `Skia` 本体内部还没有正式的 OHOS 平台 window context

### 3. OHOS 原生 buffer / image bridge

当前状态：

- 尚未实现

原因：

- 当前还没有进入 `src/ports` 层去承接 OHOS 原生 buffer 和 image 相关桥接

### 4. OHOS 专用 logging / debug / OS glue

当前状态：

- 尚未实现

原因：

- 当前仍主要复用通用实现，没有形成专门的 OHOS 平台 glue

## 当前边界结论

到 `2026-03-26` 为止，可以这样理解：

- `Skia` 已经完成：
  - OHOS 构建层识别
  - GPU 直连渲染能力验证
  - shaped text 能力验证

- `Skia` 还没有完成：
  - OHOS 专用字体平台实现
  - OHOS 专用窗口 / surface 平台实现
  - `src/ports` 层正式平台 glue

因此，当前阶段更准确的定位是：

- `Skia on OHOS` 已经具备稳定的功能验证基础
- 但“平台实现权责”仍然有一部分留在应用侧
- `Phase 5` 的核心任务，就是把这些平台职责逐步从验证工程转回 `Skia` 平台层
