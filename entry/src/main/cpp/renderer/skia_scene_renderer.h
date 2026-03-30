#ifndef SKIA_OHOS_SKIA_SCENE_RENDERER_H
#define SKIA_OHOS_SKIA_SCENE_RENDERER_H

#include "renderer/renderer_state.h"

namespace skia_ohos {

bool EnsureTypeface(RendererState& state);
void DrawGpuScene(RendererState& state);

}  // namespace skia_ohos

#endif  // SKIA_OHOS_SKIA_SCENE_RENDERER_H
