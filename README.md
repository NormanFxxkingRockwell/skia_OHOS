# skia_OHOS

`skia_OHOS` 是一个 HarmonyOS ArkTS 验证工程，用来验证 `Skia` 在 HarmonyOS 上的当前接入结果。

相关仓库：
- ArkTS 示例工程：`https://github.com/NormanFxxkingRockwell/skia_OHOS.git`
  当前分支：`main`
- Skia 适配仓库：`https://github.com/NormanFxxkingRockwell/skia.git`
  当前适配分支：`m146-ohos`
  基线分支：`chrome/m146`

记录日期：
- `2026-03-26`

## 当前状态

当前已经完成：
- `Phase 0`：基础 OHOS 化与最小构建验证
- `Phase 1`：ArkTS 工程接入与可见渲染验证
- `Phase 2`：GPU direct rendering 真机验证
- `Phase 3`：`freetype + harfbuzz + ICU + shaped text` 真机验证
- `Phase 4`：已进入源码级平台适配，第一项 `OHOS` 专用字体管理入口已完成第一版

当前项目定位：
- 已经不是“只编译通过”
- 已经不是“只有 smoke test”
- 当前是“`Skia` 的 GPU + shaped text 已在真机 HAP 中真实落地验证”
- 但还不是完整的平台深度适配

## 阶段计划

- [x] Phase 0：基础 OHOS 化与最小构建验证
- [x] Phase 1：ArkTS 工程接入与可见渲染验证
- [x] Phase 2：GPU 直连渲染路径
- [x] Phase 3：文本与字体系统恢复
- [ ] Phase 4：OHOS 平台分支整理与稳定化
- [ ] Phase 5：更深层源码级平台适配

### Phase 4 当前进度

- [x] 4.1 整理 OHOS feature matrix 第一版
- [x] 4.2 梳理 OHOS 平台入口与边界第一版
- [x] 4.3 固化 `lycium -> smoke test -> HAP` 验证链第一版
- [x] 4.4 识别源码级平台适配点
- [x] 第一项源码级平台工作：
  `SkFontMgr_ohos`
- [x] `SkFontMgr_ohos` 已从“手读系统配置文件”切到“OHOS NativeDrawing 官方接口优先”
- [ ] 语言匹配与更细的 fallback 策略
- [ ] 第二项源码级平台工作：
  `OHOS window / surface context`

## 当前效果

### Phase 2：GPU 直连渲染

![skia_OHOS phase2 demo](docs/images/skia-ohos-demo.png)

说明：
- 当前渲染路径不是旧的“CPU 位图离屏后再贴纹理”
- 而是 `Skia` 直接通过 GPU 后端绘制到 `XComponent` 对应的渲染目标

### Phase 3：文本与多字体验证

![skia_OHOS phase3 text demo](docs/images/skia-ohos-phase3-text-demo.png)

说明：
- 当前已经完成中文文本显示
- 当前已经完成多字体样张验证
- 当前已经完成 `HarfBuzz + ICU` shaped text 路线验证

## 当前实现链路

当前 `skia_OHOS` 的真实调用链路是：

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

当前看到的内容是：
- 图形内容由 `Skia` 绘制
- 文本内容由 `Skia + HarfBuzz + ICU` shaping 后绘制
- 屏幕显示由 `XComponent + NativeWindow + EGL/GLES` 承载

## 当前适配点

### 1. 构建层适配

- 增加 `OHOS` 平台识别
- 在 `GN` 和 recipe 中引入 `skia_use_ohos`
- 剥离 Linux 桌面特性：
  - `x11`
  - `fontconfig`
  - `perfetto`
- 通过 `lycium` 固化 OHOS 主构建流程

### 2. GPU 路径适配

- 启用 `EGL/GLES`
- 跑通 `Ganesh + EGL/GLES`
- 在真机完成 `gpu_direct` 验证

### 3. 文本路径适配

- 恢复 `freetype`
- 恢复 `harfbuzz`
- 从 `bidi subset` 切换到 `ICU Unicode`
- 增加：
  - `ohos_text_smoke`
  - `ohos_shaper_smoke`
- 完成 shaped text 真机验证

### 4. 字体管理入口适配

- 新增 `SkFontMgr_ohos`
- 第一版完成了 `OHOS` 专用字体管理入口
- 当前主路径已经改成：
  `OHOS NativeDrawing 官方接口优先`
- 当前优先使用的官方接口包括：
  - `OH_Drawing_GetSystemFontConfigInfo`
  - `OH_Drawing_CreateFontParser`
  - `OH_Drawing_FontParserGetSystemFontList`
  - `OH_Drawing_FontParserGetFontByName`

当前含义：
- 系统字体目录、generic alias、fallback 信息优先来自平台接口
- `Skia` 继续使用自己的 `FreeType + HarfBuzz + ICU` 完成实际渲染
- 这已经进入 `Skia src/ports` 的源码级平台适配阶段

## 当前已实现

- `OHOS` 基础平台识别
- `lycium` 可构建 GPU + shaped text 版 `libskia.so`
- `ohos_egl_smoke`
- `ohos_text_smoke`
- `ohos_shaper_smoke`
- 真机 GPU 直连渲染验证
- 真机 shaped text 验证
- `skia_OHOS` HAP shaped text 接入
- `SkFontMgr_ohos`
- `NativeDrawing` 官方字体接口优先

## 当前未实现

- `SkFontMgr_ohos` 的语言匹配精细化
- 更完整的 fallback 策略
- `Skia` 本体中的正式 `OHOS window context / surface context`
- `src/ports` 层的 OHOS 原生 buffer / image bridge
- `OHOS` 专用 logging / debug / OS glue
- Vulkan / Graphite 的 OHOS 路线

## 相关报告

- [adaptation-plan.md](docs/reports/adaptation-plan.md)
- [deep-analysis-report.md](docs/reports/deep-analysis-report.md)
- [deep-adaptation-progress.md](docs/reports/deep-adaptation-progress.md)
- [phase4-feature-matrix.md](docs/reports/phase4-feature-matrix.md)
- [phase4-platform-boundary.md](docs/reports/phase4-platform-boundary.md)
- [phase4-validation-chain.md](docs/reports/phase4-validation-chain.md)
- [phase5-candidate-modules.md](docs/reports/phase5-candidate-modules.md)

## 当前结论

截至 `2026-03-26`：
- `Phase 2` 已完成
- `Phase 3` 已完成
- `Phase 4` 已进入源码级平台适配
- 当前最新里程碑是：
  `SkFontMgr_ohos` 已切到 OHOS 官方字体接口优先

下一步优先方向：
- 做 `SkFontMgr_ohos` 的语言匹配和 fallback 精细化
- 然后进入 `OHOS window / surface context` 的源码级平台适配
## 2026-03-26 最新同步

- 已同步最新 `libskia.so`
- 已完成 `SkFontMgr_ohos` 最新版本的 HAP 回归
- 当前字体管理已推进到：
  - `OHOS NativeDrawing` 官方接口优先
  - `bcp47` 语言感知 fallback
  - `groupName + familyName` 更细匹配

本轮 HAP 回归结果：

```text
OnSurfaceCreated
surface created size=2030x986 ready=1
RenderFrame finished frame=0 mode=gpu_direct
RenderFrame finished frame=1 mode=gpu_direct
```

这说明：
- 最新字体管理增强没有破坏 `gpu_direct`
- `skia_OHOS` 仍然可以作为当前 Phase 4 的应用侧验证工程
