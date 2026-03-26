# Skia 深度适配方案

## 1. 本轮 Phase 3 目标

- 当前处于新的 `Phase 3`，仍依据 `docs/06-code-analysis.md` 做方案分析。
- 本轮目标不再是“最小可移植”，而是 `Skia` 在 HarmonyOS/OpenHarmony 上的“深度适配方案”。
- 方案重点：
  - 显式建立 `OHOS` 平台分支，而不是继续借道 `linux`
  - 恢复文本/字体能力
  - 恢复一个可验证的 GPU 离屏后端
  - 为后续 Phase 4/5 明确源码改造边界、构建开关和验证入口

## 2. 当前仓库内已知事实

- 当前源码位于 `libs/Skia/`，任务版本对应 `chrome/m146`。
- 现有最小移植已经证明：
  - `libskia.so` 可在 HarmonyOS 交叉工具链下构建
  - 上游 CLI `sksl-minify` 可在设备侧运行
- 但该结果建立在较强裁剪之上，当前被关闭的能力包括：
  - `freetype`
  - `harfbuzz`
  - `fontconfig`
  - `egl`
  - `gl`
  - `vulkan`
  - `svg`
  - `pdf`
  - `skottie`
- 因此当前结果只能证明“Skia 核心源码和最小 CLI 在 HarmonyOS 工具链下可成立”，不能证明：
  - 字体链路可用
  - 文本 shaping 可用
  - GPU 渲染可用
  - 与 HarmonyOS 原生渲染载体的结合可用

## 3. 问题分层

### 3.1 构建层问题

- 上游主构建系统仍是 `GN + Bazel`。
- 现有最小 recipe 只是通过 `GN` 参数裁剪出一个可编译子集。
- 若要深度适配，不能继续长期依赖“关闭不兼容能力直到能编译”的策略，而是要为 `OHOS` 建立单独 profile。

### 3.2 平台分支问题

- 结合先前仓库内分析，`Skia` 的平台条件主要围绕：
  - `is_linux`
  - `is_android`
  - `is_win`
  - `is_mac`
  - `is_ios`
- 当前没有可依赖的现成 `OHOS` 平台分支证据。
- 这意味着 HarmonyOS 相关能力目前大概率被错误归入：
  - Linux 桌面分支
  - 或 Android 移动分支
- 这两者都不适合作为长期深度适配基线。

### 3.3 功能层问题

- 文本与字体链路当前被整体关闭，导致：
  - 只能验证图形库最小核心
  - 无法验证真实 UI/图形文本场景
- GPU 后端当前被整体关闭，导致：
  - 无法验证离屏或屏幕渲染上下文
  - 无法验证 `Skia` 在 HarmonyOS 图形栈中的真实位置

## 4. HarmonyOS 相关接口调研结论

本轮方案明确要求“搜索 HarmonyOS 相关接口，并对 OHOS 的分支做单独处理”。现阶段可作为深度适配依据的接口证据如下。

### 4.1 NativeWindow / XComponent 路线

- OpenHarmony `NativeWindow` 文档明确说明：
  - `NativeWindow` 是本地平台化窗口
  - 作为图形队列的生产者端
  - 可用于和 `EGL` 对接
- 相关能力与接口包括：
  - `OH_NativeWindow_NativeWindowRequestBuffer`
  - `OH_NativeWindow_NativeWindowFlushBuffer`
  - `OH_NativeWindow_NativeWindowHandleOpt`
- `XComponent` / `Native XComponent` 文档明确说明：
  - `XComponent` 的 `surface` / `texture` 类型可以承载 native 渲染
  - 可在 native 侧拿到 `OH_NativeXComponent`
  - 再由其生命周期回调中获取 `NativeWindow`
  - 进而创建 `EGL/OpenGL ES` 环境
- 这说明：对于 `Skia` 的 HarmonyOS 深度适配，`Native XComponent + NativeWindow + EGL/GLES` 是一条有官方能力依据的主路线。

### 4.2 EGL / OpenGL ES 路线

- OpenHarmony `NativeWindow` 和 `XComponent` 开发指导都明确把 `EGL/OpenGL ES` 作为原生渲染标准路径。
- 这意味着对 `Skia` 来说：
  - 首个 GPU 深度适配目标应优先选择 `EGL/GLES`
  - 不应先选 Linux 桌面式 `GLX/X11`
