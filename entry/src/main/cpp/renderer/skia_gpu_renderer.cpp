#include "renderer/skia_gpu_renderer.h"

#include <algorithm>
#include <cinttypes>

#include <GLES3/gl3.h>
#include <hilog/log.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurfaceProps.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "renderer/render_log.h"
#include "renderer/skia_scene_renderer.h"

namespace skia_ohos {
namespace {

bool MakeContextCurrent(const RendererState& state) {
    if (state.display == EGL_NO_DISPLAY || state.surface == EGL_NO_SURFACE || state.context == EGL_NO_CONTEXT) {
        return false;
    }
    if (eglMakeCurrent(state.display, state.surface, state.surface, state.context) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "eglMakeCurrent failed");
        return false;
    }
    return true;
}

bool RecreateSkSurface(RendererState& state) {
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
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag,
                     "WrapBackendRenderTarget failed framebuffer=%{public}d stencil=%{public}d samples=%{public}d",
                     framebuffer, stencilBits, sampleCount);
        return false;
    }
    return true;
}

}  // namespace

bool InitEgl(RendererState& state) {
    state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state.display == EGL_NO_DISPLAY) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "eglGetDisplay failed");
        return false;
    }

    if (eglInitialize(state.display, nullptr, nullptr) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "eglInitialize failed");
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
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "eglChooseConfig failed");
        return false;
    }

    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "eglBindAPI failed");
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    state.context = eglCreateContext(state.display, config, EGL_NO_CONTEXT, contextAttribs);
    if (state.context == EGL_NO_CONTEXT) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "eglCreateContext failed");
        return false;
    }

    state.surface = eglCreateWindowSurface(
        state.display, config, reinterpret_cast<EGLNativeWindowType>(state.nativeWindow), nullptr);
    if (state.surface == EGL_NO_SURFACE) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "eglCreateWindowSurface failed");
        return false;
    }

    if (!MakeContextCurrent(state)) {
        return false;
    }

    state.directContext = GrDirectContexts::MakeGL();
    if (!state.directContext) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "GrDirectContexts::MakeGL failed");
        return false;
    }

    return RecreateSkSurface(state);
}

void DestroyRenderer(RendererState& state) {
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

void RenderFrame(RendererState& state) {
    if (!state.ready || state.width == 0 || state.height == 0) {
        OH_LOG_Print(LOG_APP, LOG_WARN, kAppLogDomain, kAppLogTag,
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

    OH_LOG_Print(LOG_APP, LOG_INFO, kAppLogDomain, kAppLogTag,
                 "RenderFrame finished frame=%{public}u mode=gpu_direct", state.frameIndex);
    state.frameIndex++;
}

}  // namespace skia_ohos
