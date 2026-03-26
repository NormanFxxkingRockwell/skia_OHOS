# skia_OHOS

## 项目概览

`skia_OHOS` 是一个 HarmonyOS ArkTS 验证工程，用来验证 `Skia` 在 HarmonyOS 上的当前接入结果。

相关仓库：

- ArkTS 示例工程：
  - `https://github.com/NormanFxxkingRockwell/skia_OHOS.git`
  - 当前分支：`main`
- Skia 适配仓库：
  - `https://github.com/NormanFxxkingRockwell/skia.git`
  - 当前适配分支：`m146-ohos`
  - 基线分支：`chrome/m146`

记录日期：

- `2026-03-26`

## 当前状态

当前已经完成：

- `Skia` 基于 `Milestone 146` 的基础 OHOS 适配
- `lycium` 可构建带 GPU 能力和 shaped text 能力的 `libskia.so`
- `skia_OHOS` 已打通：
  - `XComponent + NativeWindow + EGL/GLES + Skia GPU direct rendering`
- 真机 HAP 已完成 GPU 直连渲染验证
- 真机 HAP 已完成基础文本验证
- 真机 HAP 已完成 `HarfBuzz + ICU` shaped text 接入验证

当前项目定位：

- 已经不是“只编译通过”
- 已经不是“只有 smoke test”
- 当前处于“GPU + 文本 shaping 已在真机 HAP 中真实落地验证”的阶段
- 但还不是完整的平台深度适配

## 阶段计划

- [x] Phase 0：基础 OHOS 化与最小构建验证
- [x] Phase 1：ArkTS 工程接入与可见渲染验证
- [x] Phase 2：GPU 直连渲染路径
- [x] Phase 3：文本与字体系统恢复
- [ ] Phase 4：OHOS 平台分支整理与稳定化
- [ ] Phase 5：更深层源码级平台适配

### ~~Phase 0：基础 OHOS 化与最小构建验证~~

状态：已完成

结果：

- 建立了 `OHOS` 基础平台识别
- `Skia` 已可通过 `lycium` 构建出最小可用的 `libskia.so`

### ~~Phase 1：ArkTS 工程接入与可见渲染验证~~

状态：已完成

结果：

- 已接入 `XComponent`
- 已接入 `NativeWindow`
- 已建立原生渲染链路
- 已在设备上看见 `Skia` 渲染结果

### ~~Phase 2：GPU 直连渲染路径~~

状态：已完成

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

### ~~Phase 3：文本与字体系统恢复~~

状态：已完成

结果：

- 已恢复 `freetype`
- 已恢复 `harfbuzz`
- 已切到 `ICU Unicode`
- 已完成 `ohos_text_smoke`
- 已完成 `ohos_shaper_smoke`
- 已完成真机 shaping 验证
- 已把 shaped text 接入 `skia_OHOS` 的 HAP

关键设备结果：

```text
font_families=235
shaper=shaper_driven_wrapper
pixel_checksum=8004268475873723857
```

### Phase 4：OHOS 平台分支整理与稳定化

状态：未开始

目标：

- 把当前“能跑通”的能力收敛成更稳定、可维护的 OHOS 平台方案
- 明确哪些内容属于构建层配置，哪些内容已经进入平台分支
- 为 Phase 5 的源码级平台适配准备清晰边界

#### Phase 4 实施清单

##### 4.1 整理 OHOS feature matrix

要做的事：

- [ ] 明确当前 OHOS 必开能力：
  - `freetype`
  - `harfbuzz`
  - `icu`
  - `egl`
  - `gl`
- [ ] 明确当前 OHOS 必关能力：
  - `fontconfig`
  - `x11`
  - `vulkan`
  - `perfetto`
  - `pdf`
  - `svg`
  - `skottie`
- [ ] 把这套能力矩阵写清楚，避免后续分散在 `HPKBUILD / BUILD.gn / skia.gni` 各处

优先检查文件：

