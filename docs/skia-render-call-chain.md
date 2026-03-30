# Skia 到 HarmonyOS 应用的完整调用链

最后更新：`2026-03-30`

## 修改日志

- `2026-03-30`
  - 新增当前版本调用链文档
  - 按 native 重构后的代码结构重新梳理图形链路、文字链路和桥接链路

本文基于当前两个仓库的实际代码梳理：

- `skia_OHOS`：ArkTS 验证工程
- `skia`：`m146-ohos` 分支上的 OHOS 适配版 `Skia`

本文目标是把当前“Skia 如何把图形和字体渲染到 HarmonyOS 应用上”这件事，从底层到上层完整串起来。

## 1. 总览

当前链路不是：

- `Skia CPU 位图离屏绘制 -> OpenGL 贴纹理 -> XComponent`

而是：

- `Skia GPU 直绘 -> EGLSurface -> XComponent`

当前文字链路也不是：

- 应用手写字体路径 + 直接 drawString 的临时方案

而是：

- `SkFontMgr_ohos -> NativeDrawing 官方字体接口优先 -> FreeType + HarfBuzz + ICU -> Shaped Text -> GPU SkSurface`

可以把当前系统拆成 4 层：

1. HarmonyOS 应用承载层  
   `ArkTS + XComponent + NativeWindow`

2. native 图形上下文层  
   `EGLDisplay + EGLContext + EGLSurface`

3. Skia GPU 渲染层  
   `GrDirectContext + WrapBackendRenderTarget + SkSurface + SkCanvas`

4. Skia 字体与文本层  
   `SkFontMgr_ohos + FreeType + HarfBuzz + ICU + SkTextBlob`

## 2. 自下而上的图形渲染链

这一部分先不看文字，只看图形是怎么从 `Skia` 画到 HarmonyOS 窗口里的。

### 2.1 底层承载对象

HarmonyOS 应用侧真正承载画面的对象是：

- `XComponent`
- `NativeWindow`
- `EGLSurface`

它们在 `skia_OHOS` 的 native 模块中被拿到并初始化。

关键文件：

- [Index.ets](/D:/codeBase/skia_OHOS/entry/src/main/ets/pages/Index.ets)
- [napi_init.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/napi_init.cpp)
- [xcomponent_bridge.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/renderer/xcomponent_bridge.cpp)
- [skia_gpu_renderer.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/renderer/skia_gpu_renderer.cpp)
- [skia_scene_renderer.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/renderer/skia_scene_renderer.cpp)

### 2.2 EGL 图形上下文建立

`skia_gpu_renderer.cpp` 里的 `InitEgl(RendererState& state)` 负责建立：

- `EGLDisplay`
- `EGLContext`
- `EGLSurface`

关键步骤：

1. `eglGetDisplay(EGL_DEFAULT_DISPLAY)`
2. `eglInitialize(...)`
3. `eglChooseConfig(...)`
4. `eglBindAPI(EGL_OPENGL_ES_API)`
5. `eglCreateContext(...)`
6. `eglCreateWindowSurface(...)`
7. `eglMakeCurrent(...)`

完成后，当前线程就拥有了一个可用的 GLES 渲染上下文。

### 2.3 Skia GPU 上下文建立

EGL 就绪后，Skia 开始接管 GPU 渲染：

1. 调用 `GrDirectContexts::MakeGL()`
2. 创建 `GrDirectContext`

对应代码在：

- [skia_gpu_renderer.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/renderer/skia_gpu_renderer.cpp)

这个对象是 Skia Ganesh GL 后端的核心入口。

### 2.4 把 HarmonyOS 当前 framebuffer 包成 Skia 的 GPU 渲染目标

函数：

- `RecreateSkSurface(RendererState& state)`

这里做的事情是：

