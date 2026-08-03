#pragma once

#include "gui/GuiPrivate.hpp"

#include "DisplayList.hpp"

#include <Aero/FrameworkElement.hpp>

namespace Aero::Render {

enum class RenderEffectKind : std::uint8_t {
    None = 0U,
    Blur,
    DropShadow
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
};

struct RenderNodeSnapshot {
    RenderNodeId id = InvalidRenderNodeId;
    RenderNodeId parentId = InvalidRenderNodeId;
    Rect layoutSlot;
    Rect clip;
    // LayoutClip is meaningful to rendering only when the element explicitly
    // requests bounds clipping. Otherwise a GetRenderTransform (for example the
    // scale installed by Viewbox) is allowed to paint outside its layout slot.
    bool clipsToBounds = false;
    Size renderSize;
    Transform2D renderTransform;
    BlendMode blendMode = BlendMode::Normal;
    double opacity = 1.0;
    RenderEffectSnapshot effect;
    std::uint32_t commandOffset = 0U;
    std::uint32_t commandCount = 0U;
    std::uint64_t elementRevision = 0U;
};

} // namespace Aero::Render

namespace Aero::Render::Detail { class RenderTree; }

namespace Aero::Integration {

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
    std::uint64_t Version() const noexcept { return version_; }
    std::uint64_t StableHash() const noexcept;

private:
    friend class ::Aero::Render::Detail::RenderTree;
    Base::Vector<RenderNodeSnapshot> nodes_;
    Base::Vector<RenderCommand> commands_;
    std::uint64_t version_ = 0U;
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

} // namespace Aero::Integration

namespace Aero::Render::Detail {

using namespace ::Aero::Render;
using namespace ::Aero::Integration;

class RenderTree {
public:
    explicit RenderTree(::Aero::Threading::Dispatcher& dispatcher) noexcept;
    ~RenderTree() noexcept;

    RenderTree(const RenderTree&) = delete;
    RenderTree& operator=(const RenderTree&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> SetRoot(Visual* root) noexcept;
    Base::Result<void> Attach(
        Visual& parent,
        Visual& child) noexcept;
    Base::Result<void> Detach(
        Visual& parent,
        Visual& child) noexcept;
    Base::Result<void> Invalidate(
        Visual& visual,
        RenderInvalidation invalidation =
            RenderInvalidation::Drawing) noexcept;
    Base::Result<void> SetOverlays(
        Base::Span<FrameworkElement* const> overlays,
        Base::Span<const Point> origins) noexcept;
    Base::Result<std::uint32_t> Commit() noexcept;

    const Integration::RenderFrame& CurrentFrame() const noexcept {
        return currentFrame_;
    }
    Integration::RenderDiagnostics Diagnostics() const noexcept;
    Base::Status LastCommitStatus() const noexcept {
        return lastCommitStatus_;
    }

private:
    struct DrawingRecord {
        Visual* visual = nullptr;
        DisplayList drawing;
        bool valid = false;
    };

    ::Aero::Threading::Dispatcher* dispatcher_ = nullptr;
    Visual* root_ = nullptr;
    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> dirty_;
    Base::Vector<DrawingRecord> drawings_;
    struct OverlayRecord {
        FrameworkElement* element = nullptr;
        Point origin;
    };
    Base::Vector<OverlayRecord> overlays_;
    Integration::RenderFrame currentFrame_;
    ::Aero::Threading::DispatcherFrameHookHandle phaseHook_;
    RenderNodeId nextNodeId_ = 1U;
    std::uint64_t commitVersion_ = 0U;
    Base::Status lastCommitStatus_;
    bool committing_ = false;

    Base::Result<void> VerifyElement(
        const Visual& visual) const noexcept;
    Base::Result<void> QueueDirty(
        Visual& visual) noexcept;
    void RemoveQueued(Visual& visual) noexcept;
    void MarkCommittedSubtree(
        Visual& visual,
        bool ancestorVisible = true) noexcept;
    DrawingRecord* FindDrawing(Visual& visual) noexcept;
    void RemoveDrawing(Visual& visual) noexcept;
    Base::Result<void> BuildSubtree(
        Visual& visual,
        RenderNodeId parentId,
        Integration::RenderFrame& plan,
        bool overlayRoot = false) noexcept;
    bool IsOverlay(
        const Visual& visual) const noexcept;
    static void RenderCommitHook(void* context) noexcept;
};

} // namespace Aero::Render::Detail
