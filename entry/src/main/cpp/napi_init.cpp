#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <js_native_api.h>
#include <native_window/external_window.h>
#include <napi/native_api.h>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <mutex>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include "include/core/SkSurfaceProps.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"

namespace {

constexpr int32_t APP_LOG_DOMAIN = 0x3201;
constexpr const char* APP_LOG_TAG = "SkiaXComponent";

struct RendererState {
    OH_NativeXComponent* component = nullptr;
    OHNativeWindow* nativeWindow = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    sk_sp<GrDirectContext> directContext;
    sk_sp<SkSurface> skSurface;
    uint64_t width = 0;
    uint64_t height = 0;
    uint32_t frameIndex = 0;
    bool callbacksRegistered = false;
    bool ready = false;
};

RendererState g_renderer;
std::mutex g_mutex;

bool MakeContextCurrent(const RendererState& state)
{
    if (state.display == EGL_NO_DISPLAY || state.surface == EGL_NO_SURFACE || state.context == EGL_NO_CONTEXT) {
        return false;
    }
    if (eglMakeCurrent(state.display, state.surface, state.surface, state.context) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglMakeCurrent failed");
        return false;
    }
    return true;
}

bool RecreateSkSurface(RendererState& state)
{
    if (!state.directContext || state.width == 0 || state.height == 0) {
        return false;
    }
    if (!MakeContextCurrent(state)) {
        return false;
    }

    state.skSurface.reset();

    GLint framebuffer = 0;
    GLint stencilBits = 0;
    GLint sampleCount = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    glGetIntegerv(GL_SAMPLES, &sampleCount);

    GrGLFramebufferInfo framebufferInfo;
    framebufferInfo.fFBOID = static_cast<GrGLuint>(framebuffer);
    framebufferInfo.fFormat = GL_RGBA8;
    framebufferInfo.fProtected = skgpu::Protected::kNo;

    GrBackendRenderTarget backendRenderTarget = GrBackendRenderTargets::MakeGL(
        static_cast<int>(state.width),
        static_cast<int>(state.height),
        std::max(0, sampleCount),
        std::max(0, stencilBits),
        framebufferInfo);

    static const SkSurfaceProps surfaceProps;
    state.skSurface = SkSurfaces::WrapBackendRenderTarget(state.directContext.get(),
                                                          backendRenderTarget,
                                                          kBottomLeft_GrSurfaceOrigin,
                                                          kRGBA_8888_SkColorType,
                                                          nullptr,
                                                          &surfaceProps);
    if (!state.skSurface) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG,
            "WrapBackendRenderTarget failed framebuffer=%{public}d stencil=%{public}d samples=%{public}d",
            framebuffer, stencilBits, sampleCount);
        return false;
    }
    return true;
}

bool InitEgl(RendererState& state)
{
    state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state.display == EGL_NO_DISPLAY) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglGetDisplay failed");
        return false;
    }

    if (eglInitialize(state.display, nullptr, nullptr) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglInitialize failed");
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint configCount = 0;
    if (eglChooseConfig(state.display, configAttribs, &config, 1, &configCount) != EGL_TRUE || configCount == 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglChooseConfig failed");
        return false;
    }

    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglBindAPI failed");
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    state.context = eglCreateContext(state.display, config, EGL_NO_CONTEXT, contextAttribs);
    if (state.context == EGL_NO_CONTEXT) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglCreateContext failed");
        return false;
    }

    state.surface = eglCreateWindowSurface(
        state.display, config, reinterpret_cast<EGLNativeWindowType>(state.nativeWindow), nullptr);
    if (state.surface == EGL_NO_SURFACE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglCreateWindowSurface failed");
        return false;
    }

    if (!MakeContextCurrent(state)) {
        return false;
    }

    state.directContext = GrDirectContexts::MakeGL();
    if (!state.directContext) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "GrDirectContexts::MakeGL failed");
        return false;
    }

    return RecreateSkSurface(state);
}

