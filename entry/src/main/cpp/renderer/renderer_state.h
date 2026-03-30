#ifndef SKIA_OHOS_RENDERER_STATE_H
#define SKIA_OHOS_RENDERER_STATE_H

#include <cstdint>

#include <EGL/egl.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>

#include "include/core/SkFontMgr.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/gpu/ganesh/GrDirectContext.h"

namespace skia_ohos {

struct RendererState {
    OH_NativeXComponent* component = nullptr;
    OHNativeWindow* nativeWindow = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    sk_sp<GrDirectContext> directContext;
    sk_sp<SkSurface> skSurface;
    sk_sp<SkFontMgr> fontMgr;
    sk_sp<SkTypeface> typeface;
    uint64_t width = 0;
    uint64_t height = 0;
    uint32_t frameIndex = 0;
    bool callbacksRegistered = false;
    bool ready = false;
};

}  // namespace skia_ohos

#endif  // SKIA_OHOS_RENDERER_STATE_H
