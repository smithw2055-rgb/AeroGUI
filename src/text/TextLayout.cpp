#include <Aero/Text/TextLayout.hpp>

#include <cmath>
#include <utility>

namespace Aero::Text {

Base::Result<void> TextLayout::TryAddRun(GlyphRun&& run) noexcept {
    if (!run.face.IsValid() ||
        !std::isfinite(run.pixelSize) ||
        run.pixelSize <= 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Glyph run requires a valid face and positive pixel size");
    }
    for (const PositionedGlyph& glyph : run.glyphs) {
        if (glyph.glyph == InvalidGlyphId ||
            !std::isfinite(glyph.x) ||
            !std::isfinite(glyph.y)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Glyph run contains invalid positioned glyph data");
        }
    }
    Base::Result<GlyphRun*> appended =
        runs_.TryEmplaceBack(std::move(run));
    return appended
        ? Base::Result<void>()
        : Base::Result<void>(appended.GetStatus());
}

Base::Result<void> TextLayout::TryAddLine(
    const TextLine& line) noexcept {
    if (line.firstRun > runs_.Size() ||
        line.runCount > runs_.Size() - line.firstRun ||
        !std::isfinite(line.width) ||
        !std::isfinite(line.ascent) ||
        !std::isfinite(line.descent) ||
        !std::isfinite(line.baseline) ||
        !std::isfinite(line.y) ||
        line.width < 0.0F ||
        line.ascent < 0.0F ||
        line.descent < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text line range or metrics are invalid");
    }
    return lines_.TryPushBack(line);
}

Base::Result<void> TextLayout::SetSize(
    TextLayoutSize size) noexcept {
    if (!std::isfinite(size.width) ||
        !std::isfinite(size.height) ||
        size.width < 0.0F ||
        size.height < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text layout size must be finite and nonnegative");
    }
    size_ = size;
    return {};
}

void TextLayout::Clear() noexcept {
    runs_.Clear();
    lines_.Clear();
    size_ = {};
}

} // namespace Aero::Text