void DestroyRenderer(RendererState& state)
{
    state.skSurface.reset();

    if (state.directContext) {
        state.directContext->flushAndSubmit();
        state.directContext->abandonContext();
        state.directContext.reset();
    }

    if (state.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (state.surface != EGL_NO_SURFACE) {
        eglDestroySurface(state.display, state.surface);
        state.surface = EGL_NO_SURFACE;
    }
    if (state.context != EGL_NO_CONTEXT) {
        eglDestroyContext(state.display, state.context);
        state.context = EGL_NO_CONTEXT;
    }
    if (state.display != EGL_NO_DISPLAY) {
        eglTerminate(state.display);
        state.display = EGL_NO_DISPLAY;
    }

    state.ready = false;
    state.width = 0;
    state.height = 0;
    state.nativeWindow = nullptr;
}

void DrawGpuScene(RendererState& state)
{
    if (!state.skSurface) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "skSurface unavailable");
        return;
    }

    SkCanvas* canvas = state.skSurface->getCanvas();
    canvas->clear(SkColorSetRGB(16, 24, 32));

    const float wf = static_cast<float>(state.width);
    const float hf = static_cast<float>(state.height);
    const float progress = static_cast<float>(state.frameIndex % 360) / 360.0f;

    SkPaint panelPaint;
    panelPaint.setAntiAlias(true);
    panelPaint.setColor(SkColorSetARGB(255, 38, 73, 92));
    canvas->drawRoundRect(SkRect::MakeXYWH(24.0f, 24.0f, wf - 48.0f, hf - 48.0f), 28.0f, 28.0f, panelPaint);

    SkPaint circlePaint;
    circlePaint.setAntiAlias(true);
    circlePaint.setColor(SkColorSetRGB(
        static_cast<U8CPU>(60 + progress * 120.0f),
        static_cast<U8CPU>(160 + progress * 40.0f),
        190));
    canvas->drawCircle(wf * 0.3f, hf * 0.35f, std::min(wf, hf) * 0.16f, circlePaint);

    SkPaint rectPaint;
    rectPaint.setAntiAlias(true);
    rectPaint.setColor(SkColorSetRGB(255, 183, 3));
    canvas->drawRect(SkRect::MakeXYWH(wf * 0.52f, hf * 0.2f, wf * 0.22f, hf * 0.22f), rectPaint);

    SkPaint strokePaint;
    strokePaint.setAntiAlias(true);
    strokePaint.setStyle(SkPaint::kStroke_Style);
    strokePaint.setStrokeWidth(10.0f);
    strokePaint.setColor(SkColorSetRGB(251, 86, 122));
    canvas->drawLine(wf * 0.18f, hf * 0.76f, wf * 0.48f, hf * 0.58f, strokePaint);
    canvas->drawLine(wf * 0.48f, hf * 0.58f, wf * 0.8f, hf * 0.76f, strokePaint);

    SkPaint dotPaint;
    dotPaint.setAntiAlias(true);
    dotPaint.setColor(SK_ColorWHITE);
    canvas->drawCircle(wf * (0.18f + progress * 0.54f), hf * 0.76f, 12.0f, dotPaint);
}

void RenderFrameLocked(RendererState& state)
{
    if (!state.ready || state.width == 0 || state.height == 0) {
        OH_LOG_Print(LOG_APP, LOG_WARN, APP_LOG_DOMAIN, APP_LOG_TAG,
            "RenderFrame skipped ready=%{public}d size=%{public}" PRIu64 "x%{public}" PRIu64,
            state.ready, state.width, state.height);
        return;
    }

    if (!MakeContextCurrent(state)) {
        return;
    }

    if (!state.skSurface && !RecreateSkSurface(state)) {
        return;
    }

    DrawGpuScene(state);
    state.directContext->flushAndSubmit(state.skSurface.get(), GrSyncCpu::kNo);
    eglSwapBuffers(state.display, state.surface);

    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG,
        "RenderFrame finished frame=%{public}u mode=gpu_direct", state.frameIndex);
    state.frameIndex++;
}

