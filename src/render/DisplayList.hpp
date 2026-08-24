#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/DrawingContext.hpp>

#include <cstdint>

namespace Aero::Render { class RenderTree; }

namespace Aero::Render {

using RenderNodeId = Base::RenderNodeId;
inline constexpr RenderNodeId InvalidRenderNodeId =
    Base::InvalidRenderNodeId;
using RenderImageId = std::uint64_t;
inline constexpr RenderImageId InvalidRenderImageId = 0U;
using RenderMeshId = std::uint64_t;
inline constexpr RenderMeshId InvalidRenderMeshId = 0U;
using RenderGlyphRunId = std::uint64_t;
inline constexpr RenderGlyphRunId InvalidRenderGlyphRunId = 0U;
using Color = Base::Color;
using Transform2D = Base::Transform2D;
using Rect = Base::Rect;
using Point = Base::Point;

bool IsFinite(Color value) noexcept;
bool IsFinite(Transform2D value) noexcept;
bool IsValidOpacity(double value) noexcept;

enum class RenderCommandKind : std::uint8_t {
    PushClip = 0U,
    PopClip,
    PushOpacity,
    PopOpacity,
    PushTransform,
    PopTransform,
    FillRect,
    FillRoundedRect,
    StrokeRect,
    DrawImage,
    DrawMesh,
    DrawGlyphRun,
    FillGradientQuad
};

struct RenderCommand {
    RenderCommandKind kind = RenderCommandKind::FillRect;
    Rect rect;
    Transform2D transform;
    Color color;
    Point points[4]{};
    Color colors[4]{};
    Rect sourceUv;
    RenderImageId image = InvalidRenderImageId;
    RenderMeshId mesh = InvalidRenderMeshId;
    RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId;
    double scalar = 0.0;
    double cornerRadius = 0.0;
};

class DisplayList {
public:
    DisplayList() noexcept = default;

    Base::Span<const RenderCommand> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    std::uint32_t CommandCount() const noexcept {
        return commands_.Size();
    }
    std::uint64_t StableHash() const noexcept;

private:
    friend class DisplayListBuilder;
    friend class ::Aero::Render::RenderTree;
    Base::Vector<RenderCommand> commands_;
};

class DisplayListBuilder {
public:
    DisplayListBuilder() noexcept = default;

    Base::Result<void> PushClip(Rect clip) noexcept;
    Base::Result<void> PopClip() noexcept;
    Base::Result<void> PushOpacity(double opacity) noexcept;
    Base::Result<void> PopOpacity() noexcept;
    Base::Result<void> PushTransform(Transform2D value) noexcept;
    Base::Result<void> PopTransform() noexcept;
    Base::Result<void> FillRect(Rect rect, Color color) noexcept;
    Base::Result<void> FillRoundedRect(
        Rect rect, Color color, double cornerRadius) noexcept;
    Base::Result<void> FillGradientQuad(
        const Point points[4], const Color colors[4]) noexcept;
    Base::Result<void> StrokeRect(
        Rect rect, Color color, double thickness,
        double cornerRadius = 0.0) noexcept;
    Base::Result<void> DrawImage(
        RenderImageId image,
        Rect destination,
        Rect sourceUv,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    Base::Result<void> DrawMesh(
        RenderMeshId mesh,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    Base::Result<void> DrawGlyphRun(
        RenderGlyphRunId glyphRun,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    Base::Result<DisplayList> Finish() noexcept;

private:
    DisplayList list_;
    std::uint32_t clipDepth_ = 0U;
    std::uint32_t opacityDepth_ = 0U;
    std::uint32_t transformDepth_ = 0U;
    bool finished_ = false;

    Base::Result<void> Append(
        const RenderCommand& command) noexcept;
};

} // namespace Aero::Render
namespace Aero {

// Source-private bridge used by FrameworkElement::OnRender implementations.
// Keep it next to DisplayListBuilder instead of recreating a broad render
// umbrella header.
struct Media::DrawingContextRuntime {
    static ::Aero::Media::DrawingContext Create(
        Render::DisplayListBuilder& builder) noexcept {
        return ::Aero::Media::DrawingContext(&builder);
    }

    static Render::DisplayListBuilder& Builder(
        ::Aero::Media::DrawingContext& context) noexcept {
        return *static_cast<Render::DisplayListBuilder*>(
            context.implementation_);
    }
};

} // namespace Aero

namespace Aero::Render {
using DrawingPrivate = ::Aero::Media::DrawingContextRuntime;
} // namespace Aero::Render
