# Skia 深度适配进展

最后更新：`2026-03-30`

## 修改日志

- `2026-03-30`
  - 清理历史追加内容，改为当前快照版本
  - 收敛到当前 Phase 4 的实际状态
  - 补充 `SkFontMgr_ohos` 和 `skia_OHOS` native 重构的最新验证结论

## 当前阶段

- 当前主阶段：`Phase 4`
- 主工作树：`ho-thirdparty-porting/libs/Skia`
- 主构建路径：`lycium-first`
- 当前验证工程：`skia_OHOS`

## 当前结论

截至当前，已经完成的关键里程碑是：

- `Phase 2`
  - `Ganesh + EGL/GLES + XComponent` 的 GPU 直连渲染已在真机验证通过
- `Phase 3`
  - `freetype + harfbuzz + ICU + shaped text` 已在真机验证通过
- `Phase 4`
  - 第一项源码级平台适配已落地：`SkFontMgr_ohos`
  - 当前字体主路线已经切到：
    - `OHOS NativeDrawing` 官方接口优先
    - `bcp47` 语言感知 fallback
    - `groupName + familyName` 更细匹配

## 当前有效能力

### 1. 构建能力

- `lycium` 可稳定构建：
  - `libskia.so`
  - `ohos_egl_smoke`
  - `ohos_text_smoke`
  - `ohos_shaper_smoke`

### 2. 图形能力

- `Skia` 已可直接对 OHOS 应用的 GPU 渲染目标绘制
- 当前主路径为：
  - `XComponent`
  - `NativeWindow`
  - `EGL/GLES`
  - `GrDirectContext`
  - `WrapBackendRenderTarget`
  - `SkSurface`

### 3. 文本能力

- 已恢复：
  - `FreeType`
  - `HarfBuzz`
  - `ICU`
- 已可完成：
  - 系统字体发现
  - shaped text
  - 多字体样张验证
  - 真机 HAP 可视化验证

### 4. 字体管理能力

`SkFontMgr_ohos` 当前已完成：

- `NativeDrawing` 官方接口优先
- generic alias 解析
- fallback family 解析
- `bcp47` 语言感知 fallback
- `groupName + familyName` 更细匹配

## 当前验证结果

### 1. smoke test

当前已知正确的 `ohos_text_smoke` 结果：

```text
font_families=235
alias_harmonyos_sans=1
alias_serif=1
fallback_cjk=1
fallback_arabic_lang=1
fallback_tibetan_lang=1
pixel_checksum=18319541926308614285
```

当前已知正确的 `ohos_shaper_smoke` 结果：

```text
font_families=235
shaper=shaper_driven_wrapper
pixel_checksum=8004268475873723857
```

### 2. HAP 回归

`skia_OHOS` 当前已完成：

- HAP 打包成功
- HAP 安装成功
- Ability 启动成功
- `gpu_direct` 日志回归通过

当前已知正确日志：

```text
native xcomponent callbacks registered
OnSurfaceCreated
surface created size=2030x986 ready=1
font manager ready families=235 source=SkFontMgr_New_OHOS
RenderFrame finished frame=0 mode=gpu_direct
RenderFrame finished frame=1 mode=gpu_direct
```

## 当前工程状态

`skia_OHOS` native 侧已完成第一轮工程化拆分：

- `napi_init.cpp`
  - 只负责入口和模块注册
- `renderer/xcomponent_bridge.cpp`
  - 负责 `XComponent` 生命周期与桥接
- `renderer/skia_gpu_renderer.cpp`
  - 负责 `EGL` / `GrDirectContext` / `SkSurface`
- `renderer/skia_scene_renderer.cpp`
  - 负责图形、字体和文字场景绘制

这说明当前验证工程已经不再是“把全部逻辑堆在单文件里”的实验状态。

## 当前未完成项

- `Skia` 本体中的正式 `OHOS window / surface context`
- 更完整的 OHOS 原生 buffer / image bridge
- 更完整的 `SkFontMgr_ohos` fallback 策略
- Vulkan / Graphite 的 OHOS 路线
- 更完整的平台级 logging/debug glue

## 下一步

当前最合理的下一步是：

- 进入第二项源码级平台工作：
  - `OHOS window / surface context`

也就是把当前还主要由 `skia_OHOS` 承接的：

- `XComponent`
- `NativeWindow`
- `EGLSurface / lifecycle`

进一步向 `Skia` 本体里的正式平台入口推进。
