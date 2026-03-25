# skia_OHOS

## 项目概览

`skia_OHOS` 是一个 HarmonyOS ArkTS 示例工程，用来验证 `Skia` 在 HarmonyOS 上的当前接入结果。

当前有两个相关仓库：

- ArkTS 示例工程：`https://github.com/NormanFxxkingRockwell/skia_OHOS.git`
  - 当前使用分支：`main`
- Skia 适配仓库：`https://github.com/NormanFxxkingRockwell/skia.git`
  - 当前适配分支：`m146-ohos`
  - 基线分支：`chrome/m146`

记录日期：

- `2026-03-25`

## 当前效果

下图是当前 `skia_OHOS` 在 HarmonyOS 上的实际运行效果。

![skia_OHOS current demo](docs/images/skia-ohos-demo.png)

当前页面中看到的圆形、矩形、线段和点，已经不是“CPU 离屏位图 + 贴纹理”的旧路径，而是 `Skia` 通过 GPU 后端直接绘制到 `XComponent` 对应的 `EGL` 渲染目标。

## 当前进展

截至 `2026-03-25`，当前已经完成的是“基础鸿蒙化适配 + ArkTS 工程接入验证 + Phase 2 GPU 直连渲染验证”，但还不是完整的平台深度适配。

当前已经确认的能力：

- `Skia` 已经完成基于 `Milestone 146` 的基础 OHOS 化适配
- `lycium` 已经可以构建出带 GPU 能力的 `libskia.so`
- `skia_OHOS` 已经把 `Skia` 接进 `XComponent + NativeWindow + EGL/GLES`
- 设备侧已经确认当前渲染路径为 `gpu_direct`

设备日志中的关键结果：

```text
native xcomponent callbacks registered
OnSurfaceCreated
surface created size=2030x986 ready=1
RenderFrame finished frame=0 mode=gpu_direct
RenderFrame finished frame=1 mode=gpu_direct
```

## Skia 适配点

这一节只描述 `Skia` 本体和 `lycium` 相关的适配点，不包含 ArkTS 页面代码本身。

### 1. 平台识别适配

当前已经在 `Skia` 本体中加入了基础 `OHOS` 平台识别，而不是继续完全借用 Linux 桌面默认分支。

实际落点：

- `gn/skia.gni`

当前已做的事情：

- 增加 `skia_use_ohos = target_os == "ohos"`
- 增加：
  - `skia_use_ohos_native_window`
  - `skia_use_ohos_egl`
  - `skia_use_ohos_gles`
- 让 `OHOS` 成为一个可以单独控制 feature matrix 的平台 profile

这一步的意义是：

- 让 `OHOS` 不再被等同为普通 Linux 桌面环境
- 为后续 GPU、文本和更多平台分支适配留出明确入口

### 2. GN 默认特性开关适配

当前没有直接把桌面 Linux 的整套能力照搬到 OHOS，而是做了有选择的裁剪。

实际落点：

- `gn/skia.gni`

当前已做的事情：

- 关闭 Linux 桌面相关能力：
  - `skia_use_x11 = false` on OHOS
  - `skia_use_fontconfig = false` on OHOS
  - `skia_use_perfetto = false` on OHOS
- 保留或允许后续恢复的跨平台能力：
  - `skia_use_freetype` 在 OHOS 下允许打开
  - `skia_use_gl` 可用
  - `skia_use_egl` 可用

这一步的意义是：

- 先避开明显不适合 OHOS 的 Linux 桌面依赖
- 保证 GPU 路径和后续文本路径有继续演进的空间

### 3. GPU 路径适配

当前最关键的适配点，是让 `Skia` 可以在 OHOS 上走 `Ganesh + GL/EGL` 路径。

实际落点：

- `gn/skia.gni`
- `BUILD.gn`
- `tpc_c_cplusplus/community/skia/HPKBUILD`

当前已做的事情：

- 在 `lycium` 的 Skia recipe 中显式打开：
  - `skia_use_gl=true`
  - `skia_use_egl=true`
  - `skia_use_ohos=true`
- 继续关闭暂不需要的能力，避免依赖爆炸：
  - `vulkan`
  - `fontconfig`
  - `x11`
  - `perfetto`
  - `pdf`
  - `svg`
  - `skottie`
- 让最终构建产物里的 `libskia.so` 具备 GPU 相关符号
  - 已确认包含 `GrDirectContexts::MakeGL`
  - 已确认包含 `GrGLMakeNativeInterface`

这一步的意义是：

