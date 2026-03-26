# Skia Deep Adaptation Progress

## Date

- 2026-03-25

## Current Stage

- The work is now at the end of Phase 3.
- The main worktree is still `libs/Skia`.
- The primary build path is still `lycium-first`.

## What Changed In This Round

### 1. Lycium dependency caching was added

- `tpc_c_cplusplus/community/skia/HPKBUILD` now keeps Skia externals outside the transient `builddir`.
- Cached dependencies now include:
  - `freetype`
  - `harfbuzz`
  - `icu`
  - `libpng`
  - `zlib`
- `fetch-gn` and `fetch-ninja` are now only invoked when the local tool is missing.
- Result: repeated rebuilds no longer need to re-download the same dependency set every time.

### 2. Phase 3 moved from bidi subset to full ICU Unicode

- The earlier `skia_use_bidi=true` path was enough for minimal font-directory text rendering.
- It was not enough for full HarfBuzz shaping.
- Source inspection confirmed that `SkUnicodes::Bidi` leaves important Unicode methods unimplemented, including UTF-8 `computeCodeUnitFlags(...)` and break iterators.
- The build was therefore moved to:
  - `skia_use_icu=true`
  - `skia_use_bidi=false`
  - `skia_use_harfbuzz=true`

### 3. HarfBuzz shaping smoke test is now real

- `libs/Skia/tools/ohos_shaper_smoke.cpp` now:
  - prefers `SkUnicodes::ICU::Make()`
  - uses HarfBuzz shaping instead of plain `drawString`
  - emits a deterministic pixel checksum after raster readback
- The smoke test also reports which shaper path succeeded.

## Build Result

- WSL build command:

```bash
LIB_NAME=Skia PKGNAME=skia RECIPE_SCOPE=community ARCH=arm64-v8a bash scripts/run-lycium-build.sh
```

- Latest successful outputs:
  - `outputs/Skia/lib/libskia.so`
  - `outputs/Skia/bin/ohos_egl_smoke`
  - `outputs/Skia/bin/ohos_text_smoke`
  - `outputs/Skia/bin/ohos_shaper_smoke`

## Device Validation

- Windows-side `hdc` was used for the device step.
- Target device:
  - `3QC0124A24000185`
- Runtime font directory:
  - `/system/fonts`

- Device command:

```sh
export LD_LIBRARY_PATH=/data/local/tmp:$LD_LIBRARY_PATH
/data/local/tmp/ohos_shaper_smoke /system/fonts
```

- Actual device result:

```text
font_families=235
shaper=shaper_driven_wrapper
pixel_checksum=8004268475873723857
```

## Phase 3 Conclusion

- Phase 3 is now functionally complete.
- The project has moved from:
  - `freetype + directory font loading + drawString`
- to:
  - `freetype + harfbuzz + ICU Unicode + shaped text smoke on device`

This means the work is no longer limited to:
- source-level entry validation
- compile-only success
- non-shaped basic text drawing

It now proves:
- `lycium` can build a Skia library with HarfBuzz + ICU enabled on OHOS
- shaped text rendering can execute on a real HarmonyOS device
- the Phase 3 text stack is no longer blocked on the last runtime issue

## Remaining Gaps After Phase 3

- `skia_OHOS` still needs a HAP-side shaped text demo using the new library.
- fallback font behavior still needs a more systematic validation matrix.
- an OHOS-specific font manager path has not been implemented yet.
- deeper OHOS-native platform glue under `src/ports/` is still future work.

## Recommended Next Step

1. Promote the new `libskia.so` into `skia_OHOS`.
2. Replace the current HAP text demo with a true HarfBuzz-shaped text demo.
3. Start the next phase on OHOS-specific font management and platform glue.

## Phase 4 Update: NativeDrawing-based OHOS Font Manager

### What changed

- `SkFontMgr_ohos` no longer treats `/system/etc/fontconfig*.json` parsing as the primary source.
- The OHOS font manager now prefers official NativeDrawing interfaces:
  - `OH_Drawing_GetSystemFontConfigInfo`
  - `OH_Drawing_CreateFontParser`
  - `OH_Drawing_FontParserGetSystemFontList`
  - `OH_Drawing_FontParserGetFontByName`
- `font_dir`, generic alias mapping, and fallback families are now obtained from the system through the OHOS API layer first.
- `libnative_drawing.so` is now linked into the `fontmgr_ohos` build target.

### Why this matters

- The previous JSON parsing approach was useful for early bring-up, but it was fragile:
  - it depended on file layout details
  - it duplicated logic that the platform already exposes via API
- The new route is closer to a real platform adaptation:
  - system font configuration comes from official OHOS interfaces
  - `Skia` still uses its own FreeType / HarfBuzz / ICU stack for rendering
  - but font discovery and alias/fallback metadata now come from the platform API first

### Validation result

- `lycium` build remained successful after linking `libnative_drawing.so`
- device `ohos_text_smoke` result:

```text
font_families=235
alias_harmonyos_sans=1
alias_serif=1
fallback_cjk=1
pixel_checksum=18319541926308614285
```

- device `ohos_shaper_smoke` result:

```text
font_families=235
shaper=shaper_driven_wrapper
pixel_checksum=8004268475873723857
```

### Current conclusion

- OHOS font management has now entered source-level platform adaptation.
- The current implementation is no longer “read config file and hope it matches the system”.
- It is now “NativeDrawing API first, directory fallback only when the platform data is unavailable”.
