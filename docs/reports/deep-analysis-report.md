# Skia 深度分析报告

## 1. 报告定位

- 本文件是新的 `Phase 3` 分析补充报告。
- 它不是 `Phase 4` 的实施报告，也不是 `Phase 5` 的构建报告。
- 作用是为后续是否批准进入深度适配实施提供决策依据。

## 2. 为什么需要新的深度方案

- 现有最小移植结果只能证明：
  - `libskia.so` 可构建
  - `sksl-minify` 可运行
- 但无法证明：
  - 真实文本能力可用
  - 真实 GPU 能力可用
  - 真实 HarmonyOS 原生图形载体接入可用

因此，对 `Skia` 而言，“仅编译通过”并不足以支撑深层使用场景。

## 3. 深度适配的核心判断

### 3.1 不建议直接替换成 HarmonyOS 原生图形/文本实现

- 当前没有足够证据表明，应把 `Skia` 的核心文本和渲染实现直接整体替换为 HarmonyOS 原生接口。
- 对 `Skia` 来说，更现实的深度适配方式是：
  - 保留 `Skia` 自身跨平台实现
  - 在平台接入层增加 `OHOS` 分支
  - 在原生载体层使用 HarmonyOS/OpenHarmony 提供的 `NativeWindow`、`XComponent`、`EGL/GLES`

### 3.2 `OHOS` 分支必须单独处理

- 继续借 `linux` 只是临时构建策略，不适合深度适配。
- 深度适配必须做到：
  - `OHOS != linux desktop`
  - `OHOS != android`
- 应建立单独的 `OHOS` feature matrix。

### 3.3 文本链路应优先恢复，而不是继续关闭

- 对图形库来说，字体与文本能力是高价值能力。
- 若继续长期关闭 `freetype`、`harfbuzz`，则 `Skia` 在 HarmonyOS 上仍然只是一个被过度裁剪的子集。

### 3.4 GPU 首选 `EGL/GLES`

- 文档证据表明 OpenHarmony 原生渲染标准路径是：
  - `XComponent`
  - `NativeWindow`
  - `EGL/OpenGL ES`
- 因此首轮 GPU 深度适配不应从 `Vulkan` 起步。

## 4. 推荐的实施边界

推荐纳入：

- `OHOS` 平台 profile
- `freetype + harfbuzz`
- `NativeWindow + EGL/GLES`
- 文本和 GPU 两类 smoke tests

不推荐首轮纳入：

- `viewer` / `dm` 全量恢复
- ArkUI 深集成
- 完整窗口链路适配
- Vulkan 主路线
- 系统字体发现全量接管

## 5. 审批建议

若要推进新的深度适配实施，建议批准以下目标：

- 平台分支目标：
  - 为 `Skia` 增加显式 `OHOS` 分支
- 文本目标：
  - 恢复 `freetype + harfbuzz`
- GPU 目标：
  - 恢复 `NativeWindow + EGL/GLES` 离屏路径
- 验证目标：
  - 增加 `skia_text_smoke`
  - 增加 `skia_gpu_offscreen_smoke`

## 6. 风险提示

- 风险 1：`GN` 平台分支拆分后，现有最小 recipe 需要重新整理
- 风险 2：恢复 `freetype + harfbuzz` 后，依赖与链接复杂度会上升
- 风险 3：`EGL/GLES` 能否顺利在当前设备侧跑通，取决于 `NativeWindow` 载体接入方式
- 风险 4：若后续试图把字体发现、窗口系统、UI 集成一起推进，范围会快速失控

## 7. 结论

`Skia` 的深度适配是可做的，但应严格限定为：

- `OHOS` 独立分支
- 文本链路恢复
- `EGL/GLES` 离屏后端恢复

这是一条比“仅编译通过”更可信、又比“完整平台集成”更可收敛的中间路线。
