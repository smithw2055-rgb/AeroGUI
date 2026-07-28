#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/Rendering.hpp>

namespace Aero::Controls::Detail {

struct TextLayoutRequest final {
    Base::StringView text;
    Presentation::Size availableSize;
    double dpiScale = 1.0;
};

struct TextLayoutResult final {
    explicit TextLayoutResult(
        Base::IAllocator* allocator = nullptr) noexcept
        : glyphRuns(allocator) {}

    Base::Vector<Presentation::RenderGlyphRunId> glyphRuns;
    Presentation::Size desiredSize;
};

class TextLayoutService {
public:
    virtual ~TextLayoutService() = default;

    virtual Base::Result<void> ShapeAndPrepare(
        const TextLayoutRequest& request,
        TextLayoutResult& output) noexcept = 0;
    virtual void ReleaseGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept = 0;
};

} // namespace Aero::Controls::Detail
