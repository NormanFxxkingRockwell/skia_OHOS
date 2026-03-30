# skia_OHOS 构建与验证手册

最后更新：`2026-03-30`

## 修改日志

- `2026-03-30`
  - 清理历史追加内容，改为当前快照版本
  - 补充 native 重构后的目录结构与同步规则
  - 补充最新 HAP 打包、安装、启动与日志回归结果

## 文档定位

本文档只记录当前有效的构建、同步、打包和验证方式，不保留历史过程记录。

## 仓库分工

主适配仓库：

- `ho-thirdparty-porting`
- `Skia` 的真实工作树和 `lycium` 主构建入口都在这里

验证工程：

- `skia_OHOS`
- 这里只负责：
  - 同步产物
  - HAP 打包
  - 真机界面验证

对外同步仓库：

- `skia`
- 用于同步 `Skia` 本体的 OHOS 改动，以及 `lycium` 配置说明

## 当前主构建方式

当前编译 OHOS 适配版 `Skia`，必须走 `lycium-first`。

主命令：

```bash
cd /home/aoqiduan/projects/harmonyOS-mcp/harmonyOS-tool/ho-thirdparty-porting
LIB_NAME=Skia PKGNAME=skia RECIPE_SCOPE=community ARCH=arm64-v8a bash scripts/run-lycium-build.sh
```

当前主 recipe：

- `tpc_c_cplusplus/community/skia/HPKBUILD`

## 当前有效构建配置

当前 OHOS 已验证通过的关键配置：

- `skia_use_ohos=true`
- `skia_use_freetype=true`
- `skia_use_harfbuzz=true`
- `skia_use_icu=true`
- `skia_use_bidi=false`
- `skia_use_egl=true`
- `skia_use_gl=true`
- `skia_use_fontconfig=false`
- `skia_use_x11=false`
- `skia_use_vulkan=false`

当前验证过的产物：

- `outputs/Skia/lib/libskia.so`
- `outputs/Skia/bin/ohos_egl_smoke`
- `outputs/Skia/bin/ohos_text_smoke`
- `outputs/Skia/bin/ohos_shaper_smoke`

## lycium 侧当前必须保留的改动

当前 `HPKBUILD` 里必须保留的关键点：

- 从 `libs/Skia` 同步当前工作树文件
- 同步：
  - `BUILD.gn`
  - `gn/skia.gni`
  - `include/ports/SkFontMgr_ohos.h`
  - `src/ports/SkFontMgr_ohos.cpp`
  - `tools/ohos_egl_smoke.cpp`
  - `tools/ohos_text_smoke.cpp`
  - `tools/ohos_shaper_smoke.cpp`
- 保留 bundled 依赖同步：
  - `freetype`
  - `harfbuzz`
  - `icu`
  - `libpng`
  - `zlib`
- 依赖缓存移出 `builddir`
- 工具目标必须继续构建：
  - `ohos_egl_smoke`
  - `ohos_text_smoke`
  - `ohos_shaper_smoke`

## 当前字体系统路线

当前字体系统不是 Linux `fontconfig` 路线。

当前主路线是：

- `SkFontMgr_ohos`
- `FreeType + HarfBuzz + ICU`
- `OHOS NativeDrawing` 官方字体接口优先

当前优先使用的官方接口：

- `OH_Drawing_GetSystemFontConfigInfo`
- `OH_Drawing_CreateFontParser`
- `OH_Drawing_FontParserGetSystemFontList`
- `OH_Drawing_FontParserGetFontByName`

当前含义：

- 系统字体目录、generic alias、fallback 信息优先来自 OHOS 官方接口
- `Skia` 自己负责真实 typeface 创建和文本渲染
- 显式目录参数只保留给 smoke test 覆盖场景，不作为系统主路径

## 设备 smoke test 验证

设备命令统一使用 Windows 侧：

- `D:\deveco\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe`

不要使用 WSL 侧 `command-line-tools` 里的 `hdc`。

典型步骤：

```powershell
$hdc='D:\deveco\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe'
$tmp='C:\Users\aoqiduan\AppData\Local\Temp\skia_smoke'
```

同步产物后在设备上执行：

```sh
export LD_LIBRARY_PATH=/data/local/tmp/SkiaPhase4:$LD_LIBRARY_PATH
/data/local/tmp/SkiaPhase4/ohos_text_smoke
/data/local/tmp/SkiaPhase4/ohos_shaper_smoke
```

当前已知正确结果：

```text
font_families=235
alias_harmonyos_sans=1
alias_serif=1
fallback_cjk=1
fallback_arabic_lang=1
fallback_tibetan_lang=1
pixel_checksum=18319541926308614285
```

```text
font_families=235
shaper=shaper_driven_wrapper
pixel_checksum=8004268475873723857
```