- `libs/Skia/gn/skia.gni`
- `libs/Skia/BUILD.gn`
- `tpc_c_cplusplus/community/skia/HPKBUILD`

完成标准：

- 当前 OHOS feature matrix 有一份稳定、唯一的定义
- 重新构建时不会再依赖“记忆当前哪些开关可用”

##### 4.2 梳理 OHOS 平台入口与边界

要做的事：

- [ ] 列出当前 OHOS 实际已经使用的平台能力：
  - `XComponent`
  - `NativeWindow`
  - `EGL/GLES`
  - `/system/fonts`
- [ ] 明确哪些还在应用侧承接
- [ ] 明确哪些后续应该进入 `Skia src/`

优先检查范围：

- `libs/Skia/tools/`
- `libs/Skia/src/ports/`
- `libs/Skia/src/gpu/`
- `skia_OHOS/entry/src/main/cpp/napi_init.cpp`

完成标准：

- 当前 OHOS 接入边界清晰
- 不再混淆“Skia 本体适配”和“应用侧承载层接入”

##### 4.3 稳定验证链路

要做的事：

- [ ] 固化 `lycium -> smoke test -> HAP` 这条验证链
- [ ] 记录每一步的固定输入、输出和成功标志
- [ ] 明确 smoke test 和 HAP 各自验证的能力边界

当前涉及：

- `ohos_egl_smoke`
- `ohos_text_smoke`
- `ohos_shaper_smoke`
- `skia_OHOS` HAP

完成标准：

- 后续每次升级都能重复走这条链
- 不再依赖临时记忆命令和经验

##### 4.4 识别必须进入源码级平台适配的点

要做的事：

- [ ] 列出已经证明“只靠构建和 smoke test 不够”的模块
- [ ] 识别 Phase 5 将真正进入 `Skia src/` 的候选点

当前优先候选：

- `OHOS` 专用 font manager
- `OHOS` 专用 window / surface context
- `src/ports` 中的 OHOS 原生 glue
- 更稳定的 GPU 生命周期管理

完成标准：

- Phase 5 不再是泛泛而谈，而是有明确文件和模块边界

### Phase 5：更深层源码级平台适配

状态：未开始

目标：

- 进入更深层的 `Skia` 源码级平台适配

计划内容：

- [ ] `OHOS` 专用字体管理层
- [ ] `OHOS` 专用 window / surface context
- [ ] `OHOS` 原生 buffer / image bridge
- [ ] 更稳定的 GPU 生命周期和 surface 重建逻辑
- [ ] Vulkan / Graphite 的 OHOS 路线评估

## 当前效果

### Phase 2：GPU 直连渲染

![skia_OHOS phase2 demo](docs/images/skia-ohos-demo.png)

说明：

- 这一阶段证明 `Skia` 已经不是旧的“CPU 离屏位图 + 贴纹理”路线
- 而是直接通过 GPU 后端绘制到 `XComponent` 对应的 `EGL` 渲染目标

### Phase 3：文本与多字体验证

![skia_OHOS phase3 text demo](docs/images/skia-ohos-phase3-text-demo.png)

说明：

- 当前已经可以从 `/system/fonts` 加载系统字体
- 当前已经在真机 HAP 中验证中文文本显示
- 当前页面已经接入真正的 `HarfBuzz + ICU` shaped text 路线

## 当前实现链路

当前 `skia_OHOS` 的真实调用链路：

1. ArkTS 页面创建 `XComponent`
2. native 拿到 `OH_NativeXComponent` 和 `NativeWindow`
3. native 创建 `EGLDisplay / EGLContext / EGLSurface`
4. `Skia` 创建 `GrDirectContext`
5. 当前 framebuffer 被包装成 `GrBackendRenderTarget`
6. `Skia` 通过 `SkSurfaces::WrapBackendRenderTarget(...)` 得到 GPU `SkSurface`
7. `SkCanvas` 直接在 GPU `SkSurface` 上绘制
8. 文本部分通过 `HarfBuzz + ICU` shaping 生成 `SkTextBlob`
9. `flushAndSubmit(...)`
10. `eglSwapBuffers(...)`

