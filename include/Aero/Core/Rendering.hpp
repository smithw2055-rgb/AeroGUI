#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Layout.hpp>

#include <cstdint>

namespace Aero::Core {

using RenderNodeId = Base::RenderNodeId;
constexpr RenderNodeId InvalidRenderNodeId = Base::InvalidRenderNodeId;
using RenderImageId = std::uint64_t;
constexpr RenderImageId InvalidRenderImageId = 0U;
using RenderMeshId = std::uint64_t;
constexpr RenderMeshId InvalidRenderMeshId = 0U;
using RenderGlyphRunId = std::uint64_t;
constexpr RenderGlyphRunId InvalidRenderGlyphRunId = 0U;
using Color = Base::Color;
using Transform2D = Base::Transform2D;

AERO_NODISCARD AERO_API bool IsFinite(Color value) noexcept;
AERO_NODISCARD AERO_API bool IsFinite(Transform2D value) noexcept;
AERO_NODISCARD AERO_API bool IsValidOpacity(double value) noexcept;

// Commands contain only immutable value data. Backends must never retain pointers
// to active UI objects while consuming a RenderPlan.
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
    DrawGlyphRun
};

struct RenderCommand final {
    RenderCommandKind kind = RenderCommandKind::FillRect;
    Rect rect;
    Transform2D transform;
    Color color;
    Rect sourceUv;
    RenderImageId image = InvalidRenderImageId;
    RenderMeshId mesh = InvalidRenderMeshId;
    RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId;
    double scalar = 0.0;
};

class AERO_API DisplayList final {
public:
    explicit DisplayList(Base::IAllocator* allocator = nullptr) noexcept
        : commands_(allocator) {}

    AERO_NODISCARD Base::Span<const RenderCommand> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    AERO_NODISCARD std::uint32_t CommandCount() const noexcept {
        return commands_.Size();
    }
    AERO_NODISCARD std::uint64_t StableHash() const noexcept;

private:
    friend class DisplayListBuilder;
    friend class RenderManager;
    Base::Vector<RenderCommand> commands_;
};

class AERO_API DisplayListBuilder final {
public:
    explicit DisplayListBuilder(Base::IAllocator* allocator = nullptr) noexcept
        : list_(allocator) {}

