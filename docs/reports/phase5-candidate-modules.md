# Skia Phase 5 Candidate Modules

## 日期

- 2026-03-26

## 目标

- 列出后续真正需要进入 `Skia src/` 的 OHOS 平台适配候选点
- 为 Phase 5 的源码级工作划清边界
- 避免继续把“构建适配”和“平台实现”混在一起

## 当前判断原则

只有当一个问题满足以下条件之一时，才进入 Phase 5 源码级适配：

1. 仅靠 `GN/HPKBUILD/smoke test` 已无法稳定表达
2. 当前能力仍由应用侧承接，不在 `Skia` 本体内
3. 要达到 Linux / Android 级别的平台支持，必须进入 `src/ports`、`src/gpu` 或相关平台层

## 候选模块一：OHOS 专用字体管理层

### 当前状态

- 未实现

### 当前问题

- 当前仍依赖 `/system/fonts`
- 当前主要依赖文件字体加载
- 当前没有正式的 OHOS 系统字体发现和 fallback 策略

### 候选位置

- `libs/Skia/src/ports/`
- `libs/Skia/src/text/`

### 目标

- 让 `Skia` 不再只依赖目录扫描和文件加载
- 建立更正式的 OHOS 字体发现、fallback、配置解析路径

## 候选模块二：OHOS 专用 window / surface context

### 当前状态

- 未实现

### 当前问题

- 当前 `XComponent + NativeWindow + EGL` 生命周期都在应用侧
- `Skia` 本体没有正式的 OHOS 平台 window context

### 候选位置

- `libs/Skia/src/gpu/`
- `libs/Skia/src/ports/`
- `libs/Skia/tools/window/`（若先以验证入口方式推进）

### 目标

- 让 OHOS 的 window / surface 生命周期不再完全依赖验证工程
- 让平台接入逻辑逐步回到 `Skia` 平台层

## 候选模块三：OHOS 原生 buffer / image bridge

### 当前状态

- 未实现

### 当前问题

- 当前尚未进入 OHOS 原生 buffer 和图像桥接层
- 还没有把 `OH_NativeBuffer` 这类能力纳入 `Skia` 平台实现

### 候选位置

- `libs/Skia/src/ports/`
- `libs/Skia/src/gpu/`

### 目标

- 为后续更正式的 GPU / 图像互操作打基础

## 候选模块四：更稳定的 GPU 生命周期管理

### 当前状态

- 部分能力仍在应用侧

### 当前问题

- 当前 `surface` 创建、变化、销毁和重建主要在 `skia_OHOS` 中处理
- 尚未形成 `Skia` 平台层内部更正式的生命周期模型

### 候选位置

- `libs/Skia/src/gpu/`
- `libs/Skia/src/ports/`

### 目标

- 降低应用侧 glue 复杂度
- 提高后续平台接入的可复用性

## 候选模块五：OHOS 平台 glue

### 当前状态

- 未实现

### 当前问题

- 当前没有形成专门的 OHOS logging / debug / OS glue
- 仍主要复用通用实现

### 候选位置

- `libs/Skia/src/ports/`

### 目标

- 补齐平台级基础 glue

## 当前优先级建议

建议优先顺序：

1. OHOS 专用字体管理层
2. OHOS 专用 window / surface context
3. 更稳定的 GPU 生命周期管理
4. OHOS 原生 buffer / image bridge
5. 其他平台 glue

## 结论

到 `2026-03-26` 为止，Phase 5 不应再写成泛泛的“继续深度适配”。

更准确的 Phase 5 起点应是：

- `OHOS` 专用字体管理层
- `OHOS` 专用 window / surface context
- `src/ports` 与 `src/gpu` 中的 OHOS 平台 glue

这三类才是接下来真正会进入 `Skia` 源码级平台适配的核心模块。
