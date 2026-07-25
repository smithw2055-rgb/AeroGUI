#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/Rendering.hpp>

namespace Aero::Controls {

struct TextBlockLayoutRequest final {
    Base::StringView text;
    Presentation::Size availableSize;
    double dpiScale = 1.0;
};

struct TextBlockLayoutResult final {
    explicit TextBlockLayoutResult(
        Base::IAllocator* allocator = nullptr) noexcept
        : glyphRuns(allocator) {}

    Base::Vector<Presentation::RenderGlyphRunId> glyphRuns;
    Presentation::Size desiredSize;
};

// Host-owned bridge between TextBlock and the selected text/render stack.
// Implementations must outlive every TextBlock that captures them and must
// keep returned glyph-run resources alive until ReleaseGlyphRun is called.
// Every ID in a successful result transfers one lease, including IDs that
// also appeared in an earlier result.
class AERO_API ITextBlockLayoutService {
public:
    virtual ~ITextBlockLayoutService() = default;

    virtual Base::Result<void> ShapeAndPrepare(
        const TextBlockLayoutRequest& request,
        TextBlockLayoutResult& output) noexcept = 0;
    virtual void ReleaseGlyphRun(
        Presentation::RenderGlyphRunId glyphRun) noexcept = 0;
};

AERO_API ITextBlockLayoutService*
GetCurrentTextBlockLayoutService() noexcept;

class AERO_API TextBlockLayoutServiceScope final {
public:
    explicit TextBlockLayoutServiceScope(
        ITextBlockLayoutService& service) noexcept;
    ~TextBlockLayoutServiceScope();

    TextBlockLayoutServiceScope(
        const TextBlockLayoutServiceScope&) = delete;
    TextBlockLayoutServiceScope& operator=(
        const TextBlockLayoutServiceScope&) = delete;
    TextBlockLayoutServiceScope(
        TextBlockLayoutServiceScope&&) = delete;
    TextBlockLayoutServiceScope& operator=(
        TextBlockLayoutServiceScope&&) = delete;

private:
    ITextBlockLayoutService* service_ = nullptr;
    ITextBlockLayoutService* previous_ = nullptr;
    Core::DispatcherThreadToken ownerThread_ = 0U;
};

} // namespace Aero::Controls
