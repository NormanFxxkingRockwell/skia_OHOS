# skia_OHOS

## 项目概览

`skia_OHOS` 是一个 HarmonyOS ArkTS 示例工程，用来验证 `Skia` 在 HarmonyOS 上的当前接入结果。

相关仓库：

- ArkTS 示例工程：`https://github.com/NormanFxxkingRockwell/skia_OHOS.git`
  - 当前分支：`main`
- Skia 适配仓库：`https://github.com/NormanFxxkingRockwell/skia.git`
  - 当前适配分支：`m146-ohos`
  - 基线分支：`chrome/m146`

记录日期：`2026-03-25`

## 当前状态

当前已经完成：

- `Skia` 基于 `Milestone 146` 的基础 OHOS 化适配
- `lycium` 可构建带 GPU 能力和基础文本能力的 `libskia.so`
- `skia_OHOS` 已打通 `XComponent + NativeWindow + EGL/GLES + Skia GPU direct rendering`
- 真机 HAP 已完成 GPU 直连渲染验证
- 真机 HAP 已完成系统字体目录文本渲染验证
- 真机 HAP 已完成多字体样张验证

当前项目定位：

- 已经不是“只编译通过”
- 已经不是“只有命令行 smoke test”
- 目前处于“GPU + 基础文本能力已真实落地验证”的阶段
- 但还不是完整的平台深度适配

## 阶段计划

- [x] Phase 0：基础 OHOS 化与最小构建验证
- [x] Phase 1：ArkTS 工程接入与可见渲染验证
- [x] Phase 2：GPU 直连渲染路径
- [ ] Phase 3：文本与字体系统恢复
- [ ] Phase 4：OHOS 平台分支整理与稳定化
- [ ] Phase 5：更深层源码级平台适配

### Phase 0

状态：已完成

目标：
- 建立 `OHOS` 基础平台识别
- 让 `Skia` 能在 `lycium` 路径下完成最小构建

结果：
- 已增加 `skia_use_ohos`
- 已剥离 Linux 桌面能力：`x11`、`fontconfig`、`perfetto`
- 已能产出基础版 `libskia.so`

### Phase 1

状态：已完成

目标：
- 在 ArkTS 工程中把 `Skia` 接起来
- 先看到真实渲染结果

结果：
- 已接入 `XComponent`
- 已接入 `NativeWindow`
- 已建立原生渲染链路
- 已在设备上看到 `Skia` 渲染结果

### Phase 2

状态：已完成

目标：
- 从旧的 CPU 离屏贴纹理路径切到 GPU 直连路径

结果：
- 已完成 `gpu_direct`
- 当前链路为：
  - `XComponent -> NativeWindow/EGL -> GrDirectContext -> WrapBackendRenderTarget -> Skia GPU SkSurface -> eglSwapBuffers`
- 真机日志已确认：

```text
native xcomponent callbacks registered
OnSurfaceCreated
surface created size=2030x986 ready=1
RenderFrame finished frame=0 mode=gpu_direct
RenderFrame finished frame=1 mode=gpu_direct
```

### Phase 3

状态：进行中

目标：
- 恢复文本与字体系统的最小可用路径
- 完成 HAP 级实机文本验证

已完成：
- [x] `freetype` 最小路径恢复
- [x] `ohos_text_smoke` 可构建
- [x] 设备侧 `/system/fonts` 字体目录加载验证
- [x] 真机 HAP 文本渲染验证
- [x] 多字体样张验证

未完成：
- [ ] `harfbuzz` 恢复
- [ ] 复杂文本 shaping 验证
- [ ] 中英文混排 / fallback font 验证
- [ ] `OHOS` 专用 font manager
- [ ] 更正式的系统字体发现与配置解析

关键验证结果：

```text
font_families=235
pixel_checksum=708586963486131855
```

### Phase 4

状态：未开始

目标：
- 整理更系统的 `OHOS` 平台分支
- 让适配从“可跑通”走向“可维护”

计划内容：
- [ ] 整理 `GN/BUILD` 的 `OHOS` feature matrix
- [ ] 梳理 `tools/window` / `src/ports` 中的 OHOS 入口
- [ ] 规范化 smoke test 与验证链路

### Phase 5

状态：未开始

目标：
- 进入更深层的源码级平台适配

计划内容：
- [ ] `OHOS` 专用字体管理层
- [ ] `OHOS` 专用 window / surface context
- [ ] `OHOS` 原生 buffer / image bridge
- [ ] 更稳定的 GPU 生命周期和 surface 重建逻辑
- [ ] Vulkan / Graphite 的 OHOS 正式路线评估

## 当前效果

### Phase 2 GPU 直连渲染

![skia_OHOS phase2 demo](docs/images/skia-ohos-demo.png)

