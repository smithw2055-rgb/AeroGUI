#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/DrawingContext.hpp>

#include <cstdint>
#include <cstddef>
#include <cstring>

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
using ProjectiveTransform2D = Base::ProjectiveTransform2D;
using Rect = Base::Rect;
using Point = Base::Point;

bool IsFinite(Color value) noexcept;
bool IsFinite(Transform2D value) noexcept;
bool IsFinite(ProjectiveTransform2D value) noexcept;
bool IsValidOpacity(double value) noexcept;

enum class RenderWrapMode : std::uint8_t {
    ClampToEdge = 0U,
    ClampToZero = 1U,
    Repeat = 2U,
    MirrorU = 3U,
    MirrorV = 4U,
    Mirror = 5U
};

enum class RenderCommandKind : std::uint8_t {
    PushClip = 0U,
    PopClip,
    PushOpacity,
    PopOpacity,
    PushTransform,
    PopTransform,
    FillRect,                 // CmdFillRect (24B)
    FillRoundedRect,          // CmdRoundedRect (28B)
    StrokeRect,               // CmdStrokeRect (32B)
    DrawAnalyticalShadow,     // CmdAnalyticalShadow (36B)
    DrawImage,                // CmdDrawImage (48B)
    DrawGlyphRun,             // CmdDrawGlyphRun (24B)
    DrawTriangles,            // CmdDrawTriangles (24B + arrays)
    FillGradientQuad,         // CmdGradientQuad (160B)
    DrawSolidMesh,            // CmdDrawSolidMesh (24B)
    DrawGradientMesh,         // CmdDrawGradientMesh (120B)
    DrawMesh = DrawSolidMesh  // Backward-compatibility alias
};

// Fixed 8-byte command header
struct alignas(4) RenderCommandHeader {
    RenderCommandKind kind = RenderCommandKind::FillRect;
    std::uint8_t paintKind = 0U; // 0=Solid, 1=Linear, 2=Radial
    std::uint8_t wrapMode = 0U;
    std::uint8_t flags = 0U;
    std::uint32_t payloadOffset = 0U; // Byte offset into DisplayList payload
};

struct CmdPushClip {
    Rect rect;
};

struct CmdPushOpacity {
    double opacity = 1.0;
};

struct CmdPushTransform {
    ProjectiveTransform2D transform;
};

struct CmdFillRect {
    Rect rect;
    Color color;
};

struct CmdRoundedRect {
    Rect rect;
    Color color;
    float radius = 0.0f;
    float pad = 0.0f;
};

struct CmdStrokeRect {
    Rect rect;
    Color color;
    float thickness = 1.0f;
    float cornerRadius = 0.0f;
};

struct CmdAnalyticalShadow {
    Rect rect;
    Color color;
    float cornerRadius = 0.0f;
    float blurRadius = 0.0f;
    Point offset;
};

struct CmdDrawImage {
    RenderImageId image = InvalidRenderImageId;
    Rect destination;
    Rect sourceUv;
    Color tint{1.0F, 1.0F, 1.0F, 1.0F};
};

struct CmdDrawGlyphRun {
    RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId;
    Color tint{1.0F, 1.0F, 1.0F, 1.0F};
};

struct CmdGradientQuad {
    Point points[4]{};
    Color colors[4]{};
    Point uvs[4]{};
    std::uint32_t gradientRamp = UINT32_MAX;
    std::uintptr_t gradientBrush = 0U;
    float paintUniforms[8]{};
};

struct CmdDrawTriangles {
    Color color;
    std::uint32_t vertexOffset = 0U;
    std::uint32_t vertexCount = 0U;
    std::uint32_t indexOffset = 0U;
    std::uint32_t indexCount = 0U;
};

struct CmdDrawSolidMesh {
    RenderMeshId mesh = InvalidRenderMeshId;
    Color tint{1.0F, 1.0F, 1.0F, 1.0F};
};

struct CmdDrawGradientMesh {
    RenderMeshId mesh = InvalidRenderMeshId;
    Color tint{1.0F, 1.0F, 1.0F, 1.0F};
    float paintUniforms[8]{};
    std::uintptr_t gradientBrush = 0U;
    std::uint32_t gradientRamp = UINT32_MAX;
    Rect bounds;
    Point startOrOrigin;
    Point delta;
    Point radii;
    double len2 = 0.0;
    bool hasInverse = false;
    Transform2D inverse;
};

class DisplayList {
public:
    DisplayList() noexcept = default;

