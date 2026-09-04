#pragma once

#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"

#include "DisplayList.hpp"

#include <Aero/FrameworkElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>

#include <array>

namespace Aero::Render {

enum class RenderEffectKind : std::uint8_t {
    None = 0U,
    Blur,
    DropShadow,
    Pixelate,
    Tint,
    DirectionalBlur,
    Custom
};

enum class RenderInvalidation : std::uint8_t {
    None = 0U,
    State = 1U << 0U,
    Drawing = 1U << 1U,
    Children = 1U << 2U,
    All = 0x07U
};

constexpr RenderInvalidation operator|(
    RenderInvalidation left,
    RenderInvalidation right) noexcept {
    return static_cast<RenderInvalidation>(
        static_cast<std::uint8_t>(left) |
        static_cast<std::uint8_t>(right));
}

constexpr bool HasRenderInvalidation(
    RenderInvalidation value,
    RenderInvalidation flag) noexcept {
    return (static_cast<std::uint8_t>(value) &
        static_cast<std::uint8_t>(flag)) != 0U;
}

struct RenderEffectSnapshot {
    RenderEffectKind kind = RenderEffectKind::None;
    double radius = 0.0;
    double direction = 315.0;
    double depth = 0.0;
    double opacity = 1.0;
    Color color{0.0F, 0.0F, 0.0F, 1.0F};
    double size = 1.0;
    std::uint32_t shaderId = 0U;
    std::array<float, 16> uniforms{};
    std::uint32_t uniformCount = 0U;
    Base::StringView shaderSource{};
    Base::Span<const std::uint8_t> bytecode{};
};

enum class RenderMaskKind : std::uint8_t {
    None = 0U,
    Solid,
    Image,
    LinearGradient,
    RadialGradient
};

inline constexpr std::uint32_t GradientRampWidth = 256U;

struct RenderGradientRampSnapshot {
    std::uintptr_t brushIdentity = 0U;
    std::uint64_t revision = 0U;
    std::array<std::uint8_t, GradientRampWidth * 4U> pixels{};
};

struct RenderMaskSnapshot {
    RenderMaskKind kind = RenderMaskKind::None;
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
    RenderImageId image = InvalidRenderImageId;
    Rect sourceUv{0.0, 0.0, 1.0, 1.0};
    Rect viewport{0.0, 0.0, 1.0, 1.0};
    Point startPoint{0.0, 0.0};
    Point endPoint{1.0, 1.0};
    Point center{0.5, 0.5};
    Point gradientOrigin{0.5, 0.5};
    Transform2D relativeTransform;
    double radiusX = 0.5;
    double radiusY = 0.5;
    std::uint32_t gradientRamp = UINT32_MAX;
    std::uint32_t imageWidth = 0U;
    std::uint32_t imageHeight = 0U;
    std::uint8_t mappingMode = 0U;
    std::uint8_t viewboxUnits = 0U;
    std::uint8_t viewportUnits = 0U;
    std::uint8_t stretch = 0U;
    std::uint8_t tileMode = 0U;
    std::uint8_t alignmentX = 0U;
    std::uint8_t alignmentY = 0U;
};

struct RenderNodeSnapshot {
    RenderNodeId id = InvalidRenderNodeId;
    RenderNodeId parentId = InvalidRenderNodeId;
    Rect layoutSlot;
    Rect clip;
    // ClipToBounds clips to local (0,0,RenderSize). layoutClip is stored in
    // the same local space; a parent-space rect would be double-translated
    // by layoutSlot. Viewbox scale (and other render transforms) may paint
    // outside the layout slot when ClipToBounds is false.
    bool clipsToBounds = false;
    Size renderSize;
    ProjectiveTransform2D renderTransform;
    // Viewbox stretch stored on this node. Offscreen effect capture bakes it
    // into the layer so unscaled children fit RenderSize; composite then
    // omits it to avoid a second scale.
    bool hasViewboxTransform = false;
    Transform2D viewboxTransform{};
    ::Aero::BlendMode blendMode = ::Aero::BlendMode::Normal;
    double opacity = 1.0;
    RenderMaskSnapshot mask;
    RenderEffectSnapshot effect;
    std::uint32_t geometryClipVertexOffset = UINT32_MAX;
    std::uint32_t geometryClipIndexOffset = UINT32_MAX;
    std::uint32_t geometryClipVertexCount = 0U;
    std::uint32_t geometryClipIndexCount = 0U;
    std::uint32_t commandOffset = 0U;
    std::uint32_t commandCount = 0U;
    std::uint64_t elementRevision = 0U;
};

} // namespace Aero::Render

namespace Aero::Render { class RenderTree; }

namespace Aero::Render {

using namespace ::Aero::Render;

class RenderFrame {
public:
    RenderFrame() noexcept : nodes_(), commands_() {}

