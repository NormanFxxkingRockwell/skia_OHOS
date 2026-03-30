#include "renderer/skia_scene_renderer.h"

#include <cstring>
#include <memory>

#include <hilog/log.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkTextBlob.h"
#include "include/ports/SkFontMgr_ohos.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skshaper/include/SkShaper_skunicode.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#include "renderer/render_log.h"

namespace skia_ohos {
namespace {

constexpr const char* kAltScFont = "/system/fonts/FZHeiT-SC-Regular.ttf";
constexpr const char* kCjkFont = "/system/fonts/NotoSansCJK-Regular.ttc";

sk_sp<SkTextBlob> MakeShapedBlob(sk_sp<SkFontMgr> fontMgr,
                                 sk_sp<SkTypeface> typeface,
                                 const char* text,
                                 SkScalar fontSize) {
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

    SkTextBlobBuilderRunHandler builder(text, {0.0f, 0.0f});
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

}  // namespace

bool EnsureTypeface(RendererState& state) {
    if (state.typeface) {
        return true;
    }

    if (!state.fontMgr) {
        state.fontMgr = SkFontMgr_New_OHOS();
        if (!state.fontMgr || state.fontMgr->countFamilies() == 0) {
            OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag,
                         "failed to load fonts from SkFontMgr_New_OHOS");
            return false;
        }
    }

    state.typeface = state.fontMgr->legacyMakeTypeface(nullptr, SkFontStyle());
    if (!state.typeface) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag,
                     "failed to create typeface from SkFontMgr_New_OHOS");
        return false;
    }

    OH_LOG_Print(LOG_APP, LOG_INFO, kAppLogDomain, kAppLogTag,
                 "font manager ready families=%{public}d source=SkFontMgr_New_OHOS",
                 state.fontMgr->countFamilies());
    return true;
}

void DrawGpuScene(RendererState& state) {
    if (!state.skSurface) {
        OH_LOG_Print(LOG_APP, LOG_ERROR, kAppLogDomain, kAppLogTag, "skSurface unavailable");
        return;
    }
    if (!EnsureTypeface(state)) {
        return;
    }

    sk_sp<SkTypeface> altTypeface = state.fontMgr->makeFromFile(kAltScFont);
    sk_sp<SkTypeface> cjkTypeface = state.fontMgr->makeFromFile(kCjkFont);

    SkCanvas* canvas = state.skSurface->getCanvas();
    canvas->clear(SkColorSetRGB(14, 22, 30));

    const float wf = static_cast<float>(state.width);
    const float hf = static_cast<float>(state.height);
    const float padding = std::max(24.0f, wf * 0.035f);

    SkPaint cardPaint;
    cardPaint.setAntiAlias(true);
    cardPaint.setColor(SkColorSetARGB(255, 33, 56, 72));
    canvas->drawRoundRect(SkRect::MakeXYWH(padding, padding, wf - padding * 2.0f, hf - padding * 2.0f),
                          26.0f, 26.0f, cardPaint);

    SkPaint bannerPaint;
    bannerPaint.setAntiAlias(true);
    bannerPaint.setColor(SkColorSetRGB(208, 232, 255));
    canvas->drawRoundRect(
        SkRect::MakeXYWH(padding + 22.0f, padding + 22.0f, wf - padding * 2.0f - 44.0f, 84.0f),
        18.0f, 18.0f, bannerPaint);

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
        sk_sp<SkTextBlob> blob =
            MakeShapedBlob(state.fontMgr, face ? face : state.typeface,
                           u8"中文字体渲染验证 ABC 123", sampleFontSize);
        if (blob) {
            canvas->drawTextBlob(blob.get(), padding + 36.0f, y + 34.0f, bodyPaint);
            return;
        }

        SkFont fallbackFont(face ? face : state.typeface, sampleFontSize);
        fallbackFont.setSubpixel(true);
        fallbackFont.setEdging(SkFont::Edging::kSubpixelAntiAlias);
        canvas->drawString("shaping unavailable", padding + 36.0f, y + 34.0f, fallbackFont, bodyPaint);
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
                       padding + 36.0f, hf - padding - 28.0f, footerFont, footerPaint);
}

}  // namespace skia_ohos
