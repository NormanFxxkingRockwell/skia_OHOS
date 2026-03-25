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

下图是当前 `skia_OHOS` 在 HarmonyOS 上的实际运行效果。图中的圆形、矩形、线段和点由 `Skia` 离屏绘制生成，再通过 `XComponent + NativeWindow + EGL/OpenGL ES` 显示到界面上。

![skia_OHOS current demo](docs/images/skia-ohos-demo.png)

## 当前进展

截至 `2026-03-25`，当前已经完成的是“基础鸿蒙化适配 + ArkTS 工程接入验证”，还不是完整平台深度适配。

`Skia` 侧已经完成：

- 基于 `Milestone 146` 建立 OHOS 适配分支 `m146-ohos`
- 在 `Skia` 本体中加入基础 OHOS 平台识别
- 调整 `GN` 默认开关，使 `OHOS` 不再直接等同于 Linux 桌面默认能力
- 关闭当前阶段不适合直接打开的 Linux 桌面相关能力
  - `x11`
  - `fontconfig`
  - `perfetto`
- 增加两个最小验证入口
  - `ohos_text_smoke`
  - `ohos_egl_smoke`
- 在 `lycium` 路径下验证过可构建出：
  - `libskia.so`
  - `sksl-minify`
  - `ohos_egl_smoke`

当前 `Skia` 本体实际落下的核心改动文件是：

- `gn/skia.gni`
- `BUILD.gn`
- `tools/ohos_text_smoke.cpp`
- `tools/ohos_egl_smoke.cpp`

## 当前实现

当前 `skia_OHOS` 工程已经完成 `ArkTS + XComponent + NativeWindow/EGL + Skia` 的接入验证，并且已经可以在 HarmonyOS 上看到图形输出。

当前实现链路如下：

1. ArkTS 页面创建 `XComponent`
2. native 层拿到 `OH_NativeXComponent` 和 `NativeWindow`
3. native 层创建 `EGL` 上下文和窗口 surface
4. `Skia` 先在 CPU bitmap 上离屏绘制
5. native 层把 bitmap 上传为 GL 纹理
6. 最终通过 `eglSwapBuffers` 显示到 `XComponent`

换句话说，当前看到的图形内容是：

- 图形内容本身由 `Skia` 绘制
- 屏幕显示由 `XComponent + NativeWindow + EGL/OpenGL ES` 完成

这不是“Skia 直接控制 HarmonyOS 原生窗口系统”的完整 GPU 后端适配，而是：

- `Skia` 作为绘图库负责离屏出图
- HarmonyOS 原生承载层负责把像素显示出来

## 已验证能力

- `Skia` 可作为共享库接入 ArkTS 工程
- `XComponent` 生命周期和 native 回调链路可正常工作
- `NativeWindow + EGL` 可正常初始化
- `Skia` 离屏绘制内容可被显示到 HarmonyOS 界面中
- 当前已经成功看到基础图形：
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
- ~~当前渲染方式为 `Skia CPU 离屏绘制 + GL 纹理上屏`~~

### Phase 2：GPU 直连渲染路径

状态：`待开始`

目标：

- 从“CPU bitmap + 贴纹理”升级到“Skia 直接驱动 GPU surface”
- 让 `Skia` 直接对接 `EGL/GLES` 渲染目标

计划内容：

- 梳理 `Ganesh + GLES` 在 OHOS 下的最小可行路径
- 研究 `SkSurface` / `GrDirectContext` / `BackendRenderTarget` 的绑定方式
- 做一个 `Skia` 直接 GPU 绘制到界面的最小样例

阶段完成标准：

- 不再依赖 CPU bitmap 中转
- `Skia` 可直接对 GPU render target 绘制

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
- 后续可继续演进到更完整 GPU/文本/窗口集成

## 关键代码

ArkTS 工程中的关键文件：

- `entry/src/main/ets/pages/Index.ets`
- `entry/src/main/ets/interface/XComponentContext.ts`
- `entry/src/main/cpp/napi_init.cpp`
- `entry/src/main/cpp/CMakeLists.txt`

Skia 预编译库位置：

- `skia/prebuilt/arm64-v8a/libskia.so`

## 当前边界

目前已经跑通的是“最小可验证集成”，还没有完成这些更深层的能力：

- `Skia` 直接面向 OHOS 的完整 GPU 后端适配
- `Skia` 字体系统在 OHOS 上的完整适配
- 面向 OHOS 原生窗口系统的完整平台后端
- 更完整的文本、图片、复杂路径和高级渲染能力验证

## 当前结论

截至 `2026-03-25`，可以确认：

- `Skia` 已经完成基础 OHOS 化改造
- `skia_OHOS` 已经能把 `Skia` 真正接入到 HarmonyOS ArkTS 工程中
- 当前可见图形来自 `Skia` 的离屏渲染结果
- 项目目标已经从“仅编译通过”推进到了“设备侧可见渲染结果”
