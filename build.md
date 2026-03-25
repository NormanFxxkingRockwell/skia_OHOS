# 构建说明

## 目的

这份文件用于记录当前 `Skia on OHOS` 的实际可用构建流程、打包流程和验证流程。

后续继续做这个项目时，默认按这里的步骤执行，不要临时发明新流程。

## 仓库关系

当前有两条主线：

1. 主适配线
   - 在 `ho-thirdparty-porting` 里修改 `Skia`
   - 通过 `lycium` 编译
   - 这里才是三方库鸿蒙化的主战场

2. 验证线
   - 把主适配线产出的库同步到 `skia_OHOS`
   - 在 ArkTS/HAP 工程里做真机验证
   - 这里不是 `Skia` 适配真源头，而是验证工程

不要把 `skia_OHOS` 当成 `Skia` 适配主仓库。

## 当前相关仓库

- ArkTS 示例工程：
  - `skia_OHOS`
- 主适配仓库：
  - `ho-thirdparty-porting`
- Skia fork：
  - `https://github.com/NormanFxxkingRockwell/skia`
  - 当前 OHOS 分支：`m146-ohos`

## 编译 Skia 的唯一推荐方式

如果要编译当前 OHOS 适配版 `Skia`，必须走 `lycium`。

不要把“直接手写 GN 命令编一下”当成主流程。

当前有效入口是：

- 源码工作目录：
  - `ho-thirdparty-porting/libs/Skia`
- recipe：
  - `tpc_c_cplusplus/community/skia/HPKBUILD`

## Lycium 编译命令

在 `ho-thirdparty-porting` 根目录执行：

```bash
LIB_NAME=Skia PKGNAME=skia RECIPE_SCOPE=community ARCH=arm64-v8a bash scripts/run-lycium-build.sh
```

期望产物：

- `outputs/Skia/lib/libskia.so`
- `outputs/Skia/bin/ohos_egl_smoke`
- `outputs/Skia/bin/ohos_text_smoke`
- `outputs/Skia/bin/ohos_shaper_smoke`

## 当前 recipe 的关键点

当前可用 recipe：

- `tpc_c_cplusplus/community/skia/HPKBUILD`

如果有人问“现在该怎么编 Skia”，标准回答是：

- 用 `lycium`
- 用 `community/skia` 这份 recipe
- 源码修改放在 `libs/Skia`
- 不要绕过 recipe 自己临时拼一条构建路线

## 当前必须保留的 lycium 侧修改

### 1. builddir 必须同步当前工作树内容

recipe 会把 `libs/Skia` 里的当前文件同步到 lycium 的解压构建目录中。

当前同步的关键文件包括：

- `BUILD.gn`
- `gn/skia.gni`
- `tools/ohos_egl_smoke.cpp`
- `tools/ohos_text_smoke.cpp`
- `tools/ohos_shaper_smoke.cpp`

这样 `lycium` 编译出来的内容才和当前正在调试的 OHOS 适配代码一致。

### 2. recipe 必须同步 Skia 当前需要的 externals

当前 Phase 2 和 Phase 3 已经不再是“只画几何图形”的最小版本，所以 recipe 必须准备这些依赖：

- `freetype`
- `harfbuzz`
- `icu`
- `libpng`
- `zlib`

### 3. recipe 必须保留当前有效的 GN 配置

当前验证通过的方向是：

- `skia_use_ohos=true`
- `skia_use_fontconfig=false`
- `skia_use_freetype=true`
- `skia_use_system_freetype2=false`
- `skia_use_harfbuzz=true`
- `skia_use_system_harfbuzz=false`
- `skia_use_icu=true`
- `skia_use_bidi=false`
- `skia_use_system_icu=false`
- `skia_use_egl=true`
- `skia_use_gl=true`
- `skia_use_vulkan=false`
- `skia_use_x11=false`
- `skia_use_perfetto=false`
- `skia_enable_pdf=false`
- `skia_enable_svg=false`
- `skia_enable_skottie=false`

当前额外编译参数包括：