    AERO_NODISCARD Base::Result<void> PushClip(Rect clip) noexcept;
    AERO_NODISCARD Base::Result<void> PopClip() noexcept;
    AERO_NODISCARD Base::Result<void> PushOpacity(double opacity) noexcept;
    AERO_NODISCARD Base::Result<void> PopOpacity() noexcept;
    AERO_NODISCARD Base::Result<void> PushTransform(Transform2D value) noexcept;
    AERO_NODISCARD Base::Result<void> PopTransform() noexcept;
    AERO_NODISCARD Base::Result<void> FillRect(Rect rect, Color color) noexcept;
    AERO_NODISCARD Base::Result<void> FillRoundedRect(
        Rect rect, Color color, double cornerRadius) noexcept;
    AERO_NODISCARD Base::Result<void> StrokeRect(
        Rect rect, Color color, double thickness) noexcept;
    AERO_NODISCARD Base::Result<void> DrawImage(
        RenderImageId image,
        Rect destination,
        Rect sourceUv,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    AERO_NODISCARD Base::Result<void> DrawMesh(
        RenderMeshId mesh,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    AERO_NODISCARD Base::Result<void> DrawGlyphRun(
        RenderGlyphRunId glyphRun,
        Color tint = {1.0F, 1.0F, 1.0F, 1.0F}) noexcept;
    AERO_NODISCARD Base::Result<DisplayList> Finish() noexcept;

private:
    DisplayList list_;
    std::uint32_t clipDepth_ = 0U;
    std::uint32_t opacityDepth_ = 0U;
    std::uint32_t transformDepth_ = 0U;
    bool finished_ = false;

    AERO_NODISCARD Base::Result<void> Append(
        const RenderCommand& command) noexcept;
};

class RenderManager;

class AERO_API RenderElement : public LayoutElement {
public:
    RenderElement(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry,
        TypeId runtimeType,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RenderElement() override;

    AERO_NODISCARD RenderNodeId NodeId() const noexcept { return nodeId_; }
    AERO_NODISCARD bool IsRenderValid() const noexcept { return renderValid_; }
    AERO_NODISCARD std::uint64_t RenderRevision() const noexcept {
        return renderRevision_;
    }
    AERO_NODISCARD Base::Result<void> InvalidateRender() noexcept;

protected:
    AERO_NODISCARD virtual Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept;

private:
    friend class RenderManager;
    RenderManager* renderManager_ = nullptr;
    RenderElement* renderParent_ = nullptr;
    Base::Vector<RenderElement*> renderChildren_;
    RenderNodeId nodeId_ = InvalidRenderNodeId;
    std::uint64_t renderRevision_ = 0U;
    bool renderValid_ = false;
    bool renderQueued_ = false;
    bool buildingDisplayList_ = false;
};

struct RenderNodeSnapshot final {
    RenderNodeId id = InvalidRenderNodeId;
    RenderNodeId parentId = InvalidRenderNodeId;
    Rect layoutSlot;
    Rect clip;
    Size renderSize;
    std::uint32_t commandOffset = 0U;
    std::uint32_t commandCount = 0U;
    std::uint64_t elementRevision = 0U;
};

class AERO_API RenderPlan final {
public:
    explicit RenderPlan(Base::IAllocator* allocator = nullptr) noexcept
        : nodes_(allocator), commands_(allocator) {}

    AERO_NODISCARD Base::Span<const RenderNodeSnapshot> Nodes() const noexcept {
        return {nodes_.Data(), nodes_.Size()};
    }
    AERO_NODISCARD Base::Span<const RenderCommand> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    AERO_NODISCARD std::uint64_t Version() const noexcept { return version_; }
    AERO_NODISCARD std::uint64_t StableHash() const noexcept;

private:
    friend class RenderManager;
    Base::Vector<RenderNodeSnapshot> nodes_;
    Base::Vector<RenderCommand> commands_;
    std::uint64_t version_ = 0U;
};

class AERO_API IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    AERO_NODISCARD virtual Base::Result<void> Submit(
        const RenderPlan& plan) noexcept = 0;
};

class AERO_API NullRenderBackend final : public IRenderBackend {
public:
    AERO_NODISCARD Base::Result<void> Submit(
        const RenderPlan& plan) noexcept override;

    AERO_NODISCARD std::uint64_t LastVersion() const noexcept {
        return lastVersion_;
    }
    AERO_NODISCARD std::uint64_t LastHash() const noexcept { return lastHash_; }
    AERO_NODISCARD std::uint32_t SubmissionCount() const noexcept {
        return submissionCount_;
    }

private:
    std::uint64_t lastVersion_ = 0U;
    std::uint64_t lastHash_ = 0U;
    std::uint32_t submissionCount_ = 0U;
};

struct RenderDiagnostics final {
    std::uint64_t commitVersion = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t dirtyCount = 0U;
    std::uint64_t planHash = 0U;
};

class AERO_API RenderManager final {
public:
    RenderManager(
        Dispatcher& dispatcher,
        IRenderBackend& backend,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RenderManager() noexcept;

    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    AERO_NODISCARD Base::Result<void> SetRoot(RenderElement* root) noexcept;
    AERO_NODISCARD Base::Result<void> Attach(
        RenderElement& parent,
        RenderElement& child) noexcept;
    AERO_NODISCARD Base::Result<void> Detach(
        RenderElement& parent,
        RenderElement& child) noexcept;
    AERO_NODISCARD Base::Result<void> Invalidate(
        RenderElement& element) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> Commit() noexcept;

    AERO_NODISCARD const RenderPlan& CurrentPlan() const noexcept {
        return currentPlan_;
    }
    AERO_NODISCARD RenderDiagnostics Diagnostics() const noexcept;

private:
    Dispatcher* dispatcher_ = nullptr;
    IRenderBackend* backend_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    RenderElement* root_ = nullptr;
    Base::Vector<RenderElement*> dirty_;
    RenderPlan currentPlan_;
    DispatcherFrameHookHandle phaseHook_;
    RenderNodeId nextNodeId_ = 1U;
    std::uint64_t commitVersion_ = 0U;
    bool committing_ = false;

    AERO_NODISCARD Base::Result<void> VerifyElement(
        const RenderElement& element) const noexcept;
    AERO_NODISCARD Base::Result<void> QueueDirty(
        RenderElement& element) noexcept;
    AERO_NODISCARD Base::Result<void> BuildSubtree(
        RenderElement& element,
        RenderNodeId parentId,
        RenderPlan& plan) noexcept;
    void RemoveChild(
        Base::Vector<RenderElement*>& children,
        RenderElement& child) noexcept;
    static void RenderCommitHook(void* context) noexcept;
};

} // namespace Aero::Core
