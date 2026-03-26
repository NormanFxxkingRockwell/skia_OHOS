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
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/core/SkSurfaceProps.h"
#include "include/core/SkTextBlob.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/ports/SkFontMgr_ohos.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skshaper/include/SkShaper_skunicode.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"

namespace {

constexpr int32_t APP_LOG_DOMAIN = 0x3201;
constexpr const char* APP_LOG_TAG = "SkiaXComponent";
constexpr const char* PREFERRED_SC_FONT = "/system/fonts/HarmonyOS_Sans_SC.ttf";
constexpr const char* ALT_SC_FONT = "/system/fonts/FZHeiT-SC-Regular.ttf";
constexpr const char* CJK_FONT = "/system/fonts/NotoSansCJK-Regular.ttc";

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

bool EnsureTypeface(RendererState& state)
{
    if (state.typeface) {
        return true;
    }
    if (!state.fontMgr) {
        state.fontMgr = SkFontMgr_New_OHOS();
        if (!state.fontMgr || state.fontMgr->countFamilies() == 0) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG,
                "failed to load fonts from SkFontMgr_New_OHOS");
            return false;
        }
    }
    state.typeface = state.fontMgr->legacyMakeTypeface(nullptr, SkFontStyle());
    if (!state.typeface) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG,
            "failed to create typeface from SkFontMgr_New_OHOS");
        return false;
    }
    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG,
        "font manager ready families=%{public}d source=SkFontMgr_New_OHOS", state.fontMgr->countFamilies());
    return true;
}

