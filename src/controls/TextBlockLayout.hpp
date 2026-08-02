#pragma once

#include "../render/DisplayList.hpp"

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/FrameworkElement.hpp>
#include "../text/TextLayout.hpp"
#include "../text/TextTypes.hpp"

namespace Aero::Internal {

struct TextLayoutRequest final {
    Base::StringView text;
    Aero::Size availableSize;
    double dpiScale = 1.0;
    float pixelSize = 16.0F;
    float lineHeight = 0.0F;
    Base::StringView fontFamily;
    Text::FontFace face;
    TextWrapping wrapping =
        TextWrapping::NoWrap;
    TextTrimming trimming =
        TextTrimming::None;
    TextAlignment alignment =
        TextAlignment::Start;
};

struct TextLayoutResult final {
    explicit TextLayoutResult(
        Base::IAllocator* allocator = nullptr) noexcept
        : glyphRuns(allocator), hitRegions(allocator) {}

    Base::Vector<Render::RenderGlyphRunId> glyphRuns;
    Base::Vector<TextHitRegion> hitRegions;
    Aero::Size desiredSize;
};

class TextBlockLayout {
public:
    virtual ~TextBlockLayout() = default;

    virtual Base::Result<void> ShapeAndPrepare(
        const TextLayoutRequest& request,
        TextLayoutResult& output) noexcept = 0;
    virtual void ReleaseGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept = 0;
};

} // namespace Aero::Internal