也就是说，当前看到的内容是：

- 图形内容由 `Skia` 绘制
- 文本内容由 `Skia + HarfBuzz + ICU` shaping 后绘制
- 屏幕显示由 `XComponent + NativeWindow + EGL/OpenGL ES` 承接

## Skia 适配点

当前已经完成的适配点主要分成三层。

### 1. 构建层适配

- 增加 `OHOS` 平台识别
- 在 `GN` 和 recipe 中引入 `skia_use_ohos`
- 剥离 Linux 桌面能力：
  - `x11`
  - `fontconfig`
  - `perfetto`
- 通过 `lycium` 固化 OHOS 构建流程

### 2. GPU 路径适配

- 启用 `EGL/GLES`
- 在 OHOS 设备侧跑通 `gpu_direct`
- 通过 `XComponent + NativeWindow + EGL` 承接 `Skia` GPU 渲染

### 3. 文本路径适配

- 恢复 `freetype`
- 恢复 `harfbuzz`
- 从 `bidi subset` 切换到 `ICU Unicode`
- 增加：
  - `ohos_text_smoke`
  - `ohos_shaper_smoke`
- 真机完成 shaped text 验证

## 当前已实现

- `OHOS` 基础平台识别
- `lycium` 可构建 GPU + shaped text 版 `libskia.so`
- `ohos_egl_smoke`
- `ohos_text_smoke`
- `ohos_shaper_smoke`
- 真机 GPU 直连渲染验证
- 真机 shaping 验证
- `skia_OHOS` HAP shaped text 接入

## 当前未实现

- `OHOS` 专用 font manager
- `OHOS` 系统字体配置解析
- `Skia` 本体中的正式 `OHOS window context / surface context`
- `src/ports` 层的 OHOS 原生 buffer / image bridge
- `OHOS` 专用 logging / debug / OS glue
- Vulkan / Graphite 的 OHOS 正式路线

## 平台适配对照

| 适配维度 | Linux | Android | OHOS 当前状态 |
|---|---|---|---|
| 平台识别与 feature matrix | 成熟 | 成熟 | 已有第一版，仍需整理 |
| 字体系统 | `freetype + fontconfig` | Android 专用 font manager | 已完成 `freetype + harfbuzz + ICU + /system/fonts` 路线 |
| 文本 shaping | 完整 | 完整 | 已完成第一轮 shaping 验证 |
| GPU 基础后端 | `GLX/X11`、Vulkan | `EGL`、Vulkan、NDK 路线 | 已完成 `Ganesh + EGL/GLES` |
| 窗口 / Surface 承载 | `tools/window/unix/*` | `tools/window/android/*` | 当前主要通过 `XComponent + NativeWindow + EGL` 承接 |
| 原生桥接 | POSIX / 桌面 | `AHardwareBuffer`、NDK image bridge | 尚未进入 `Skia src/ports` 正式实现 |
| 日志 / 调试 glue | 已有 | 已有 Android 专用实现 | 当前仍主要复用通用实现 |

## 关键文件

- ArkTS 页面：[Index.ets](/D:/codeBase/skia_OHOS/entry/src/main/ets/pages/Index.ets)
- native 接入层：[napi_init.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/napi_init.cpp)
- native 构建：[CMakeLists.txt](/D:/codeBase/skia_OHOS/entry/src/main/cpp/CMakeLists.txt)
- 预编译库：[libskia.so](/D:/codeBase/skia_OHOS/skia/prebuilt/arm64-v8a/libskia.so)
- 构建说明：[build.md](/D:/codeBase/skia_OHOS/build.md)

## 下一步

当前最合理的下一步是进入 `Phase 4`：

1. 整理 `OHOS` 平台分支和 feature matrix
2. 收敛当前 `lycium + smoke test + HAP` 的稳定链路
3. 识别真正需要进入 `Skia src/` 的 OHOS 平台适配点