- 让 `Skia` 不只是“能编译成一个库”
- 而是具备真正进入 OHOS `EGL/GLES` GPU 绘制链路的基础能力

### 4. Smoke Test 适配

为了让 OHOS 适配不是“只出库不验证”，当前补了两个专门的验证入口。

实际落点：

- `BUILD.gn`
- `tools/ohos_text_smoke.cpp`
- `tools/ohos_egl_smoke.cpp`

当前已做的事情：

- 新增 `ohos_text_smoke`
  - 作为后续文本/字体链路恢复的最小入口
- 新增 `ohos_egl_smoke`
  - 用于验证 `EGL/GLES` 路径是否可在 OHOS 工具链下成立
- 在 `BUILD.gn` 中增加 `ohos_gl_test_support`
  - 把 `GL/EGL` 相关最小测试依赖接进来
  - 避免无关工具目标把依赖范围拉得过大

这一步的意义是：

- 让 OHOS 适配具备最小自检入口
- 便于后续继续验证 GPU 和文本能力，而不是只看业务工程能否偶然跑通

### 5. lycium 构建适配

当前真正用于持续编译和调试的主路径，仍然是 `ho-thirdparty-porting` 仓库中的 `lycium-first`。

实际落点：

- `tpc_c_cplusplus/community/skia/HPKBUILD`

当前已做的事情：

- 让 recipe 可以从主工作树同步当前的 `Skia` 改动
- 让 `lycium` 构建产出：
  - `libskia.so`
  - `ohos_egl_smoke`
- 确认当前 `libskia.so` 是 GPU 可用版本，而不是只支持 CPU 绘制的版本

这一步的意义是：

- 保持三方库鸿蒙化主流程不变
- 所有本体改造仍然通过 `lycium` 来验证和沉淀

### 6. ArkTS 验证工程中的接入点

虽然这一层不属于 `Skia` 本体，但它解释了为什么当前已经可以在设备上看到结果。

实际落点：

- `entry/src/main/ets/pages/Index.ets`
- `entry/src/main/ets/interface/XComponentContext.ts`
- `entry/src/main/cpp/napi_init.cpp`
- `entry/src/main/cpp/CMakeLists.txt`

当前实现的链路是：

1. ArkTS 创建 `XComponent`
2. native 拿到 `OH_NativeXComponent` 和 `NativeWindow`
3. native 创建 `EGLDisplay / EGLContext / EGLSurface`
4. `Skia` 创建 `GrDirectContext`
5. native 用当前 framebuffer 构造 `GrBackendRenderTarget`
6. `Skia` 通过 `SkSurfaces::WrapBackendRenderTarget(...)` 得到 GPU `SkSurface`
7. `SkCanvas` 直接对这个 GPU `SkSurface` 绘制
8. `flushAndSubmit(...)`
9. `eglSwapBuffers(...)`

这说明当前已经不是“只是把一个 `.so` 放进项目里”，而是已经打通了：

- `Skia GPU backend`
- `OHOS NativeWindow`
- `EGL/GLES`
- `XComponent`

### 7. 当前适配的边界

当前这些适配点，已经足够支撑：

- 基础平台识别
- 基础 feature gating
- `Ganesh + GL/EGL` 路径建立
- `XComponent` 上的 GPU 直接渲染验证

但它们还不等于完整平台深度适配。

当前仍未完成的部分包括：

- `Skia` 文本/字体系统在 OHOS 上的完整恢复
  - `freetype`
  - `harfbuzz`
  - 更完整字体加载与管理
- 更系统化的 OHOS 平台分支整理
- 更稳定的 surface 生命周期封装
- 更完整的 GPU 后端验证样例
- 更高层的窗口/图形栈集成能力

## 当前实现

当前 `skia_OHOS` 已经完成 `ArkTS + XComponent + NativeWindow/EGL + Skia GPU direct rendering` 的接入验证。

当前实现链路如下：

1. ArkTS 页面创建 `XComponent`
2. native 层拿到 `OH_NativeXComponent` 和 `NativeWindow`
3. native 层创建 `EGL` 上下文和窗口 surface
4. `Skia` 创建 `GrDirectContext`
5. `Skia` 直接把当前 framebuffer 包成 GPU `SkSurface`
6. `SkCanvas` 直接对 GPU `SkSurface` 绘制
7. 最终通过 `eglSwapBuffers` 显示到 `XComponent`

换句话说，当前看到的图形内容是：

- 图形内容本身由 `Skia` 绘制
- 屏幕显示由 `XComponent + NativeWindow + EGL/OpenGL ES` 完成

