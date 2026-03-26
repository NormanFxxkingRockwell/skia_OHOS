# Skia Phase 4 Feature Matrix

## 日期

- 2026-03-26

## 目标

- 收敛当前 OHOS 路径下真正有效的 feature matrix
- 明确哪些能力已经进入稳定配置
- 明确哪些能力当前仍然故意关闭
- 为后续 `Phase 4.2 / 4.3 / 4.4` 提供统一参考

## 当前唯一有效构建来源

- 源码工作树：
  - `libs/Skia`
- 构建入口：
  - `scripts/run-lycium-build.sh`
- recipe：
  - `tpc_c_cplusplus/community/skia/HPKBUILD`

说明：

- 当前 OHOS 有效 feature matrix 以 `HPKBUILD` 为最终执行配置
- `BUILD.gn` / `skia.gni` 提供平台识别和可构建能力
- 设备验证通过 `ohos_egl_smoke`、`ohos_text_smoke`、`ohos_shaper_smoke` 和 `skia_OHOS` HAP 完成

## 当前有效 OHOS Feature Matrix

### 必开

| 功能 | 当前值 | 说明 |
|---|---|---|
| `skia_use_ohos` | `true` | 当前 OHOS 构建入口标识 |
| `skia_use_freetype` | `true` | 基础字体加载必须开启 |
| `skia_use_harfbuzz` | `true` | Phase 3 shaped text 已依赖 |
| `skia_use_icu` | `true` | 当前 shaping 使用完整 ICU Unicode |
| `skia_use_egl` | `true` | Phase 2 GPU 直连路径依赖 |
| `skia_use_gl` | `true` | 当前 GPU 后端是 GLES/Ganesh |

### 必关

| 功能 | 当前值 | 说明 |
|---|---|---|
| `skia_use_bidi` | `false` | 已被 ICU 路线替代，不再作为 shaping 主路径 |
| `skia_use_fontconfig` | `false` | Linux 桌面字体发现，不适合作为 OHOS 主路径 |
| `skia_use_x11` | `false` | Linux 桌面窗口系统，不适用当前 OHOS 路线 |
| `skia_use_vulkan` | `false` | 还未进入正式 OHOS Vulkan 路线 |
| `skia_use_perfetto` | `false` | 当前不纳入 OHOS 最小稳定矩阵 |
| `skia_enable_pdf` | `false` | 当前阶段不纳入 |
| `skia_enable_svg` | `false` | 当前阶段不纳入 |
| `skia_enable_skottie` | `false` | 当前阶段不纳入 |

### 使用 bundled 依赖

| 功能 | 当前值 | 说明 |
|---|---|---|
| `skia_use_system_freetype2` | `false` | 当前由 recipe 同步 bundled freetype |
| `skia_use_system_harfbuzz` | `false` | 当前由 recipe 同步 bundled harfbuzz |
| `skia_use_system_icu` | `false` | 当前由 recipe 同步 bundled icu |
| `skia_use_system_libpng` | `false` | 当前由 recipe 同步 bundled libpng |
| `skia_use_system_zlib` | `false` | 当前由 recipe 同步 bundled zlib |

### 当前关闭的次要编解码/扩展能力

| 功能 | 当前值 |
|---|---|
| `skia_use_zlib` | `false` |
| `skia_use_libpng_decode` | `false` |
| `skia_use_libpng_encode` | `false` |
| `skia_use_libwebp_decode` | `false` |
| `skia_use_libwebp_encode` | `false` |
| `skia_use_libjpeg_turbo_decode` | `false` |
| `skia_use_libjpeg_turbo_encode` | `false` |
| `skia_use_wuffs` | `false` |
| `skia_use_dng_sdk` | `false` |
| `skia_use_piex` | `false` |

## 当前稳定验证链路

### 1. 库构建验证

- 产出：
  - `libskia.so`

### 2. GPU 路径验证

- 入口：
  - `ohos_egl_smoke`
- 目标：
  - 验证 `EGL/GLES + Ganesh` 最小链路

### 3. 基础文本验证

- 入口：
  - `ohos_text_smoke`
- 目标：
  - 验证 `freetype + /system/fonts`

### 4. shaping 验证

- 入口：
  - `ohos_shaper_smoke`
- 目标：
  - 验证 `HarfBuzz + ICU`

### 5. HAP 验证

- 工程：
  - `skia_OHOS`
- 目标：
  - 验证 `XComponent + NativeWindow + EGL/GLES + Skia GPU + shaped text`

## 当前边界

当前 matrix 证明的是：

- 当前 OHOS 路线已经具备稳定的 GPU 直连渲染能力
- 当前 OHOS 路线已经具备稳定的 shaped text 基础能力

当前 matrix 还不能证明：

- `OHOS` 专用 font manager 已存在
- `Skia src/ports` 已存在正式 OHOS 原生 glue
- `OHOS` Vulkan 路线已成立
- `OHOS` 原生 buffer / image bridge 已成立

## 当前结论

到 `2026-03-26` 为止，当前 OHOS 有效 matrix 可以定义为：

- `freetype + harfbuzz + ICU + EGL/GLES`
- 排除 Linux 桌面字体与窗口路径
- 通过 `lycium + smoke test + HAP` 三层验证闭环成立

这份矩阵就是后续 `Phase 4` 和 `Phase 5` 的基线。