- `-DOHOS_NDK`
- `-D__MUSL__=1`
- `-fPIC`
- `-std=c++17`

### 4. recipe 必须同时构建库和 smoke test

当前 lycium 流程不只是编 `libskia.so`，还必须构建：

- `ohos_egl_smoke`
- `ohos_text_smoke`
- `ohos_shaper_smoke`

原因：

- `libskia.so` 证明库产物成立
- smoke test 证明能力在真机上真的成立
- Phase 3 的 shaping 验证目前就是靠 `ohos_shaper_smoke`

### 5. 依赖缓存必须放在 builddir 外

`lycium` 重编时会清理 recipe 的 `builddir`。

如果缓存放在 `builddir` 里面，每次重编都会重复拉这些依赖：

- `freetype`
- `harfbuzz`
- `icu`
- `libpng`
- `zlib`

当前有效做法是把缓存放到：

- `outputs/Skia/cache`

## 当前缓存策略

当前已经生效的缓存行为：

- `Skia` externals 缓存在 `outputs/Skia/cache`
- 避免每次重编都重新下载依赖
- `fetch-gn` 和 `fetch-ninja` 只在本地缺工具时才执行

## 为什么 Phase 3 必须切到 ICU

之前 `skia_use_bidi=true` 那条路足够支撑：

- 基础文本绘制
- 最小字体目录加载

但它不够支撑完整 `HarfBuzz shaping`。

已经确认的原因是：

- `SkUnicodes::Bidi` 不是完整 Unicode 实现
- 它缺少 shaping 需要的关键 Unicode 能力

所以当前 Phase 3 的正确配置是：

- `skia_use_icu=true`
- `skia_use_bidi=false`
- `skia_use_harfbuzz=true`

## 当前有效的 Skia 总配置

如果只想记住一组当前已验证通过的配置，可以直接记这一块：

- 构建方式：
  - `lycium`
- 源码位置：
  - `libs/Skia`
- recipe：
  - `tpc_c_cplusplus/community/skia/HPKBUILD`
- GPU 路线：
  - `EGL + GLES`
- 文本路线：
  - `freetype + harfbuzz + ICU`
- 当前关闭：
  - `fontconfig`
  - `x11`
  - `vulkan`
  - `perfetto`
  - `pdf`
  - `svg`
  - `skottie`

这就是当前 OHOS 已验证通过的 `Skia` 配置。

## 设备侧验证

设备命令统一用 Windows 侧 `hdc.exe`，不要再用 WSL 里的 `hdc`。

已知可用目标设备：

- `3QC0124A24000185`

查看设备：

```powershell
hdc.exe list targets -v
```

下发并执行 shaping smoke：

```powershell
$target='3QC0124A24000185'
$bin='\\wsl.localhost\Ubuntu\home\aoqiduan\projects\harmonyOS-mcp\harmonyOS-tool\ho-thirdparty-porting\outputs\Skia\bin\ohos_shaper_smoke'
$lib='\\wsl.localhost\Ubuntu\home\aoqiduan\projects\harmonyOS-mcp\harmonyOS-tool\ho-thirdparty-porting\outputs\Skia\lib\libskia.so'
hdc.exe -t $target file send $bin /data/local/tmp/ohos_shaper_smoke
hdc.exe -t $target file send $lib /data/local/tmp/libskia.so
hdc.exe -t $target shell "chmod +x /data/local/tmp/ohos_shaper_smoke; export LD_LIBRARY_PATH=/data/local/tmp:$LD_LIBRARY_PATH; /data/local/tmp/ohos_shaper_smoke /system/fonts"
```

当前 Phase 3 shaping 已知正确结果：

```text
font_families=235
shaper=shaper_driven_wrapper
pixel_checksum=8004268475873723857
```

## 同步到 skia_OHOS

至少需要同步：

- `libskia.so`

如果要让 `skia_OHOS` 在应用侧直接调用 shaping API，目前还要一起同步这些静态库：

- `libskshaper.a`
- `libskunicode_core.a`
- `libskunicode_icu.a`
- `libharfbuzz.a`
- `libicu.a`

原因：