void OnSurfaceCreated(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG, "OnSurfaceCreated");
    std::lock_guard<std::mutex> lock(g_mutex);
    DestroyRenderer(g_renderer);

    g_renderer.component = component;
    g_renderer.nativeWindow = static_cast<OHNativeWindow*>(window);
    OH_NativeXComponent_GetXComponentSize(component, window, &g_renderer.width, &g_renderer.height);
    g_renderer.ready = InitEgl(g_renderer);
    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG,
        "surface created size=%{public}" PRIu64 "x%{public}" PRIu64 " ready=%{public}d",
        g_renderer.width, g_renderer.height, g_renderer.ready);
    if (g_renderer.ready) {
        RenderFrameLocked(g_renderer);
    }
}

void OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG, "OnSurfaceChanged");
    std::lock_guard<std::mutex> lock(g_mutex);
    g_renderer.component = component;
    g_renderer.nativeWindow = static_cast<OHNativeWindow*>(window);
    OH_NativeXComponent_GetXComponentSize(component, window, &g_renderer.width, &g_renderer.height);
    g_renderer.skSurface.reset();
    RenderFrameLocked(g_renderer);
}

void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window)
{
    (void)component;
    (void)window;
    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG, "OnSurfaceDestroyed");
    std::lock_guard<std::mutex> lock(g_mutex);
    DestroyRenderer(g_renderer);
}

void DispatchTouchEvent(OH_NativeXComponent* component, void* window)
{
    (void)component;
    (void)window;
    std::lock_guard<std::mutex> lock(g_mutex);
    RenderFrameLocked(g_renderer);
}

OH_NativeXComponent_Callback g_callback = {
    .OnSurfaceCreated = OnSurfaceCreated,
    .OnSurfaceChanged = OnSurfaceChanged,
    .OnSurfaceDestroyed = OnSurfaceDestroyed,
    .DispatchTouchEvent = DispatchTouchEvent,
};

bool RegisterXComponentCallbacksFromValue(napi_env env, napi_value holder)
{
    napi_value nativeXComponentValue = nullptr;
    napi_status status = napi_get_named_property(env, holder, OH_NATIVE_XCOMPONENT_OBJ, &nativeXComponentValue);
    if (status != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_WARN, APP_LOG_DOMAIN, APP_LOG_TAG,
            "native xcomponent object not found on holder, status=%{public}d", status);
        return false;
    }

    void* nativeXComponent = nullptr;
    status = napi_unwrap(env, nativeXComponentValue, &nativeXComponent);
    if (status != napi_ok || nativeXComponent == nullptr) {
        status = napi_get_value_external(env, nativeXComponentValue, &nativeXComponent);
    }

    if (status != napi_ok || nativeXComponent == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG,
            "failed to acquire native xcomponent pointer");
        return false;
    }

    int32_t result =
        OH_NativeXComponent_RegisterCallback(static_cast<OH_NativeXComponent*>(nativeXComponent), &g_callback);
    if (result != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG,
            "OH_NativeXComponent_RegisterCallback failed: %{public}d", result);
        return false;
    }

    g_renderer.component = static_cast<OH_NativeXComponent*>(nativeXComponent);
    g_renderer.callbacksRegistered = true;
    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG, "native xcomponent callbacks registered");
    return true;
}

napi_value RegisterSurface(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value thisArg = nullptr;
    if (napi_get_cb_info(env, info, &argc, nullptr, &thisArg, nullptr) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "napi_get_cb_info failed");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_renderer.callbacksRegistered) {
        RegisterXComponentCallbacksFromValue(env, thisArg);
    }
    return nullptr;
}

napi_value RenderNow(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value thisArg = nullptr;
    if (napi_get_cb_info(env, info, &argc, nullptr, &thisArg, nullptr) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "napi_get_cb_info failed in renderNow");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_renderer.callbacksRegistered) {
        RegisterXComponentCallbacksFromValue(env, thisArg);
    }
    RenderFrameLocked(g_renderer);
    return nullptr;
}

}  // namespace

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "registerSurface", nullptr, RegisterSurface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "renderNow", nullptr, RenderNow, nullptr, nullptr, nullptr, napi_default, nullptr }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    RegisterXComponentCallbacksFromValue(env, exports);
    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = nullptr,
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
}
