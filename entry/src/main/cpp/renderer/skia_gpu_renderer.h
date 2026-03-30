#ifndef SKIA_OHOS_SKIA_GPU_RENDERER_H
#define SKIA_OHOS_SKIA_GPU_RENDERER_H

#include "renderer/renderer_state.h"

namespace skia_ohos {

bool InitEgl(RendererState& state);
void DestroyRenderer(RendererState& state);
void RenderFrame(RendererState& state);

}  // namespace skia_ohos

#endif  // SKIA_OHOS_SKIA_GPU_RENDERER_H