- 当前 `libskia.so` 本身没有把应用侧 shaping 需要的符号全部直接导出来
- `ohos_shaper_smoke` 能跑，是因为工具目标把这些模块一起静态链接了
- 所以当前 `skia_OHOS` 也必须把这几份静态库一起链进 `libentry.so`

## skia_OHOS 打包命令

打包入口就用这条：

```powershell
& 'D:\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.bat' assembleHap
```

但在一个新 shell 里执行前，要先补环境变量：

```powershell
$env:NODE_HOME='D:\deveco\DevEco Studio\tools\node'
$env:JAVA_HOME='D:\deveco\DevEco Studio\jbr'
$env:DEVECO_SDK_HOME='D:\deveco\DevEco Studio\sdk'
$env:Path="$env:NODE_HOME;$env:JAVA_HOME\bin;$env:DEVECO_SDK_HOME\default\openharmony\toolchains;$env:Path"
```

如果 `assembleHap` 失败，先检查：

- `NODE_HOME`
- `JAVA_HOME`
- `DEVECO_SDK_HOME`
- 当前 shell 是否能找到 `node`
- 当前 shell 是否指向完整的 HarmonyOS SDK

## skia_OHOS 当前 native 链接规则

当前 `entry/src/main/cpp/CMakeLists.txt` 里，应用侧 shaping 路线要链接：

- `libskia.so`
- `libskshaper.a`
- `libskunicode_core.a`
- `libskunicode_icu.a`
- `libharfbuzz.a`
- `libicu.a`

如果 native 链接时报这些符号缺失：

- `SkUnicodes::ICU::Make()`
- `SkShapers::HB::ShaperDrivenWrapper(...)`
- `SkShaper::MakeFontMgrRunIterator(...)`

那通常不是代码错了，而是应用侧少链了 `skshaper/skunicode/icu/harfbuzz`。

## HAP 安装与启动

当前已知可用 HAP：

- `entry/build/default/outputs/default/entry-default-signed.hap`

安装和启动：

```powershell
$target='3QC0124A24000185'
$hap='D:\codeBase\skia_OHOS\entry\build\default\outputs\default\entry-default-signed.hap'
hdc.exe -t $target install -r $hap
hdc.exe -t $target shell "aa start -a EntryAbility -b com.example.myapplication"
```

查看应用日志：

```powershell
hdc.exe -t $target shell "hilog -x | grep SkiaXComponent | tail -n 40"
```

当前已知正确日志特征：

```text
native xcomponent callbacks registered
OnSurfaceCreated
surface created size=2030x986 ready=1
font manager ready families=235 font=/system/fonts/HarmonyOS_Sans_SC.ttf
RenderFrame finished frame=0 mode=gpu_direct
RenderFrame finished frame=1 mode=gpu_direct
```

## 当前阶段判断

Phase 2：

- 已完成
- GPU direct rendering 已在真机验证通过

Phase 3：

- 库层面已完成
- `freetype + harfbuzz + ICU + shaping smoke` 已在真机验证通过
- `skia_OHOS` 已完成 shaped text 路线接入，并且 HAP 打包通过

## 常见坑

- 用了 WSL 侧 `hdc`，而不是 Windows 侧 `hdc.exe`
- 重编 `lycium` 时忘了检查缓存是否还在生效
- 把 `skia_OHOS` 当成 `Skia` 主适配仓库
- 忘了把新版 `libskia.so` 同步进 `skia_OHOS`
- 以为应用侧只要 `libskia.so`，忘了还要 `skshaper/skunicode/harfbuzz/icu`
- 在一个没有设置 `NODE_HOME / JAVA_HOME / DEVECO_SDK_HOME` 的 shell 里直接跑 `assembleHap`

## 后续默认动作

从当前状态继续时，默认按这个顺序走：

1. 在 `ho-thirdparty-porting` 修改 `Skia` 或 recipe
2. 通过 `lycium` 重编
3. 用 smoke test 在真机上验证
4. 把库同步到 `skia_OHOS`
5. 打 HAP
6. 真机安装并验证