说明：
- 这一阶段证明 `Skia` 已经不是“CPU 离屏位图 + 贴纹理”的旧路径
- 而是直接通过 GPU 后端绘制到 `XComponent` 对应的 `EGL` 渲染目标

### Phase 3 文本与多字体验证

![skia_OHOS phase3 text demo](docs/images/skia-ohos-phase3-text-demo.png)

说明：
- 当前已经可以从 `/system/fonts` 加载系统字体
- 当前已经在真机 HAP 中验证中文文本显示
- 当前页面会同时展示多种字体样张，用于观察不同中文字体的效果是否正常

## 当前实现链路

当前 `skia_OHOS` 的真实调用链路：

1. ArkTS 页面创建 `XComponent`
2. native 拿到 `OH_NativeXComponent` 和 `NativeWindow`
3. native 创建 `EGLDisplay / EGLContext / EGLSurface`
4. `Skia` 创建 `GrDirectContext`
5. 当前 framebuffer 被包装成 `GrBackendRenderTarget`
6. `Skia` 通过 `SkSurfaces::WrapBackendRenderTarget(...)` 得到 GPU `SkSurface`
7. `SkCanvas` 直接对这个 GPU `SkSurface` 绘制
8. `flushAndSubmit(...)`
9. `eglSwapBuffers(...)`

也就是说，当前看到的内容是：

- 图形和文本内容由 `Skia` 绘制
- 屏幕显示由 `XComponent + NativeWindow + EGL/OpenGL ES` 完成

## 当前已实现

- `OHOS` 基础平台识别
- `skia_use_ohos`、`skia_use_ohos_native_window`、`skia_use_ohos_egl`、`skia_use_ohos_gles`
- Linux 桌面能力剥离：`x11`、`fontconfig`、`perfetto`
- `lycium` 可构建 GPU 可用版 `libskia.so`
- `ohos_egl_smoke` 可构建
- `ohos_text_smoke` 可构建
- 真机 GPU 直连渲染验证
- 真机文本渲染验证
- 真机多字体样张验证

## 当前未实现

- `harfbuzz` 恢复
- 复杂文本 shaping
- 中英文混排与 fallback font 验证
- `OHOS` 专用 font manager
- `OHOS` 字体配置解析、系统字体发现策略
- `Skia` 本体内正式的 `OHOS window context / surface context`
- `Skia src/ports` 层的 `OHOS` 原生 buffer / image bridge
- `OHOS` 专用 logging / debug / OS glue
- 对标 Android / Linux 的完整平台工具链

## 平台适配对照

| 适配维度 | Linux | Android | OHOS 当前状态 |
|---|---|---|---|
| 平台识别与 feature matrix | 成熟 | 成熟 | 已有第一版，仍需整理 |
| 字体主机实现 | `freetype + fontconfig` | Android 专用 font manager | 已有 `freetype + /system/fonts` 最小闭环 |
| 文本 shaping | 完整度较高 | 完整度较高 | 仍缺 `harfbuzz` |
| GPU 基础后端 | `GLX/X11`、Vulkan XCB | `EGL`、Vulkan、NDK 路线 | 已完成 `Ganesh + EGL/GLES` 第一轮可用性验证 |
| 窗口 / Surface 承载 | `tools/window/unix/*` | `tools/window/android/*` | 当前主要在应用侧通过 `XComponent + NativeWindow + EGL` 承接 |
| 原生桥接 | POSIX / 桌面库 | `AHardwareBuffer`、NDK image bridge | 尚未进入 `Skia src/ports` 正式实现 |
| 日志 / 调试 glue | 已有 | 已有 Android 专用实现 | 当前主要复用通用实现 |

## 后续真正会进入 Skia 源码级平台适配的方向

- `OHOS` 专用字体管理层
- `harfbuzz` 文本 shaping 恢复
- `OHOS` 专用 window / surface context
- `OHOS` 原生 buffer / image bridge
- 更稳定的 GPU 生命周期和 surface 重建逻辑
- `OHOS` 专用 logging / debug / OS glue
- Vulkan / Graphite 的 OHOS 路线

## 关键文件

- ArkTS 页面：`entry/src/main/ets/pages/Index.ets`
- native 接入层：`entry/src/main/cpp/napi_init.cpp`
- native 构建：`entry/src/main/cpp/CMakeLists.txt`
- 预编译库：`skia/prebuilt/arm64-v8a/libskia.so`

## 下一步

当前最合理的下一步是继续完成 `Phase 3`，优先级如下：

1. 恢复 `harfbuzz`
2. 验证复杂文本 shaping
3. 验证中英文混排与 fallback font
4. 再进入 `OHOS` 专用字体管理层