    Base::Span<const RenderNodeSnapshot> Nodes() const noexcept {
        return {nodes_.Data(), nodes_.Size()};
    }
    Base::Span<const RenderCommand> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    Base::Span<const RenderGradientRampSnapshot>
    GradientRamps() const noexcept {
        return {gradientRamps_.Data(), gradientRamps_.Size()};
    }
    Base::Span<const Point> GeometryClipVertices() const noexcept {
        return {geometryClipVertices_.Data(), geometryClipVertices_.Size()};
    }
    Base::Span<const std::uint32_t> GeometryClipIndices() const noexcept {
        return {geometryClipIndices_.Data(), geometryClipIndices_.Size()};
    }
    std::uint64_t Version() const noexcept { return version_; }
    Aero::Size LogicalSize() const noexcept { return logicalSize_; }
    std::uint32_t PixelWidth() const noexcept { return pixelWidth_; }
    std::uint32_t PixelHeight() const noexcept { return pixelHeight_; }
    double DpiScale() const noexcept { return dpiScale_; }
    std::uint64_t StableHash() const noexcept;

private:
    friend class ::Aero::Render::RenderTree;
    Base::Vector<RenderNodeSnapshot> nodes_;
    Base::Vector<RenderCommand> commands_;
    Base::Vector<RenderGradientRampSnapshot> gradientRamps_;
    Base::Vector<Point> geometryClipVertices_;
    Base::Vector<std::uint32_t> geometryClipIndices_;
    std::uint64_t version_ = 0U;
    Aero::Size logicalSize_{};
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
    double dpiScale_ = 1.0;
};

Base::Result<void> ValidateRenderFrame(const RenderFrame& frame) noexcept;

struct RenderDiagnostics {
    std::uint64_t commitVersion = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t glyphCommandCount = 0U;
    std::uint32_t dirtyCount = 0U;
    std::uint64_t frameHash = 0U;
};

} // namespace Aero::Render

namespace Aero::Render {

using namespace ::Aero::Render;

class RenderTree {
public:
    explicit RenderTree(::Aero::Threading::Dispatcher& dispatcher) noexcept;
    ~RenderTree() noexcept;

    RenderTree(const RenderTree&) = delete;
    RenderTree& operator=(const RenderTree&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> SetRoot(::Aero::Media::Visual* root) noexcept;
    Base::Result<void> Attach(
        ::Aero::Media::Visual& parent,
        ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> Detach(
        ::Aero::Media::Visual& parent,
        ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> Invalidate(
        ::Aero::Media::Visual& visual,
        RenderInvalidation invalidation =
            RenderInvalidation::Drawing) noexcept;
    Base::Result<void> SetOverlays(
        Base::Span<FrameworkElement* const> overlays,
        Base::Span<const Point> origins) noexcept;
    Base::Result<void> SetOverlays(
        Base::Span<FrameworkElement* const> overlays,
        Base::Span<const Base::Transform2D> transforms) noexcept;
    Base::Result<void> SetViewport(
        Aero::Size logicalSize,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight,
        double dpiScale) noexcept;
    Base::Result<std::uint32_t> Commit() noexcept;
    // P3.2 explicit RenderCommit phase entry (formerly the frame-hook
    // body). ViewFrame calls it directly; no hook registration remains.
    static void RenderCommitHook(void* context) noexcept;

    const ::Aero::Render::RenderFrame& CurrentFrame() const noexcept {
        return currentFrame_;
    }
    ::Aero::Render::RenderDiagnostics Diagnostics() const noexcept;
    Base::Status LastCommitStatus() const noexcept {
        return lastCommitStatus_;
    }

private:
    struct DrawingRecord {
        ::Aero::Media::Visual* visual = nullptr;
        DisplayList drawing;
        bool valid = false;
    };

    ::Aero::Threading::Dispatcher* dispatcher_ = nullptr;
    ::Aero::Media::Visual* root_ = nullptr;
    Base::Vector<Aero::VisualLease> dirty_;
    Base::Vector<DrawingRecord> drawings_;
    struct OverlayRecord {
        FrameworkElement* element = nullptr;
        Base::Transform2D transform;
    };
    Base::Vector<OverlayRecord> overlays_;
    ::Aero::Render::RenderFrame currentFrame_;
    bool initialized_ = false;
    RenderNodeId nextNodeId_ = 1U;
    std::uint64_t commitVersion_ = 0U;
    Base::Status lastCommitStatus_;
    Aero::Size logicalSize_{};
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
    double dpiScale_ = 1.0;
    bool viewportDirty_ = false;
    bool committing_ = false;

    Base::Result<void> VerifyElement(
        const ::Aero::Media::Visual& visual) const noexcept;
    Base::Result<void> QueueDirty(
        ::Aero::Media::Visual& visual) noexcept;
    void RemoveQueued(::Aero::Media::Visual& visual) noexcept;
    void MarkCommittedSubtree(
        ::Aero::Media::Visual& visual,
        bool ancestorVisible = true) noexcept;
    DrawingRecord* FindDrawing(::Aero::Media::Visual& visual) noexcept;
    void RemoveDrawing(::Aero::Media::Visual& visual) noexcept;
    Base::Result<void> BuildSubtree(
        ::Aero::Media::Visual& visual,
        RenderNodeId parentId,
        ::Aero::Render::RenderFrame& plan,
        bool overlayRoot,
        const ::Aero::Media::Transform3DContext& transform3D) noexcept;
    bool IsOverlay(
        const ::Aero::Media::Visual& visual) const noexcept;
    static ::Aero::Render::RenderEffectSnapshot BuildEffectSnapshot(
        const ::Aero::Media::Effect* effect) noexcept;
    static ::Aero::Render::RenderMaskSnapshot BuildMaskSnapshot(
        const ::Aero::UIElement& element,
        ::Aero::Render::RenderFrame& plan) noexcept;
    static std::uint32_t AppendGradientRamp(
        ::Aero::Render::RenderFrame& plan,
        const ::Aero::Media::GradientBrush& brush) noexcept;
};

} // namespace Aero::Render
