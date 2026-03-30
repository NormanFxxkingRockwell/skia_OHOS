#include "renderer/xcomponent_bridge.h"

#include <cinttypes>
#include <mutex>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <napi/native_api.h>

#include "renderer/render_log.h"
#include "renderer/renderer_state.h"
#include "renderer/skia_gpu_renderer.h"

namespace skia_ohos {
namespace {

RendererState g_renderer;
std::mutex g_mutex;

void OnSurfaceCreated(OH_NativeXComponent* component, void* window) {
    OH_LOG_Print(LOG_APP, LOG_INFO, kAppLogDomain, kAppLogTag, "OnSurfaceCreated");
    std::lock_guard<std::mutex> lock(g_mutex);
    DestroyRenderer(g_renderer);

    g_renderer.component = component;
    g_renderer.nativeWindow = static_cast<OHNativeWindow*>(window);
    OH_NativeXComponent_GetXComponentSize(component, window, &g_renderer.width, &g_renderer.height);
    g_renderer.ready = InitEgl(g_renderer);
    OH_LOG_Print(LOG_APP, LOG_INFO, kAppLogDomain, kAppLogTag,
                 "surface created size=%{public}" PRIu64 "x%{public}" PRIu64 " ready=%{public}d",
                 g_renderer.width, g_renderer.height, g_renderer.ready);
    if (g_renderer.ready) {
        RenderFrame(g_renderer);
    }
}

void OnSurfaceChanged(OH_NativeXComponent* component, void* window) {
    OH_LOG_Print(LOG_APP, LOG_INFO, kAppLogDomain, kAppLogTag, "OnSurfaceChanged");
    std::lock_guard<std::mutex> lock(g_mutex);
    g_renderer.component = component;
    g_renderer.nativeWindow = static_cast<OHNativeWindow*>(window);
    OH_NativeXComponent_GetXComponentSize(component, window, &g_renderer.width, &g_renderer.height);
    g_renderer.skSurface.reset();
    RenderFrame(g_renderer);
}

void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window) {
    (void)component;
    (void)window;
    OH_LOG_Print(LOG_APP, LOG_INFO, kAppLogDomain, kAppLogTag, "OnSurfaceDestroyed");
    std::lock_guard<std::mutex> lock(g_mutex);
    DestroyRenderer(g_renderer);
}

void DispatchTouchEvent(OH_NativeXComponent* component, void* window) {
    (void)component;
    (void)window;
    std::lock_guard<std::mutex> lock(g_mutex);
    RenderFrame(g_renderer);
}

OH_NativeXComponent_Callback g_callback = {
    .OnSurfaceCreated = OnSurfaceCreated,
    .OnSurfaceChanged = OnSurfaceChanged,
    .OnSurfaceDestroyed = OnSurfaceDestroyed,
    .DispatchTouchEvent = DispatchTouchEvent,
};

}  // namespace

bool RegisterXComponentCallbacksFromValue(napi_env env, napi_value holder) {
    napi_value nativeXComponentValue = nullptr;
    napi_status status = napi_get_named_property(env, holder, OH_NATIVE_XCOMPONENT_OBJ, &nativeXComponentValue);
    if (status != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_WARN, kAppLogDomain, kAppLogTag,
                     "native xcomponent object not found on holder, status=%{public}d", status);
        return false;
    }

    void* nativeXComponent = nullptr;
    status = napi_unwrap(env, nativeXComponentValue, &nativeXComponent);
    if (status != napi_ok || nativeXComponent == nullptr) {
        status = napi_get_value_external(env, nativeXComponentValue, &nativeXComponent);
    }

    if (status != napi_ok || nativeXComponent == nullptr) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag,
                     "failed to acquire native xcomponent pointer");
        return false;
    }

    int32_t result =
        OH_NativeXComponent_RegisterCallback(static_cast<OH_NativeXComponent*>(nativeXComponent), &g_callback);
    if (result != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag,
                     "OH_NativeXComponent_RegisterCallback failed: %{public}d", result);
        return false;
    }

    g_renderer.component = static_cast<OH_NativeXComponent*>(nativeXComponent);
    g_renderer.callbacksRegistered = true;
    OH_LOG_Print(LOG_APP, LOG_INFO, kAppLogDomain, kAppLogTag, "native xcomponent callbacks registered");
    return true;
}

napi_value RegisterSurface(napi_env env, napi_callback_info info) {
    size_t argc = 0;
    napi_value thisArg = nullptr;
    if (napi_get_cb_info(env, info, &argc, nullptr, &thisArg, nullptr) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "napi_get_cb_info failed");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_renderer.callbacksRegistered) {
        RegisterXComponentCallbacksFromValue(env, thisArg);
    }
    return nullptr;
}

napi_value RenderNow(napi_env env, napi_callback_info info) {
    size_t argc = 0;
    napi_value thisArg = nullptr;
    if (napi_get_cb_info(env, info, &argc, nullptr, &thisArg, nullptr) != napi_ok) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "napi_get_cb_info failed in renderNow");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_renderer.callbacksRegistered) {
        RegisterXComponentCallbacksFromValue(env, thisArg);
    }
    RenderFrame(g_renderer);
    return nullptr;
}

}  // namespace skia_ohos
