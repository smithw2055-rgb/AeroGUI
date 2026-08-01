#pragma once

#include "gui/ElementInternal.hpp"

#include "DisplayList.hpp"

#include <Aero/FrameworkElement.hpp>

namespace Aero::Render {

enum class RenderEffectKind : std::uint8_t {
    None = 0U,
    Blur,
    DropShadow
};

struct RenderEffectSnapshot final {
    RenderEffectKind kind = RenderEffectKind::None;
    double radius = 0.0;
    double direction = 315.0;
    double depth = 0.0;
    double opacity = 1.0;
    Color color{0.0F, 0.0F, 0.0F, 1.0F};
};

struct RenderNodeSnapshot final {
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
    RenderEffectSnapshot effect;
    std::uint32_t commandOffset = 0U;
    std::uint32_t commandCount = 0U;
    std::uint64_t elementRevision = 0U;
};

class RenderFrame final {
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
    friend class RenderTree;
    Base::Vector<RenderNodeSnapshot> nodes_;
    Base::Vector<RenderCommand> commands_;
    std::uint64_t version_ = 0U;
};

Base::Result<void> ValidateRenderFrame(const RenderFrame& frame) noexcept;

struct RenderDiagnostics final {
    std::uint64_t commitVersion = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t glyphCommandCount = 0U;
    std::uint32_t dirtyCount = 0U;
    std::uint64_t frameHash = 0U;
};

class RenderTree final {
public:
    explicit RenderTree(Dispatcher& dispatcher) noexcept;
    ~RenderTree() noexcept;

    RenderTree(const RenderTree&) = delete;
    RenderTree& operator=(const RenderTree&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> SetRoot(FrameworkElement* root) noexcept;
    Base::Result<void> Attach(
        FrameworkElement& parent,
        FrameworkElement& child) noexcept;
    Base::Result<void> Detach(
        FrameworkElement& parent,
        FrameworkElement& child) noexcept;
    Base::Result<void> Invalidate(
        FrameworkElement& element) noexcept;
    Base::Result<void> SetOverlays(
        Base::Span<FrameworkElement* const> overlays,
        Base::Span<const Point> origins) noexcept;
    Base::Result<std::uint32_t> Commit() noexcept;

    const RenderFrame& CurrentFrame() const noexcept {
        return currentFrame_;
    }
    RenderDiagnostics Diagnostics() const noexcept;
    Base::Status LastCommitStatus() const noexcept {
        return lastCommitStatus_;
    }

private:
    Dispatcher* dispatcher_ = nullptr;
    FrameworkElement* root_ = nullptr;
    Base::Vector<Aero::Detail::VisualLease> dirty_;
    struct OverlayRecord final {
        FrameworkElement* element = nullptr;
        Point origin;
    };
    Base::Vector<OverlayRecord> overlays_;
    RenderFrame currentFrame_;
    DispatcherFrameHookHandle phaseHook_;
    RenderNodeId nextNodeId_ = 1U;
    std::uint64_t commitVersion_ = 0U;
    Base::Status lastCommitStatus_;
    bool committing_ = false;

    Base::Result<void> VerifyElement(
        const FrameworkElement& element) const noexcept;
    Base::Result<void> QueueDirty(
        FrameworkElement& element) noexcept;
    void RemoveQueued(FrameworkElement& element) noexcept;
    void MarkCommittedSubtree(FrameworkElement& element) noexcept;
    Base::Result<void> BuildSubtree(
        FrameworkElement& element,
        RenderNodeId parentId,
        RenderFrame& plan,
        bool overlayRoot = false) noexcept;
    bool IsOverlay(
        const FrameworkElement& element) const noexcept;
    static void RenderCommitHook(void* context) noexcept;
};

} // namespace Aero::Render