- 方案判断：
  - `EGL/GLES` 应作为 `OHOS` profile 下首选 GPU backend
  - `X11` 必须继续从 `OHOS` profile 中排除

### 4.3 NativeBuffer / Vulkan 路线

- OpenHarmony/OpenHarmony 生态资料显示：
  - 存在 `OH_NativeBuffer`
  - 存在 Vulkan 扩展 `VK_OHOS_external_memory`
  - 相关接口包括 `vkGetNativeBufferPropertiesOHOS`
  - 相关导入结构包括 `VkImportNativeBufferInfoOHOS`
- 这说明：
  - HarmonyOS/OpenHarmony 平台确实存在 Vulkan 与 `OH_NativeBuffer` 的结合点
  - 但这条链路的复杂度高于 `EGL/GLES`
- 方案判断：
  - `Vulkan` 适合作为第二阶段增强路线
  - 不适合作为第一阶段深度适配主线

### 4.4 字体 / 文本接口判断

- 本轮调研没有发现一个足够明确、可直接替换 `Skia` 现有 `freetype + harfbuzz` 文本链路的原生 C/C++ 平台字体栈证据。
- 虽然 OpenHarmony ArkGraphics 2D 有字体、字体集合、字体注册等文档条目，但这些能力并不足以直接证明：
  - 可以替代 `Skia` 当前跨平台字体解析与 shaping 路线
  - 或可以直接把 `Skia` 文本栈整体“替换成 HarmonyOS 原生接口”
- 方案判断：
  - 第一阶段不做“用 HarmonyOS 原生文本 API 替换 Skia 文本实现”
  - 而是优先恢复 `Skia` 自身成熟的 `freetype + harfbuzz` 路线
  - 在字体发现层或资源加载层再补 `OHOS` 特定分支

## 5. 深度适配总路线

### 5.1 总体策略

- `OHOS` 必须建立单独平台分支。
- 不再长期借用 `is_linux` 或 `is_android` 作为最终形态。
- 深度适配按三段推进：
  - 平台识别层
  - 字体文本层
  - GPU 后端层

### 5.2 平台识别层

目标：

- 在 `GN` 和必要源码中引入 `OHOS` 平台识别。
- 将 Linux 桌面能力、Android 能力、OHOS 能力显式拆分。

建议动作：

- 在 `gn/skia.gni`、`BUILD.gn` 等位置补：
  - `is_ohos`
  - `skia_use_ohos_native_window`
  - `skia_use_ohos_egl`
  - `skia_use_ohos_gles`
- 明确 `OHOS` profile 的默认能力：
  - `x11 = false`
  - `fontconfig = false`
  - `egl = true`
  - `gl = true`
  - `vulkan = false`（首阶段）
  - `freetype = true`
  - `harfbuzz = true`

### 5.3 字体文本层

目标：

- 恢复真实文本链路，而不是继续完全关闭。

推荐策略：

- 第一阶段恢复：
  - `freetype`
  - `harfbuzz`
- 第一阶段继续关闭：
  - `fontconfig`
- 字体发现与字体枚举先不追求系统级自动发现。
- 首先支持：
  - 指定字体文件加载
  - 字形栅格化
  - shaping
  - 文本绘制到 `SkSurface`

原因：

- `fontconfig` 更偏 Linux 桌面生态，不应直接作为 `OHOS` 深度适配主轴。
- `Skia` 自身 `freetype + harfbuzz` 是更稳妥的跨平台路径。

### 5.4 GPU 后端层

目标：

- 在 `OHOS` 上恢复至少一个真实 GPU backend。

推荐优先级：

1. `EGL/OpenGL ES`
2. `Vulkan`

第一阶段建议：

- 先做 `EGL/GLES` 离屏渲染
- 不做窗口系统深集成
- 不做 `viewer/dm` 全量恢复

理由：

- `NativeWindow` 和 `XComponent` 文档已明确支持 `EGL/OpenGL ES`
- `Vulkan + OH_NativeBuffer` 路线存在，但接入复杂度更高

## 6. Phase 4 建议改造范围

后续若进入 Phase 4，优先改造：

- `libs/Skia/gn/skia.gni`
- `libs/Skia/BUILD.gn`
- `libs/Skia/tools/BUILD.gn`
- `libs/Skia/src/ports/`
- `libs/Skia/src/gpu/`
- `libs/Skia/src/text/`

