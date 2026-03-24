#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <js_native_api.h>
#include <native_window/external_window.h>
#include <napi/native_api.h>

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"

namespace {

constexpr int32_t APP_LOG_DOMAIN = 0x3201;
constexpr const char* APP_LOG_TAG = "SkiaXComponent";

constexpr const char* VERTEX_SHADER = R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

constexpr const char* FRAGMENT_SHADER = R"(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
}
)";

struct RendererState {
    OH_NativeXComponent* component = nullptr;
    OHNativeWindow* nativeWindow = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    GLuint program = 0;
    GLuint texture = 0;
    GLint positionLocation = -1;
    GLint texCoordLocation = -1;
    GLint textureLocation = -1;
    uint64_t width = 0;
    uint64_t height = 0;
    uint32_t frameIndex = 0;
    bool callbacksRegistered = false;
    bool ready = false;
};

RendererState g_renderer;
std::mutex g_mutex;

GLuint CompileShader(GLenum shaderType, const char* source)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<size_t>(logLength > 1 ? logLength : 1), '\0');
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG,
        "shader compile failed: %{public}s", log.c_str());
    glDeleteShader(shader);
    return 0;
}

GLuint CreateProgram()
{
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    if (vertexShader == 0) {
        return 0;
    }

    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aTexCoord");
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(static_cast<size_t>(logLength > 1 ? logLength : 1), '\0');
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG,
        "program link failed: %{public}s", log.c_str());
    glDeleteProgram(program);
    return 0;
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
        EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint configCount = 0;
    if (eglChooseConfig(state.display, configAttribs, &config, 1, &configCount) != EGL_TRUE || configCount == 0) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglChooseConfig failed");
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglBindAPI failed");
        return false;
    }

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

    if (eglMakeCurrent(state.display, state.surface, state.surface, state.context) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglMakeCurrent failed");
        return false;
    }

    state.program = CreateProgram();
    if (state.program == 0) {
        return false;
    }

    state.positionLocation = glGetAttribLocation(state.program, "aPosition");
    state.texCoordLocation = glGetAttribLocation(state.program, "aTexCoord");
    state.textureLocation = glGetUniformLocation(state.program, "uTexture");

    glGenTextures(1, &state.texture);
    glBindTexture(GL_TEXTURE_2D, state.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}

void DestroyRenderer(RendererState& state)
{
    if (state.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (state.texture != 0) {
        glDeleteTextures(1, &state.texture);
        state.texture = 0;
    }
    if (state.program != 0) {
        glDeleteProgram(state.program);
        state.program = 0;
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

void DrawCpuScene(RendererState& state, std::vector<uint8_t>& pixels)
{
    const int32_t width = static_cast<int32_t>(state.width);
    const int32_t height = static_cast<int32_t>(state.height);
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    pixels.resize(rowBytes * static_cast<size_t>(height));

    SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    SkBitmap bitmap;
    if (!bitmap.installPixels(info, pixels.data(), rowBytes)) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "bitmap installPixels failed");
        return;
    }

    SkCanvas canvas(bitmap);
    canvas.clear(SkColorSetRGB(16, 24, 32));

    const float wf = static_cast<float>(width);
    const float hf = static_cast<float>(height);
    const float progress = static_cast<float>(state.frameIndex % 360) / 360.0f;

    SkPaint panelPaint;
    panelPaint.setAntiAlias(true);
    panelPaint.setColor(SkColorSetARGB(255, 38, 73, 92));
    canvas.drawRoundRect(SkRect::MakeXYWH(24.0f, 24.0f, wf - 48.0f, hf - 48.0f), 28.0f, 28.0f, panelPaint);

    SkPaint circlePaint;
    circlePaint.setAntiAlias(true);
    circlePaint.setColor(SkColorSetRGB(
        static_cast<U8CPU>(60 + progress * 120.0f),
        static_cast<U8CPU>(160 + progress * 40.0f),
        190));
    canvas.drawCircle(wf * 0.3f, hf * 0.35f, std::min(wf, hf) * 0.16f, circlePaint);

    SkPaint rectPaint;
    rectPaint.setAntiAlias(true);
    rectPaint.setColor(SkColorSetRGB(255, 183, 3));
    canvas.drawRect(SkRect::MakeXYWH(wf * 0.52f, hf * 0.2f, wf * 0.22f, hf * 0.22f), rectPaint);

    SkPaint strokePaint;
    strokePaint.setAntiAlias(true);
    strokePaint.setStyle(SkPaint::kStroke_Style);
    strokePaint.setStrokeWidth(10.0f);
    strokePaint.setColor(SkColorSetRGB(251, 86, 122));
    canvas.drawLine(wf * 0.18f, hf * 0.76f, wf * 0.48f, hf * 0.58f, strokePaint);
    canvas.drawLine(wf * 0.48f, hf * 0.58f, wf * 0.8f, hf * 0.76f, strokePaint);

    SkPaint dotPaint;
    dotPaint.setAntiAlias(true);
    dotPaint.setColor(SK_ColorWHITE);
    canvas.drawCircle(wf * (0.18f + progress * 0.54f), hf * 0.76f, 12.0f, dotPaint);
}

void PresentToSurface(RendererState& state, const std::vector<uint8_t>& pixels)
{
    const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
    };

    glViewport(0, 0, static_cast<GLsizei>(state.width), static_cast<GLsizei>(state.height));
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(state.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.texture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 static_cast<GLsizei>(state.width),
                 static_cast<GLsizei>(state.height),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels.data());

    glUniform1i(state.textureLocation, 0);
    glEnableVertexAttribArray(static_cast<GLuint>(state.positionLocation));
    glEnableVertexAttribArray(static_cast<GLuint>(state.texCoordLocation));
    glVertexAttribPointer(static_cast<GLuint>(state.positionLocation), 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(static_cast<GLuint>(state.texCoordLocation), 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), vertices + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(static_cast<GLuint>(state.positionLocation));
    glDisableVertexAttribArray(static_cast<GLuint>(state.texCoordLocation));

    eglSwapBuffers(state.display, state.surface);
}

void RenderFrameLocked(RendererState& state)
{
    if (!state.ready || state.width == 0 || state.height == 0) {
        OH_LOG_Print(LOG_APP, LOG_WARN, APP_LOG_DOMAIN, APP_LOG_TAG,
            "RenderFrame skipped ready=%{public}d size=%{public}" PRIu64 "x%{public}" PRIu64,
            state.ready, state.width, state.height);
        return;
    }

    if (eglMakeCurrent(state.display, state.surface, state.surface, state.context) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "eglMakeCurrent before render failed");
        return;
    }

    std::vector<uint8_t> pixels;
    DrawCpuScene(state, pixels);
    PresentToSurface(state, pixels);
    OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG,
        "RenderFrame finished frame=%{public}u", state.frameIndex);
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