1. 用 `glGetIntegerv(GL_FRAMEBUFFER_BINDING, ...)` 取当前 framebuffer
2. 读取 `GL_STENCIL_BITS`
3. 读取 `GL_SAMPLES`
4. 组装 `GrGLFramebufferInfo`
5. 调用 `GrBackendRenderTargets::MakeGL(...)`
6. 调用 `SkSurfaces::WrapBackendRenderTarget(...)`

这一步很关键，它把“当前 EGL 窗口背后的 GPU 渲染目标”包装成了 Skia 可直接绘制的 `SkSurface`。

也就是说，当前 Skia 不是自己去创建一个额外纹理再中转，而是直接对应用的实际渲染目标绘制。

### 2.5 图形绘制发生在哪里

函数：

- `DrawGpuScene(RendererState& state)`

绘制过程是：

1. `SkCanvas* canvas = state.skSurface->getCanvas();`
2. `canvas->clear(...)`
3. `canvas->drawRoundRect(...)`
4. `canvas->drawCircle(...)`
5. `canvas->drawString(...)`
6. `canvas->drawTextBlob(...)`

这里的几何图形，比如：

- 背景卡片
- banner
- 圆点

都由 `SkCanvas` 直接绘制到 GPU `SkSurface` 上。

### 2.6 图形最终怎么显示到应用上

绘制结束后：

1. 调用 `state.directContext->flushAndSubmit(...)`
2. 调用 `eglSwapBuffers(state.display, state.surface)`

于是 GPU front/back buffer 交换，内容进入 `XComponent` 对应窗口。

这一层的完整链路可以写成：

`NativeWindow -> EGLSurface -> GrDirectContext -> BackendRenderTarget -> SkSurface -> SkCanvas -> flushAndSubmit -> eglSwapBuffers`

## 3. 自下而上的文字渲染链

文字链路比图形多了一层字体管理和 shaping。

### 3.1 当前不是应用自己管理字体

当前应用侧 `skia_OHOS` 不再承担主要字体管理职责。

应用只做两件事：

1. 调用 `SkFontMgr_New_OHOS()`
2. 用这个字体管理器拿默认字体或按文件取字体

关键函数：

- `EnsureTypeface(RendererState& state)`

对应代码在：

- [skia_scene_renderer.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/renderer/skia_scene_renderer.cpp)

### 3.2 `SkFontMgr_New_OHOS()` 进入 Skia 本体

`SkFontMgr_New_OHOS()` 定义在：

- [SkFontMgr_ohos.h](/D:/codeBase/skia/include/ports/SkFontMgr_ohos.h)

实现位于：

- [SkFontMgr_ohos.cpp](/D:/codeBase/skia/src/ports/SkFontMgr_ohos.cpp)

它是当前真正的 OHOS 字体管理入口。

### 3.3 `SkFontMgr_ohos` 如何拿系统字体信息

当前策略是：

- `OHOS NativeDrawing` 官方接口优先
- 环境变量或显式目录只作为覆盖路径

主要流程在 `load_ohos_font_config(...)` 中：

1. 如果外部显式传入目录，则优先使用该目录
2. 否则检查 `SKIA_OHOS_FONT_DIR`
3. 如果没有覆盖路径，则调用：
   - `load_native_drawing_font_config(...)`
   - `load_native_drawing_font_parser(...)`

### 3.4 `NativeDrawing` 官方接口如何被使用

在 `SkFontMgr_ohos.cpp` 中，当前优先使用这些 HarmonyOS 官方接口：

- `OH_Drawing_GetSystemFontConfigInfo`
- `OH_Drawing_CreateFontParser`
- `OH_Drawing_FontParserGetSystemFontList`
- `OH_Drawing_FontParserGetFontByName`

这些接口提供的信息包括：

- 系统字体目录
- generic alias
- fallback group
- family 信息

Skia 在这一层把它们转成自己的配置数据结构：

- `OhosFontConfigData`
- `OhosGenericAlias`
- `OhosFallbackEntry`

### 3.5 系统字体文件如何进入 Skia

`SkFontMgr_ohos` 并不是自己实现一套全新的字形解析器，而是：

