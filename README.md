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

此外，`Phase 3` 的第一轮文本链路已经完成最小闭环验证：

```text
font_families=235
pixel_checksum=708586963486131855
```

这表示当前 `Skia` 已经可以在 HarmonyOS 设备上从 `/system/fonts` 加载系统字体目录，并完成最小文本渲染与像素校验。

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

## 平台适配对照表

下面这张表的目标是回答两个问题：

- Linux 和 Android 已经做了哪些源码级平台适配
- OHOS 当前已经做到哪一步，还缺哪些真正进入 `Skia src/` 的平台适配

| 适配维度 | Linux 现状 | Android 现状 | OHOS 当前已实现 | OHOS 尚未实现 |
|---|---|---|---|---|
| 平台识别与 feature matrix | 已有稳定平台分支；默认启用 `freetype`、`fontconfig`、`x11`、部分 `perfetto` | 已有稳定平台分支；默认启用 Android 相关字体、NDK、`EGL` 路线 | 已建立基础 `OHOS` profile；已从 Linux 桌面分支里剥离 `x11`、`fontconfig`、`perfetto` | 还没有形成像 Linux / Android 那样长期稳定、覆盖完整能力集的独立平台矩阵 |
| 字体主机实现 | 基于 `freetype + fontconfig`，已有完整桌面字体发现路径 | 有 Android 专用 font manager 和字体配置解析路径 | 已完成 `freetype + 自定义字体目录` 的最小闭环，可读取 `/system/fonts` | 还没有 `OHOS` 专用 font manager；还没有系统字体发现、fallback、字体配置解析能力 |
| 文本 shaping | Linux 路线通常配合 `harfbuzz` 形成完整文本链路 | Android 路线也有较完整文本链路 | 当前只验证了最小文本绘制 | `harfbuzz` 尚未恢复；复杂文本 shaping、多语言排版、中英文混排能力尚未验证 |
| GPU 基础后端 | 有 `GLX/X11`、Vulkan XCB 等桌面路径 | 有 `EGL`、Vulkan、部分 Dawn/NDK 路线 | 已完成 `Ganesh + EGL/GLES` 第一轮可用性验证；ArkTS 工程已实现 `gpu_direct` | 还没有 `OHOS` 专用 window context / surface backend；还没有更系统化的 GPU 生命周期平台封装 |
| 窗口 / Surface 承载 | `tools/window/unix/*` 已有 Unix/X11 承载实现 | `tools/window/android/*` 已有 Android window context 实现 | 当前由 `skia_OHOS` 在应用侧通过 `XComponent + NativeWindow + EGL` 承接 | `Skia` 本体里还没有对标 Android / Unix 的 `tools/window/ohos/*` 或等价平台承载实现 |
| NDK / 系统桥接 | 主要依赖 POSIX / X11 / 桌面库 | 已有 `SkImageEncoder_NDK`、`SkImageGeneratorNDK`、`SkNDKConversions`、`AHardwareBuffer` 路线 | 当前仅在应用接入层使用 `NativeWindow` 和 `EGL` | 还没有 `OHOS` 原生 buffer / image / native graphics bridge 的 `Skia src/ports` 级实现 |
| 日志 / 调试 / OS glue | 已有 `stdio` / POSIX 相关实现 | 已有 `SkLog_android.cpp`、`SkDebug_android.cpp` 等 Android 专用 glue | 当前主要复用通用实现 | 还没有 `OHOS` 专用 logging / debug / OS glue 实现 |
| 构建与验证工具 | 平台特性长期稳定，工具链成熟 | Android 路径成熟，window / NDK / GPU 工具链完整 | 已补 `ohos_egl_smoke`、`ohos_text_smoke`，并通过 `lycium` 构建与真机验证 | 还没有完整的 `OHOS` viewer / dm / window tool 路线，验证入口仍然偏 smoke test |

## OHOS 已实现 vs 未实现

为了更直观看当前进展，可以再把 OHOS 单独拆成一张状态表。

| 状态 | 具体内容 |
|---|---|
| 已实现 | `OHOS` 基础平台识别 |
| 已实现 | `skia_use_ohos`、`skia_use_ohos_native_window`、`skia_use_ohos_egl`、`skia_use_ohos_gles` |
| 已实现 | Linux 桌面能力剥离：`x11`、`fontconfig`、`perfetto` |
| 已实现 | `lycium` 可构建 GPU 可用版 `libskia.so` |
| 已实现 | `ohos_egl_smoke` 可构建 |
| 已实现 | `ohos_text_smoke` 可构建 |
| 已实现 | `skia_OHOS` 已完成 `XComponent + NativeWindow + EGL/GLES + Skia GPU direct rendering` |
| 已实现 | 设备侧已验证 GPU 直连渲染 |
| 已实现 | 设备侧已验证系统字体目录加载与最小文本渲染 |
| 未实现 | `OHOS` 专用 font manager |
| 未实现 | `OHOS` 字体配置解析、系统字体发现、fallback 策略 |
| 未实现 | `harfbuzz` 恢复与复杂文本 shaping |
| 未实现 | `Skia` 本体内正式的 `OHOS window context / surface context` |
| 未实现 | `Skia src/ports` 层的 `OHOS` 原生 buffer / image bridge |
| 未实现 | `OHOS` 专用 logging / debug / OS glue |
| 未实现 | 对标 Android / Linux 的更完整平台工具和验证体系 |

## 哪些后续工作会真正进入 Skia 源码级平台适配

如果只是“把库编出来并接进应用”，很多工作会停留在 `GN/BUILD/recipe/app glue`。

但如果目标是让 `OHOS` 对标 Linux / Android 的平台适配深度，后续真正会进入 `Skia src/` 的重点主要有这些：

| 优先级 | 适配方向 | 预计进入的位置 |
|---|---|---|
| 高 | `OHOS` 专用字体管理层 | `src/ports/` |
| 高 | `harfbuzz` 文本 shaping 恢复 | `src/text/`、`modules/skshaper/`、相关构建路径 |
| 高 | `OHOS` 专用 window / surface context | `tools/window/`，必要时配合 `src/gpu/` |
| 中 | `OHOS` 原生 buffer / image bridge | `src/ports/` |
| 中 | 更稳定的 GPU 生命周期和 surface 重建逻辑 | `src/gpu/ganesh/` 附近 |
| 中 | `OHOS` 专用日志 / 调试 / OS glue | `src/ports/` |
| 低 | Vulkan / Graphite 的 OHOS 正式路线 | `src/gpu/graphite/`、Vulkan 后端 |

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