优先动作：

- 新增 `OHOS` 平台判断
- 把 `OHOS` 与 `linux desktop` 分离
- 恢复 `freetype`
- 恢复 `harfbuzz`
- 保持 `fontconfig` 关闭
- 增加 `NativeWindow/EGL/GLES` 相关分支与链接项

## 7. Phase 5 建议构建顺序

后续若进入 Phase 5，仍然必须 `lycium-first`。

建议按三轮收敛：

### 第 1 轮

- 建立 `OHOS` profile
- 仅恢复 `freetype`
- 验证文本渲染最小闭环

### 第 2 轮

- 恢复 `harfbuzz`
- 验证 shaping + 文本绘制

### 第 3 轮

- 恢复 `EGL/GLES`
- 做 GPU 离屏渲染验证

只有在证明 `lycium` 无法表达该构建逻辑后，才允许进入 fallback。

## 8. 测试入口建议

深度适配后，不应再只依赖 `sksl-minify`。

建议新增两个验证入口：

### 8.1 `skia_text_smoke`

验证内容：

- 加载指定字体文件
- 绘制一段文本到 `SkSurface`
- 输出 PNG 或像素摘要

### 8.2 `skia_gpu_offscreen_smoke`

验证内容：

- 初始化 `NativeWindow/EGL/GLES`
- 创建离屏上下文
- 绘制简单图元
- 回读像素并输出校验结果

## 9. 不纳入首轮深度适配的内容

本轮明确不建议直接纳入：

- `viewer` / `dm` 全量移植
- ArkUI / 窗口系统深集成
- 完整系统字体发现
- `Vulkan` 主路线
- `PDF/SVG/Skottie` 全功能恢复

原因：

- 它们明显更靠近“平台集成”而不是“三方库深度适配第一阶段”
- 会导致边界失控，影响后续 Phase 4/5 收敛

## 10. 本轮推荐结论

本轮新的深度适配目标应定义为：

- 建立 `OHOS` 独立平台分支
- 恢复 `freetype + harfbuzz` 文本链路
- 首阶段对接 `NativeWindow + EGL/OpenGL ES`
- 新增文本与 GPU 两类最小验证 binary

这比“仅编译通过”更可靠，也比“一上来就做完整窗口/UI 集成”更符合当前仓库的库级适配边界。

## 11. 参考资料

- OpenHarmony NativeWindow 开发指导：
  - https://gitee.com/openharmony/docs/blob/master/zh-cn/application-dev/graphics/native-window-guidelines.md
- OpenHarmony NativeWindow API 参考：
  - https://gitee.com/openharmony/docs/blob/4a7e35861a0ce2f0aff3d326fb739e7d1f2e510b/zh-cn/application-dev/reference/apis-arkgraphics2d/_native_window.md
- OpenHarmony XComponent / Native XComponent 指导：
  - https://gitee.com/openharmony/docs/blob/54a84aefd5b06fd937a20063d39ee73444b41344/zh-cn/application-dev/ui/napi-xcomponent-guidelines.md
  - https://gitee.com/openharmony/docs/blob/b9d587c8ea14a311e9fe588b3af9854333945139/zh-cn/application-dev/ui/arkts-common-components-xcomponent.md
- OpenHarmony NDK 开发导读：
  - https://gitee.com/openharmony/docs/blob/298422d35d69c30d0ea6ce3b280da4a880e66974/zh-cn/application-dev/napi/ndk-development-overview.md
- Vulkan `VK_OHOS_external_memory` / `OH_NativeBuffer`：
  - https://docs.vulkan.org/refpages/latest/refpages/source/vkGetNativeBufferPropertiesOHOS.html
  - https://docs.vulkan.org/refpages/latest/refpages/source/VkImportNativeBufferInfoOHOS.html
  - https://docs.vulkan.org/refpages/latest/refpages/source/OH_NativeBuffer.html
- OpenHarmony ArkGraphics 2D 字体相关条目：
  - https://gitee.com/openharmony/docs/blob/6356e770ed756392d3e5c353378a78bc7a22c7ac/en/application-dev/reference/apis-arkgraphics2d/drawing__register__font_8h.md
  - https://gitee.com/openharmony/docs/blob/a25487237ad8ba30303689958b394847434a77f8/en/application-dev/reference/apis-arkgraphics2d/drawing__font__collection_8h.md