1. 通过 `NativeDrawing` 拿到系统字体配置和目录
2. 用 `OhosSystemFontLoader` 遍历字体目录
3. 扫描 `.ttf/.ttc/.otf/.pfb`
4. 用 `SkFontScanner` 建立字体家族
5. 最终构造 `SkFontMgr_Custom`

也就是说，OHOS 平台适配负责：

- 告诉 Skia 去哪里找字体
- alias/fallback 怎么组织

真正的字体扫描和字体对象构建，仍然复用 Skia 自身已有机制。

### 3.6 family / fallback 是怎么匹配的

`SkFontMgr_OHOS::onMatchFamilyStyleCharacter(...)` 是当前最关键的字符匹配入口。

它的匹配顺序是：

1. 先匹配显式请求的 family
2. 再匹配 `groupName + familyName` 组合的 OHOS fallback entry
3. 再做特定字符 fallback
4. 最后走通用 fallback family 列表

当前已经支持：

- `bcp47` 语言感知 fallback
- `groupName + familyName` 更细匹配
- 特定字符补充 fallback

对应的内部逻辑主要是：

- `language_matches(...)`
- `group_matches_requested_family(...)`
- `match_configured_family_style(...)`
- `special_fallbacks(...)`

### 3.7 文字 shaping 是怎么发生的

应用侧文字不是直接 `drawString` 完事，而是优先走 shaped text。

关键函数：

- `MakeShapedBlob(...)`

调用链如下：

1. `SkUnicodes::ICU::Make()`
2. `SkShapers::HB::ShaperDrivenWrapper(unicode, fontMgr)`
3. 创建 `SkFont`
4. 创建：
   - `BiDiRunIterator`
   - `LanguageRunIterator`
   - `ScriptRunIterator`
   - `FontRunIterator`
5. 调用 `shaper->shape(...)`
6. 通过 `SkTextBlobBuilderRunHandler` 生成 `SkTextBlob`

这里真正负责 shaping 的是：

- `ICU`
- `HarfBuzz`
- `SkShaper`

### 3.8 shaped text 最终如何绘制

在 `DrawGpuScene(...)` 里，文字样张通过 `drawSample(...)` 画出来。

内部流程是：

1. 调用 `MakeShapedBlob(...)`
2. 如果成功，执行：
   - `canvas->drawTextBlob(blob.get(), x, y, paint)`
3. 如果失败，退回普通 `drawString("shaping unavailable", ...)`

所以当前在界面上看到的中文文本，主路径实际上是：

`SkFontMgr_ohos -> SkTypeface -> ICU/HarfBuzz shaping -> SkTextBlob -> SkCanvas::drawTextBlob`

## 4. 从 ArkTS 到 native 的上层触发链

前两节讲的是底层是怎么画的，这里看上层是怎么触发的。

### 4.1 ArkTS 页面创建 `XComponent`

文件：

- [Index.ets](/D:/codeBase/skia_OHOS/entry/src/main/ets/pages/Index.ets)

关键动作：

1. 页面创建 `XComponent`
2. `libraryname` 指向 native 模块 `entry`

### 4.2 `XComponent` 加载后调用 native 方法

在 `onLoad` 回调里：

1. 保存 `xComponentContext`
2. 调用 `registerSurface()`
3. 调用 `renderNow()`

按钮点击时也会再次调用：

- `renderNow()`

### 4.3 NAPI 导出的方法如何进入 native

`entry` 模块在 `napi_init.cpp` 中导出：

- `registerSurface`
- `renderNow`

注册逻辑在：

- `Init(...)`
- `napi_define_properties(...)`

### 4.4 `registerSurface()` 做了什么

`registerSurface()` 最核心的是：

1. 从 ArkTS 传下来的 holder 上拿到 `OH_NATIVE_XCOMPONENT_OBJ`
2. 调用 `OH_NativeXComponent_RegisterCallback(...)`
3. 注册回调：
   - `OnSurfaceCreated`
   - `OnSurfaceChanged`
   - `OnSurfaceDestroyed`
   - `DispatchTouchEvent`