## 同步到 skia_OHOS

每次 `ho-thirdparty-porting` 里产物或头文件更新后，至少同步：

- `skia/prebuilt/arm64-v8a/libskia.so`
- `skia/include/ports/SkFontMgr_ohos.h`

如果应用侧调用方式变化，还要同步：

- `entry/src/main/cpp/CMakeLists.txt`
- `entry/src/main/cpp/napi_init.cpp`
- `entry/src/main/cpp/renderer/render_log.h`
- `entry/src/main/cpp/renderer/renderer_state.h`
- `entry/src/main/cpp/renderer/skia_scene_renderer.h`
- `entry/src/main/cpp/renderer/skia_scene_renderer.cpp`
- `entry/src/main/cpp/renderer/skia_gpu_renderer.h`
- `entry/src/main/cpp/renderer/skia_gpu_renderer.cpp`
- `entry/src/main/cpp/renderer/xcomponent_bridge.h`
- `entry/src/main/cpp/renderer/xcomponent_bridge.cpp`

## skia_OHOS 当前 native 工程结构

- `napi_init.cpp`
  - 仅负责 NAPI 模块注册与导出绑定
- `renderer/xcomponent_bridge.cpp`
  - 管理 `XComponent` 生命周期与 native 桥接
- `renderer/skia_gpu_renderer.cpp`
  - 管理 `EGL`、`GrDirectContext`、`SkSurface`
- `renderer/skia_scene_renderer.cpp`
  - 管理图形与文字场景绘制
- `renderer/renderer_state.h`
  - 管理共享渲染状态

## 当前 native 链接规则

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

通常不是代码写错了，而是应用侧少链接了 `skshaper/skunicode/icu/harfbuzz`。

## skia_OHOS 打包命令

打包入口：

```powershell
& 'D:\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.bat' assembleHap
```

新 shell 里执行前，先补环境变量：

```powershell
$env:NODE_HOME='D:\deveco\DevEco Studio\tools\node'
$env:JAVA_HOME='D:\deveco\DevEco Studio\jbr'
$env:DEVECO_SDK_HOME='D:\deveco\DevEco Studio\sdk'
$env:Path="$env:NODE_HOME;$env:JAVA_HOME\bin;$env:DEVECO_SDK_HOME\default\openharmony\toolchains;$env:Path"
```

## HAP 安装与启动

当前可用 HAP：

- `entry/build/default/outputs/default/entry-default-signed.hap`

安装与启动：

```powershell
$target='3QC0124A24000185'
$hap='D:\codeBase\skia_OHOS\entry\build\default\outputs\default\entry-default-signed.hap'
$hdc='D:\deveco\DevEco Studio\sdk\default\openharmony\toolchains\hdc.exe'
& $hdc -t $target install -r $hap
& $hdc -t $target shell "aa start -a EntryAbility -b com.example.myapplication"
```

查看日志：

```powershell
& $hdc -t $target shell "hilog -x | grep SkiaXComponent | tail -n 40"
```

当前已知正确日志特征：

```text
native xcomponent callbacks registered
OnSurfaceCreated
surface created size=2030x986 ready=1
font manager ready families=235 source=SkFontMgr_New_OHOS
RenderFrame finished frame=0 mode=gpu_direct
RenderFrame finished frame=1 mode=gpu_direct
```

## 当前阶段判断

- `Phase 2`
  - 已完成
  - GPU direct rendering 已在真机验证通过
- `Phase 3`
  - 已完成
  - `freetype + harfbuzz + ICU + shaping smoke` 已在真机验证通过
  - `skia_OHOS` 已完成 shaped text 路线接入，并且 HAP 打包通过
- `Phase 4`
  - 已开始并完成第一项源码级平台工作
  - `SkFontMgr_ohos` 已切到 OHOS 官方字体接口优先
  - `skia_OHOS` native 已完成第一轮工程化拆分并回归通过

## 常见坑

- 用了 WSL 侧 `hdc`，而不是 Windows 侧 `hdc.exe`
- 修改了 `libs/Skia`，但忘了同步到 `HPKBUILD` 工作树复制列表
- 重编 `lycium` 后忘了同步新 `libskia.so` 到 `skia_OHOS`
- 以为应用侧只要 `libskia.so`，忘了还要 `skshaper/skunicode/harfbuzz/icu`
- 在没有设置 `NODE_HOME / JAVA_HOME / DEVECO_SDK_HOME` 的 shell 里直接跑 `assembleHap`

## 默认续做顺序

1. 在 `ho-thirdparty-porting` 修改 `Skia` 或 recipe
2. 通过 `lycium` 重编
3. 用 smoke test 在真机上验证
4. 把产物同步到 `skia_OHOS`
5. 打 HAP
6. 真机安装并验证