sk_sp<SkTextBlob> MakeShapedBlob(sk_sp<SkFontMgr> fontMgr,
                                 sk_sp<SkTypeface> typeface,
                                 const char* text,
                                 SkScalar fontSize)
{
    if (!fontMgr || !typeface || !text) {
        return nullptr;
    }

    const size_t textBytes = strlen(text);
    if (textBytes == 0) {
        return nullptr;
    }

    sk_sp<SkUnicode> unicode = SkUnicodes::ICU::Make();
    if (!unicode) {
        return nullptr;
    }

    auto shaper = SkShapers::HB::ShaperDrivenWrapper(unicode, fontMgr);
    if (!shaper) {
        return nullptr;
    }

    SkFont font(typeface, fontSize);
    font.setSubpixel(true);
    font.setEdging(SkFont::Edging::kSubpixelAntiAlias);

    std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
        SkShapers::unicode::BidiRunIterator(unicode, text, textBytes, 0xfe);
    if (!bidi) {
        bidi = std::make_unique<SkShaper::TrivialBiDiRunIterator>(0xfe, textBytes);
    }
    if (!bidi) {
        return nullptr;
    }

    std::unique_ptr<SkShaper::LanguageRunIterator> language =
        std::make_unique<SkShaper::TrivialLanguageRunIterator>("zh-CN", textBytes);
    if (!language) {
        return nullptr;
    }

    const SkFourByteTag undeterminedScript = SkSetFourByteTag('Z', 'y', 'y', 'y');
    std::unique_ptr<SkShaper::ScriptRunIterator> script =
        SkShapers::HB::ScriptRunIterator(text, textBytes, undeterminedScript);
    if (!script) {
        script = std::make_unique<SkShaper::TrivialScriptRunIterator>(undeterminedScript, textBytes);
    }
    if (!script) {
        return nullptr;
    }

    std::unique_ptr<SkShaper::FontRunIterator> fontRuns =
        SkShaper::MakeFontMgrRunIterator(text, textBytes, font, fontMgr);
    if (!fontRuns) {
        return nullptr;
    }

    SkTextBlobBuilderRunHandler builder(text, { 0.0f, 0.0f });
    shaper->shape(text,
                  textBytes,
                  *fontRuns,
                  *bidi,
                  *script,
                  *language,
                  nullptr,
                  0,
                  1200.0f,
                  &builder);
    return builder.makeBlob();
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
    state.typeface.reset();
    state.fontMgr.reset();

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
    if (!EnsureTypeface(state)) {
        return;
    }

    sk_sp<SkTypeface> altTypeface = state.fontMgr->makeFromFile(ALT_SC_FONT);
    sk_sp<SkTypeface> cjkTypeface = state.fontMgr->makeFromFile(CJK_FONT);

    SkCanvas* canvas = state.skSurface->getCanvas();
    canvas->clear(SkColorSetRGB(14, 22, 30));

    const float wf = static_cast<float>(state.width);
    const float hf = static_cast<float>(state.height);
    const float padding = std::max(24.0f, wf * 0.035f);

    SkPaint cardPaint;
    cardPaint.setAntiAlias(true);
    cardPaint.setColor(SkColorSetARGB(255, 33, 56, 72));
    canvas->drawRoundRect(SkRect::MakeXYWH(padding, padding, wf - padding * 2.0f, hf - padding * 2.0f),
                          26.0f,
                          26.0f,
                          cardPaint);

    SkPaint bannerPaint;
    bannerPaint.setAntiAlias(true);
    bannerPaint.setColor(SkColorSetRGB(208, 232, 255));
    canvas->drawRoundRect(SkRect::MakeXYWH(padding + 22.0f, padding + 22.0f, wf - padding * 2.0f - 44.0f, 84.0f),
                          18.0f,
                          18.0f,
                          bannerPaint);

    SkPaint titlePaint;
    titlePaint.setAntiAlias(true);
    titlePaint.setColor(SkColorSetRGB(18, 52, 86));

    SkPaint bodyPaint;
    bodyPaint.setAntiAlias(true);
    bodyPaint.setColor(SK_ColorWHITE);

    SkPaint labelPaint;
    labelPaint.setAntiAlias(true);
    labelPaint.setColor(SkColorSetRGB(152, 201, 255));

    SkPaint accentPaint;
    accentPaint.setAntiAlias(true);
    accentPaint.setColor(SkColorSetRGB(255, 183, 3));
    canvas->drawCircle(wf - padding - 48.0f, padding + 64.0f, 12.0f, accentPaint);

    SkFont titleFont(state.typeface, std::max(32.0f, hf * 0.055f));
    titleFont.setSubpixel(true);
    titleFont.setEdging(SkFont::Edging::kSubpixelAntiAlias);
    canvas->drawString("HarmonyOS Skia", padding + 40.0f, padding + 78.0f, titleFont, titlePaint);

    SkFont infoFont(state.typeface, std::max(22.0f, hf * 0.03f));
    infoFont.setSubpixel(true);
    canvas->drawString("GPU direct multi-font rendering", padding + 36.0f, padding + 154.0f, infoFont, bodyPaint);
    canvas->drawString("Font source: SkFontMgr_New_OHOS()", padding + 36.0f, padding + 194.0f, infoFont, bodyPaint);

    const float sampleTop = padding + 248.0f;
    const float lineGap = std::max(66.0f, hf * 0.075f);

    auto drawSample = [&](float y, const char* label, sk_sp<SkTypeface> face) {
        SkFont labelFont(state.typeface, std::max(18.0f, hf * 0.024f));
        labelFont.setSubpixel(true);
        canvas->drawString(label, padding + 36.0f, y, labelFont, labelPaint);

        const SkScalar sampleFontSize = std::max(28.0f, hf * 0.035f);
        sk_sp<SkTextBlob> blob = MakeShapedBlob(state.fontMgr,
                                                face ? face : state.typeface,
                                                u8"\u4e2d\u6587\u5b57\u4f53\u6e32\u67d3\u9a8c\u8bc1 ABC 123",
                                                sampleFontSize);
        if (blob) {
            canvas->drawTextBlob(blob.get(), padding + 36.0f, y + 34.0f, bodyPaint);
            return;
        }

        SkFont fallbackFont(face ? face : state.typeface, sampleFontSize);
        fallbackFont.setSubpixel(true);
        fallbackFont.setEdging(SkFont::Edging::kSubpixelAntiAlias);
        canvas->drawString("shaping unavailable",
                           padding + 36.0f,
                           y + 34.0f,
                           fallbackFont,
                           bodyPaint);
    };

    drawSample(sampleTop, "HarmonyOS_Sans_SC.ttf", state.typeface);
    drawSample(sampleTop + lineGap, "FZHeiT-SC-Regular.ttf", altTypeface);
    drawSample(sampleTop + lineGap * 2.0f, "NotoSansCJK-Regular.ttc", cjkTypeface);

    SkPaint footerPaint;
    footerPaint.setAntiAlias(true);
    footerPaint.setColor(SkColorSetRGB(152, 201, 255));
    SkFont footerFont(state.typeface, std::max(18.0f, hf * 0.024f));
    footerFont.setSubpixel(true);
    canvas->drawString("Skia -> GrDirectContext -> WrapBackendRenderTarget -> XComponent",
                       padding + 36.0f,
                       hf - padding - 28.0f,
                       footerFont,
                       footerPaint);
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
