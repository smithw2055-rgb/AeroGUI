#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Rendering.hpp>
#include <Aero/Text/TextLayout.hpp>
#include <Aero/Text/TextTypes.hpp>

namespace Aero::Controls::Detail {

struct TextLayoutRequest final {
    Base::StringView text;
    Aero::Size availableSize;
    double dpiScale = 1.0;
    float pixelSize = 16.0F;
    float lineHeight = 0.0F;
    Base::StringView fontFamily;
    Text::FontFace face;
    Text::TextWrapping wrapping =
        Text::TextWrapping::NoWrap;
    Text::TextTrimming trimming =
        Text::TextTrimming::None;
    Text::TextAlignment alignment =
        Text::TextAlignment::Start;
};

struct TextLayoutResult final {
    explicit TextLayoutResult(
        Base::IAllocator* allocator = nullptr) noexcept
        : glyphRuns(allocator), hitRegions(allocator) {}

    Base::Vector<Render::RenderGlyphRunId> glyphRuns;
    Base::Vector<Text::TextHitRegion> hitRegions;
    Aero::Size desiredSize;
};

class TextLayoutService {
public:
    virtual ~TextLayoutService() = default;

    virtual Base::Result<void> ShapeAndPrepare(
        const TextLayoutRequest& request,
        TextLayoutResult& output) noexcept = 0;
    virtual void ReleaseGlyphRun(
        Render::RenderGlyphRunId glyphRun) noexcept = 0;
};

} // namespace Aero::Controls::Detail
