#ifndef SKIA_OHOS_XCOMPONENT_BRIDGE_H
#define SKIA_OHOS_XCOMPONENT_BRIDGE_H

#include <js_native_api.h>

namespace skia_ohos {

bool RegisterXComponentCallbacksFromValue(napi_env env, napi_value holder);
napi_value RegisterSurface(napi_env env, napi_callback_info info);
napi_value RenderNow(napi_env env, napi_callback_info info);

}  // namespace skia_ohos

#endif  // SKIA_OHOS_XCOMPONENT_BRIDGE_H
