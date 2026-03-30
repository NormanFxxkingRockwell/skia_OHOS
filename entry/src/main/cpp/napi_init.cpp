#include <js_native_api.h>
#include <napi/native_api.h>

#include "renderer/xcomponent_bridge.h"

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "registerSurface", nullptr, skia_ohos::RegisterSurface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "renderNow", nullptr, skia_ohos::RenderNow, nullptr, nullptr, nullptr, napi_default, nullptr }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    skia_ohos::RegisterXComponentCallbacksFromValue(env, exports);
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
