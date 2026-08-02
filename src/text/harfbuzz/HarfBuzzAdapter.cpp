#include "../HarfBuzzAdapter.hpp"

#include "../FreeTypeAdapter.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb-ft.h>
#include <hb.h>

#include <cmath>
#include <cstdint>
#include <limits>

namespace Aero::Text {
namespace {

float From26Dot6(hb_position_t value) noexcept {
    return static_cast<float>(value) / 64.0F;
}

hb_direction_t ToHarfBuzzDirection(
    TextDirection direction) noexcept {
    switch (direction) {
    case TextDirection::LeftToRight:
        return HB_DIRECTION_LTR;
    case TextDirection::RightToLeft:
        return HB_DIRECTION_RTL;
    case TextDirection::Auto:
        return HB_DIRECTION_INVALID;
    }
    return HB_DIRECTION_INVALID;
}

TextDirection FromHarfBuzzDirection(
    hb_direction_t direction) noexcept {
    return HB_DIRECTION_IS_BACKWARD(direction)
        ? TextDirection::RightToLeft
        : TextDirection::LeftToRight;
}

hb_script_t ToHarfBuzzScript(Script script) noexcept {
    switch (script) {
    case Script::Common: return HB_SCRIPT_COMMON;
    case Script::Inherited: return HB_SCRIPT_INHERITED;
    case Script::Latin: return HB_SCRIPT_LATIN;
    case Script::Han: return HB_SCRIPT_HAN;
    case Script::Arabic: return HB_SCRIPT_ARABIC;
    case Script::Cyrillic: return HB_SCRIPT_CYRILLIC;
    case Script::Greek: return HB_SCRIPT_GREEK;
    case Script::Hebrew: return HB_SCRIPT_HEBREW;
    case Script::Unknown: return HB_SCRIPT_INVALID;
    }
    return HB_SCRIPT_INVALID;
}

Script FromHarfBuzzScript(hb_script_t script) noexcept {
    if (script == HB_SCRIPT_COMMON) return Script::Common;
    if (script == HB_SCRIPT_INHERITED) return Script::Inherited;
    if (script == HB_SCRIPT_LATIN) return Script::Latin;
    if (script == HB_SCRIPT_HAN) return Script::Han;
    if (script == HB_SCRIPT_ARABIC) return Script::Arabic;
    if (script == HB_SCRIPT_CYRILLIC) return Script::Cyrillic;
    if (script == HB_SCRIPT_GREEK) return Script::Greek;
    if (script == HB_SCRIPT_HEBREW) return Script::Hebrew;
    return Script::Unknown;
}

Base::Result<FT_UInt> ToPixelSize(float pixelSize) noexcept {
    const double scaled = static_cast<double>(pixelSize);
    if (!std::isfinite(scaled) ||
        scaled > static_cast<double>(
            std::numeric_limits<FT_UInt>::max())) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Requested pixel size exceeds FreeType limits");
    }
    return static_cast<FT_UInt>(std::ceil(scaled));
}

} // namespace

bool HarfBuzzAdapter::Supports(
    FontProviderIdentity provider) const noexcept {
    return fonts_ != nullptr &&
        fonts_->IsInitialized() &&
        provider == fonts_->Identity();
}

Base::Result<void> HarfBuzzAdapter::Shape(
    const ShapingRequest& request,
    ShapedTextRun& output) noexcept {
    if (fonts_ == nullptr || !fonts_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "HarfBuzz adapter has no initialized FreeType provider");
    }
    if (request.text.SizeBytes() >
            static_cast<std::uint32_t>(
                std::numeric_limits<int>::max()) ||
        request.language.SizeBytes() >
            static_cast<std::uint32_t>(
                std::numeric_limits<int>::max())) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "HarfBuzz input exceeds its signed length limit");
    }
    auto* face = static_cast<FT_Face>(
        fonts_->FindNativeFace(request.face));
    if (face == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Font face is not loaded");
    }
    const Base::Result<FT_UInt> pixelSize =
        ToPixelSize(request.pixelSize);
    if (!pixelSize) return pixelSize.GetStatus();
    if (FT_Set_Pixel_Sizes(
            face, 0U, pixelSize.Value()) != 0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "FreeType pixel-size selection failed");
    }

    hb_font_t* font = hb_ft_font_create_referenced(face);
    if (font == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "HarfBuzz font creation failed");
    }
    hb_buffer_t* buffer = hb_buffer_create();
    if (buffer == nullptr) {
        hb_font_destroy(font);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "HarfBuzz buffer creation failed");
    }

    hb_buffer_add_utf8(
        buffer, request.text.Empty() ? "" : request.text.Data(),
        static_cast<int>(request.text.SizeBytes()), 0, -1);
    const hb_direction_t direction =
        ToHarfBuzzDirection(request.direction);
    if (direction != HB_DIRECTION_INVALID) {
        hb_buffer_set_direction(buffer, direction);
    }
    const hb_script_t script = ToHarfBuzzScript(request.script);
    if (script != HB_SCRIPT_INVALID) {
        hb_buffer_set_script(buffer, script);
    }
    if (!request.language.Empty()) {
        hb_buffer_set_language(buffer, hb_language_from_string(
            request.language.Data(),
            static_cast<int>(request.language.SizeBytes())));
    }
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font, buffer, nullptr, 0U);

    unsigned int count = 0U;
    const hb_glyph_info_t* infos =
        hb_buffer_get_glyph_infos(buffer, &count);
    const hb_glyph_position_t* positions =
        hb_buffer_get_glyph_positions(buffer, &count);
    output.face = request.face;
    output.direction = FromHarfBuzzDirection(
        hb_buffer_get_direction(buffer));
    output.script = FromHarfBuzzScript(
        hb_buffer_get_script(buffer));
    Base::Result<void> reserve =
        output.glyphs.Reserve(count);
    if (!reserve) {
        hb_buffer_destroy(buffer);
        hb_font_destroy(font);
        return reserve.GetStatus();
    }
    for (unsigned int index = 0U; index < count; ++index) {
        ShapedGlyph glyph;
        glyph.glyph = infos[index].codepoint;
        glyph.cluster = infos[index].cluster;
        glyph.advanceX = From26Dot6(positions[index].x_advance);
        glyph.offsetX = From26Dot6(positions[index].x_offset);
        glyph.offsetY = From26Dot6(positions[index].y_offset);
        Base::Result<void> appended =
            output.glyphs.PushBack(glyph);
        if (!appended) {
            hb_buffer_destroy(buffer);
            hb_font_destroy(font);
            return appended.GetStatus();
        }
    }
    hb_buffer_destroy(buffer);
    hb_font_destroy(font);
    return {};
}

} // namespace Aero::Text