当前已经不是旧的：

- `Skia CPU bitmap -> GL texture -> XComponent`

而是现在的：

- `Skia GPU SkSurface -> eglSwapBuffers -> XComponent`

## 已验证能力

- `Skia` 可作为共享库接入 ArkTS 工程
- `XComponent` 生命周期和 native 回调链路可正常工作
- `NativeWindow + EGL` 可正常初始化
- `Skia` 可通过 `GrDirectContext` 建立 GPU 上下文
- `Skia` 可直接对 GPU render target 绘制
- 当前已经成功在 HarmonyOS 设备上看到基础图形：
  - 圆形
  - 矩形
  - 线段
  - 点

## 阶段计划

### ~~Phase 0：基础 OHOS 化与最小构建验证~~

状态：`已完成`

目标：

- ~~为 `Skia` 建立基础 OHOS 平台识别~~
- ~~跑通 `lycium` 构建~~
- ~~产出最小可验证库和工具~~

完成结果：

- ~~`m146-ohos` 分支已建立~~
- ~~`libskia.so` 已构建成功~~
- ~~`sksl-minify` 与 `ohos_egl_smoke` 已可构建~~

### ~~Phase 1：ArkTS 工程接入与可见渲染验证~~

状态：`已完成`

目标：

- ~~在 HarmonyOS ArkTS 工程中接入 `Skia`~~
- ~~打通 `XComponent -> NativeWindow -> EGL -> Skia`~~
- ~~在设备界面上看到实际图形结果~~

完成结果：

- ~~`skia_OHOS` 已完成接入~~
- ~~已看到由 `Skia` 绘制的基础图形~~
- ~~Phase 1 的渲染方式为 `Skia CPU 离屏绘制 + GL 纹理上屏`~~

### ~~Phase 2：GPU 直连渲染路径~~

状态：`已完成`

目标：

- ~~从“CPU bitmap + 贴纹理”升级到“Skia 直接驱动 GPU surface”~~
- ~~让 `Skia` 直接对接 `EGL/GLES` 渲染目标~~

完成结果：

- ~~`Skia` 已通过 `GrDirectContext + WrapBackendRenderTarget` 对 GPU surface 绘制~~
- ~~设备日志已确认当前模式为 `gpu_direct`~~
- ~~当前渲染路径已经不再依赖 CPU bitmap 中转~~

### Phase 3：文本与字体系统恢复

状态：`待开始`

目标：

- 恢复 `Skia` 在 OHOS 上的基础文本能力
- 不再只验证几何图形

计划内容：

- 恢复 `freetype`
- 恢复 `harfbuzz`
- 先支持指定字体文件加载
- 增加文本渲染 smoke test

阶段完成标准：

- 可以在 OHOS 上正确绘制文本
- 至少完成中英文基本文本显示验证

### Phase 4：OHOS 平台分支整理与稳定化

状态：`待开始`

目标：

- 把当前零散适配收敛成可维护的平台支持
- 形成更清晰的 OHOS feature matrix

计划内容：

- 收敛 `GN` 平台分支
- 整理默认开关和能力边界
- 补文档、补验证样例、补构建说明

阶段完成标准：

- OHOS 平台支持具备清晰边界
- 后续可继续演进到更完整的 GPU/文本/窗口集成

## 关键代码

ArkTS 工程中的关键文件：

- `entry/src/main/ets/pages/Index.ets`
- `entry/src/main/ets/interface/XComponentContext.ts`
- `entry/src/main/cpp/napi_init.cpp`
- `entry/src/main/cpp/CMakeLists.txt`

Skia 预编译库位置：

- `skia/prebuilt/arm64-v8a/libskia.so`

## 当前边界

目前已经跑通的是“最小可验证集成 + GPU 直连渲染验证”，还没有完成这些更深层的能力：

- `Skia` 文本与字体系统在 OHOS 上的完整适配
- 更完整的 OHOS 平台 feature matrix
- 更系统的 surface / context 生命周期封装
- 更完整的图片、文本、复杂路径和高级渲染能力验证
- 更高层的窗口系统或图形栈集成

## 当前结论

截至 `2026-03-25`，可以确认：

- `Skia` 已经完成基础 OHOS 化改造
- `Skia` 的 GPU 路径已经在 HarmonyOS 上跑通
- `skia_OHOS` 已经把 `Skia` 真正接入到 HarmonyOS ArkTS 工程中
- 当前图形结果来自 `Skia` 的 GPU 直接渲染，而不是 CPU 离屏贴纹理的旧路径
