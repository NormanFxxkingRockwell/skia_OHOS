# Skia Phase 4 Validation Chain

## 日期

- 2026-03-26

## 目标

- 固化当前 `lycium -> smoke test -> HAP` 的稳定验证链路
- 明确每一步的输入、输出、命令和成功标志
- 避免后续继续依赖零散经验和临时命令

## 当前验证链路总览

当前 `Skia on OHOS` 的稳定验证链路分为四层：

1. `lycium` 构建层
2. 设备侧 smoke test 层
3. 应用侧 HAP 打包层
4. 真机可视化验证层

## 第 1 层：lycium 构建层

### 目标

- 生成当前 OHOS 适配版 `libskia.so`
- 同时生成最小验证工具

### 命令

在 `ho-thirdparty-porting` 根目录执行：

```bash
LIB_NAME=Skia PKGNAME=skia RECIPE_SCOPE=community ARCH=arm64-v8a bash scripts/run-lycium-build.sh
```

### 当前关键输入

- `libs/Skia`
- `tpc_c_cplusplus/community/skia/HPKBUILD`
- `tpc_c_cplusplus/community/skia/SHA512SUM`

### 当前关键输出

- `outputs/Skia/lib/libskia.so`
- `outputs/Skia/bin/ohos_egl_smoke`
- `outputs/Skia/bin/ohos_text_smoke`
- `outputs/Skia/bin/ohos_shaper_smoke`

### 成功标志

- `run-lycium-build.sh` 正常结束
- `outputs/Skia/lib/libskia.so` 存在
- `outputs/Skia/bin/ohos_egl_smoke` 存在
- `outputs/Skia/bin/ohos_text_smoke` 存在
- `outputs/Skia/bin/ohos_shaper_smoke` 存在

## 第 2 层：设备侧 smoke test 层

### 目标

- 在不依赖 HAP 的前提下，验证当前库能力已经在设备侧成立

### 统一原则

- 统一使用 Windows 侧 `hdc.exe`
- 不再使用 WSL 内部 `hdc`

### 2.1 GPU 路径验证

工具：

- `ohos_egl_smoke`

目标：

- 验证 `EGL/GLES + Ganesh` 最小链路

### 2.2 基础文本验证

工具：

- `ohos_text_smoke`

目标：

- 验证 `freetype + /system/fonts`

### 2.3 Shaping 验证

工具：

- `ohos_shaper_smoke`

目标：

- 验证 `HarfBuzz + ICU`

### 当前已知 shaping 验证命令

```powershell
$target='3QC0124A24000185'
$bin='\\wsl.localhost\Ubuntu\home\aoqiduan\projects\harmonyOS-mcp\harmonyOS-tool\ho-thirdparty-porting\outputs\Skia\bin\ohos_shaper_smoke'
$lib='\\wsl.localhost\Ubuntu\home\aoqiduan\projects\harmonyOS-mcp\harmonyOS-tool\ho-thirdparty-porting\outputs\Skia\lib\libskia.so'
hdc.exe -t $target file send $bin /data/local/tmp/ohos_shaper_smoke
hdc.exe -t $target file send $lib /data/local/tmp/libskia.so
hdc.exe -t $target shell "chmod +x /data/local/tmp/ohos_shaper_smoke; export LD_LIBRARY_PATH=/data/local/tmp:$LD_LIBRARY_PATH; /data/local/tmp/ohos_shaper_smoke /system/fonts"
```

### 当前已知成功标志

```text
font_families=235
shaper=shaper_driven_wrapper
pixel_checksum=8004268475873723857
```

## 第 3 层：应用侧 HAP 打包层

### 目标

- 把当前 `Skia` 能力同步进验证工程
- 构建出可安装 HAP

### 当前验证工程

- `D:\codeBase\skia_OHOS`

### 当前同步内容

- `libskia.so`
- `libskshaper.a`
- `libskunicode_core.a`
- `libskunicode_icu.a`
- `libharfbuzz.a`
- `libicu.a`

### 当前打包入口

```powershell
& 'D:\deveco\DevEco Studio\tools\hvigor\bin\hvigorw.bat' assembleHap
```

### 打包前必备环境变量

```powershell
$env:NODE_HOME='D:\deveco\DevEco Studio\tools\node'
$env:JAVA_HOME='D:\deveco\DevEco Studio\jbr'
$env:DEVECO_SDK_HOME='D:\deveco\DevEco Studio\sdk'
$env:Path="$env:NODE_HOME;$env:JAVA_HOME\bin;$env:DEVECO_SDK_HOME\default\openharmony\toolchains;$env:Path"
```

### 成功标志

- `assembleHap` 成功
- 产出：
  - `entry/build/default/outputs/default/entry-default-signed.hap`

## 第 4 层：真机可视化验证层

### 目标

- 验证 HAP 层面的 GPU + shaped text 已经真实生效

### 安装与启动

```powershell
$target='3QC0124A24000185'
$hap='D:\codeBase\skia_OHOS\entry\build\default\outputs\default\entry-default-signed.hap'
hdc.exe -t $target install -r $hap
hdc.exe -t $target shell "aa start -a EntryAbility -b com.example.myapplication"
```

### 日志检查

```powershell
hdc.exe -t $target shell "hilog -x | grep SkiaXComponent | tail -n 40"
```

### 当前已知成功标志

```text
native xcomponent callbacks registered
OnSurfaceCreated
surface created size=2030x986 ready=1
font manager ready families=235 font=/system/fonts/HarmonyOS_Sans_SC.ttf
RenderFrame finished frame=0 mode=gpu_direct
RenderFrame finished frame=1 mode=gpu_direct
```

## 当前验证链路结论

当前这条验证链路已经证明：

- `lycium` 产物成立
- smoke test 能力成立
- HAP 打包成立
- HAP 真机运行成立

因此，后续无论继续做 Phase 4 还是进入 Phase 5，都应保持这四层验证结构不变。