这些桥接逻辑现在实际位于：

- [xcomponent_bridge.cpp](/D:/codeBase/skia_OHOS/entry/src/main/cpp/renderer/xcomponent_bridge.cpp)

### 4.5 surface 生命周期如何触发真正渲染

当 HarmonyOS 创建 surface 后，回调进入：

- `OnSurfaceCreated(...)`

这个回调里做的事是：

1. 保存 `component` 和 `nativeWindow`
2. 读取 `XComponent` 尺寸
3. 调用 `InitEgl(...)`
4. 初始化成功后调用 `RenderFrame(...)`

当尺寸变化时：

- `OnSurfaceChanged(...)`

会重建 `SkSurface` 并重新渲染。

当点击或主动重绘时：

- `RenderNow(...)`

也会进入 `RenderFrame(...)`

## 4.6 当前 native 模块的职责分层

当前 `skia_OHOS` native 侧已经做过一次工程化拆分，不再把所有逻辑塞进一个入口文件。

当前分层是：

1. `napi_init.cpp`
   - 只负责模块注册与导出绑定
2. `xcomponent_bridge.cpp`
   - 负责 `XComponent` 生命周期、surface 回调和桥接
3. `skia_gpu_renderer.cpp`
   - 负责 `EGL` 初始化、GPU `SkSurface` 包装和帧提交
4. `skia_scene_renderer.cpp`
   - 负责图形绘制、字体初始化和 shaped text 绘制
5. `renderer_state.h`
   - 负责共享渲染状态

## 5. 图形链路和文字链路的区别

虽然图形和文字最终都落到 `SkCanvas`，但它们进入 `SkCanvas` 的路径不同。

### 5.1 图形链路

图形链路比较直接：

`ArkTS/XComponent -> NativeWindow/EGL -> GrDirectContext -> SkSurface -> SkCanvas::drawXXX -> eglSwapBuffers`

它主要依赖：

- EGL/GLES
- Skia GPU backend

### 5.2 文字链路

文字链路更长：

`ArkTS/XComponent -> NativeWindow/EGL -> GrDirectContext -> SkSurface -> SkFontMgr_New_OHOS -> NativeDrawing 字体配置 -> FreeType/HarfBuzz/ICU -> SkTextBlob -> SkCanvas::drawTextBlob -> eglSwapBuffers`

它除了依赖 GPU 之外，还依赖：

- `SkFontMgr_ohos`
- `NativeDrawing`
- `FreeType`
- `HarfBuzz`
- `ICU`

## 6. 当前实现的边界

当前已经做到：

- `Skia` 直接对 OHOS 应用的 GPU 渲染目标绘制
- `Skia` 文字渲染已走 shaped text 主路径
- 系统字体信息优先来自 HarmonyOS 官方字体接口
- `SkFontMgr_ohos` 已进入 `Skia src/ports` 源码级平台适配

当前还没做到：

- `Skia` 本体里的正式 `OHOS window / surface context`
- 更完整的原生 buffer / image bridge
- Vulkan / Graphite 的 OHOS 路线
- 更完整的平台级 logging/debug glue

## 7. 一句话总结

当前这套链路可以概括为：

- 图形是 `Skia GPU backend` 直接画到 `XComponent` 对应的 `EGLSurface`
- 文字是 `SkFontMgr_ohos + NativeDrawing + FreeType + HarfBuzz + ICU` 先完成字体选择与 shaping，再由 `Skia` 画到同一个 GPU `SkSurface`

所以现在的 `skia_OHOS` 已经不是“应用自己画，Skia 只参与一点点”，而是：

- 应用负责承载窗口和触发渲染
- `Skia` 负责真正的图形绘制和文字绘制
- `OHOS` 平台接口负责把字体和渲染目标接给 `Skia`