    Base::Span<const RenderCommandHeader> Headers() const noexcept {
        return {headers_.Data(), headers_.Size()};
    }
    template <typename T>
    const T& PayloadAt(std::uint32_t offset) const noexcept {
        return *reinterpret_cast<const T*>(payload_.Data() + offset);
    }
    template <typename T>
    T& MutablePayloadAt(std::uint32_t offset) noexcept {
        return *reinterpret_cast<T*>(payload_.Data() + offset);
    }
    Base::Span<const std::uint8_t> Payload() const noexcept {
        return {payload_.Data(), payload_.Size()};
    }
    const std::uint8_t* PayloadData() const noexcept {
        return payload_.Data();
    }
    std::uint32_t CommandCount() const noexcept {
        return headers_.Size();
    }
    std::uint64_t StableHash() const noexcept;

private:
    friend class DisplayListBuilder;
    friend class ::Aero::Render::RenderTree;
    Base::Vector<RenderCommandHeader> headers_;
    Base::Vector<std::uint8_t> payload_;
};

class DisplayListBuilder {
public:
    DisplayListBuilder() noexcept = default;

    void PushClip(Rect clip) noexcept;
    void PopClip() noexcept;
    void PushOpacity(double opacity) noexcept;
    void PopOpacity() noexcept;
    void PushTransform(Transform2D value) noexcept;
    void PushTransform(ProjectiveTransform2D value) noexcept;
    void PopTransform() noexcept;
    void FillRect(Rect rect, Color color) noexcept;
    void FillRoundedRect(
        Rect rect, Color color, double cornerRadius) noexcept;
    void FillGradientQuad(
        const Point points[4], const Color colors[4]) noexcept;
    void FillGradientQuad(
        const Point points[4],
        const Point uvs[4],
        std::uint8_t paintKind,
        const float uniforms[8],
        std::uintptr_t gradientBrush,
        std::uint8_t wrapMode = 0U) noexcept;
    void StrokeRect(
        Rect rect, Color color, double thickness,
        double cornerRadius = 0.0) noexcept;
    void DrawAnalyticalShadow(
        Rect rect, Color color, float cornerRadius, float blurRadius, Point offset) noexcept;
    void DrawImage(
        RenderImageId image,
        Rect destination,
        Rect sourceUv,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F},
        std::uint8_t wrapMode = 0U) noexcept;
    void DrawMesh(
        RenderMeshId mesh,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    void DrawMesh(
        RenderMeshId mesh,
        std::uint8_t paintKind,
        const float uniforms[8],
        std::uintptr_t gradientBrush,
        Rect bounds,
        Point startOrOrigin,
        Point delta,
        Point radii,
        double len2,
        bool hasInverse,
        Transform2D inverse,
        std::uint8_t wrapMode = 0U) noexcept;
    void DrawGlyphRun(
        RenderGlyphRunId glyphRun,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    void DrawTriangles(
        const Point* vertices,
        std::uint32_t vertexCount,
        const std::uint32_t* indices,
        std::uint32_t indexCount,
        Color color) noexcept;
    Base::Result<DisplayList> Finish() noexcept;

    std::uint32_t AppendRawBytes(const void* src, size_t size) noexcept {
        if (size == 0U || src == nullptr) return static_cast<std::uint32_t>(list_.payload_.Size());
        const size_t currentSize = list_.payload_.Size();
        const size_t alignedOffset = (currentSize + 7U) & ~size_t(7U);
        list_.payload_.Resize(static_cast<std::uint32_t>(alignedOffset + size));
        std::memcpy(list_.payload_.Data() + alignedOffset, src, size);
        return static_cast<std::uint32_t>(alignedOffset);
    }

    template <typename T>
    std::uint32_t AppendPayload(const T& data) noexcept {
        return AppendRawBytes(&data, sizeof(T));
    }

private:
    DisplayList list_;
    std::uint32_t clipDepth_ = 0U;
    std::uint32_t opacityDepth_ = 0U;
    std::uint32_t transformDepth_ = 0U;
    bool finished_ = false;
};

} // namespace Aero::Render

namespace Aero::Media {

// Source-private bridge used by FrameworkElement::OnRender implementations.
// Nested on DrawingContext so no parallel *Runtime companion is exported.
struct DrawingContext::Bridge {
    static DrawingContext Create(
        ::Aero::Render::DisplayListBuilder& builder) noexcept {
        return DrawingContext(&builder);
    }

    static ::Aero::Render::DisplayListBuilder& Builder(
        DrawingContext& context) noexcept {
        return *static_cast<::Aero::Render::DisplayListBuilder*>(
            context.implementation_);
    }
};

} // namespace Aero::Media

namespace Aero::Render {
using DrawingBridge = ::Aero::Media::DrawingContext::Bridge;
} // namespace Aero::Render
