#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Presentation/Layout.hpp>

#include <cstdint>

namespace Aero::Presentation {

using namespace Aero::Core;

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

} // namespace Aero::Presentation

namespace Aero::Core {

template<>
struct MetaTypeTraits<Base::Color> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Color"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Color"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Core

namespace Aero::Presentation {

using namespace Aero::Core;

AERO_API bool IsFinite(Color value) noexcept;
AERO_API bool IsFinite(Transform2D value) noexcept;
AERO_API bool IsValidOpacity(double value) noexcept;

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
    DisplayList() noexcept : commands_() {}

    Base::Span<const RenderCommand> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    std::uint32_t CommandCount() const noexcept {
        return commands_.Size();
    }
    std::uint64_t StableHash() const noexcept;

private:
    friend class DisplayListBuilder;
    friend class RenderManager;
    Base::Vector<RenderCommand> commands_;
};

class AERO_API DisplayListBuilder final {
public:
    DisplayListBuilder() noexcept : list_() {}

    Base::Result<void> PushClip(Rect clip) noexcept;
    Base::Result<void> PopClip() noexcept;
    Base::Result<void> PushOpacity(double opacity) noexcept;
    Base::Result<void> PopOpacity() noexcept;
    Base::Result<void> PushTransform(Transform2D value) noexcept;
    Base::Result<void> PopTransform() noexcept;
    Base::Result<void> FillRect(Rect rect, Color color) noexcept;
    Base::Result<void> FillRoundedRect(
        Rect rect, Color color, double cornerRadius) noexcept;
    Base::Result<void> StrokeRect(
        Rect rect, Color color, double thickness) noexcept;
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

class RenderManager;
class FrameworkElement;

class FrameworkElementChildRange final {
public:
    class Iterator final {
    public:
        Iterator(Base::Span<Visual* const> children,
            std::uint32_t index) noexcept
            : children_(children), index_(index) { Advance(); }
        FrameworkElement* operator*() const noexcept {
            return index_ < children_.Size()
                ? children_[index_]->AsFrameworkElement() : nullptr;
        }
        Iterator& operator++() noexcept {
            if (index_ < children_.Size()) ++index_;
            Advance();
            return *this;
        }
        bool operator!=(const Iterator& other) const noexcept {
            return index_ != other.index_ ||
                children_.Data() != other.children_.Data();
        }
    private:
        Base::Span<Visual* const> children_;
        std::uint32_t index_ = 0U;
        void Advance() noexcept {
            while (index_ < children_.Size() &&
                children_[index_]->AsFrameworkElement() == nullptr) {
                ++index_;
            }
        }
    };

    explicit FrameworkElementChildRange(
        Base::Span<Visual* const> children) noexcept : children_(children) {}
    Iterator begin() const noexcept { return Iterator(children_, 0U); }
    Iterator end() const noexcept {
        return Iterator(children_, children_.Size());
    }
    bool Empty() const noexcept { return !(begin() != end()); }
    std::uint32_t Size() const noexcept {
        std::uint32_t count = 0U;
        for (FrameworkElement* child : *this) {
            (void)child;
            ++count;
        }
        return count;
    }
private:
    Base::Span<Visual* const> children_;
};

class AERO_API FrameworkElement : public UIElement {
    AERO_TYPED_META(FrameworkElement, UIElement)
public:
    explicit FrameworkElement(TypeId runtimeType) noexcept;
    ~FrameworkElement() override;

    FrameworkElement* AsFrameworkElement() noexcept override { return this; }
    const FrameworkElement* AsFrameworkElement() const noexcept override {
        return this;
    }
    FrameworkElement* RenderParent() const noexcept {
        Visual* parent = VisualParent();
        return parent != nullptr ? parent->AsFrameworkElement() : nullptr;
    }
    FrameworkElementChildRange RenderChildren() const noexcept {
        return FrameworkElementChildRange(VisualChildren());
    }

    bool UseLayoutRounding() const noexcept;
    double DpiScale() const noexcept { return dpiScale_; }
    bool HasWidth() const noexcept;
    bool HasHeight() const noexcept;
    double Width() const noexcept;
    double Height() const noexcept;
    Size MinSize() const noexcept;
    Size MaxSize() const noexcept;
    Thickness Margin() const noexcept;
    Base::Result<Base::Ref<Base::Object>> GetDataContext() const noexcept;
    DependencyObject* TemplatedParent() const noexcept {
        return templatedParent_;
    }
    HorizontalAlignment GetHorizontalAlignment() const noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept;

    inline static constexpr Aero::Core::DependencyPropertyHandle
        DataContextProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "DataContext");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        WidthProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Width");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        HeightProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Height");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        MinWidthProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "MinWidth");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        MaxWidthProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "MaxWidth");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        MinHeightProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "MinHeight");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        MaxHeightProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "MaxHeight");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        MarginProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Margin");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        HorizontalAlignmentProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "HorizontalAlignment");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        VerticalAlignmentProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "VerticalAlignment");
    inline static constexpr Aero::Core::DependencyPropertyHandle
        UseLayoutRoundingProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "UseLayoutRounding");

    Base::Result<void> SetLayoutRounding(
        bool enabled, double dpiScale = 1.0) noexcept;
    Base::Result<void> SetWidth(double value) noexcept;
    Base::Result<void> ClearWidth() noexcept;
    Base::Result<void> SetHeight(double value) noexcept;
    Base::Result<void> ClearHeight() noexcept;
    Base::Result<void> SetMinSize(Size value) noexcept;
    Base::Result<void> SetMaxSize(Size value) noexcept;
    Base::Result<void> SetMargin(Thickness value) noexcept;
    Base::Result<void> SetDataContext(
        Base::Ref<Base::Object> value) noexcept;
    Base::Result<void> ClearDataContext() noexcept;
    Base::Result<void> SetTemplatedParent(
        DependencyObject* value) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        templatedParent_ = value;
        return {};
    }
    Base::Result<void> SetHorizontalAlignment(
        HorizontalAlignment value) noexcept;
    Base::Result<void> SetVerticalAlignment(
        VerticalAlignment value) noexcept;

    RenderNodeId NodeId() const noexcept { return nodeId_; }
    bool IsRenderValid() const noexcept { return renderValid_; }
    std::uint64_t RenderRevision() const noexcept {
        return renderRevision_;
    }
    Base::Result<void> InvalidateRender() noexcept;

protected:
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept;

private:
    friend class RenderManager;
    RenderManager* renderManager_ = nullptr;
    double dpiScale_ = 1.0;
    RenderNodeId nodeId_ = InvalidRenderNodeId;
    std::uint64_t renderRevision_ = 0U;
    bool renderAttached_ = false;
    bool renderValid_ = false;
    bool renderQueued_ = false;
    bool buildingDisplayList_ = false;
    DependencyObject* templatedParent_ = nullptr;
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
    RenderPlan() noexcept : nodes_(), commands_() {}

    Base::Span<const RenderNodeSnapshot> Nodes() const noexcept {
        return {nodes_.Data(), nodes_.Size()};
    }
    Base::Span<const RenderCommand> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    std::uint64_t Version() const noexcept { return version_; }
    std::uint64_t StableHash() const noexcept;

private:
    friend class RenderManager;
    Base::Vector<RenderNodeSnapshot> nodes_;
    Base::Vector<RenderCommand> commands_;
    std::uint64_t version_ = 0U;
};

class AERO_API IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    virtual Base::Result<void> Submit(
        const RenderPlan& plan) noexcept = 0;
};

class AERO_API NullRenderBackend final : public IRenderBackend {
public:
    Base::Result<void> Submit(
        const RenderPlan& plan) noexcept override;

    std::uint64_t LastVersion() const noexcept {
        return lastVersion_;
    }
    std::uint64_t LastHash() const noexcept { return lastHash_; }
    std::uint32_t SubmissionCount() const noexcept {
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
        IRenderBackend& backend) noexcept;
    ~RenderManager() noexcept;

    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;

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
    Base::Result<std::uint32_t> Commit() noexcept;

    const RenderPlan& CurrentPlan() const noexcept {
        return currentPlan_;
    }
    RenderDiagnostics Diagnostics() const noexcept;

private:
    Dispatcher* dispatcher_ = nullptr;
    IRenderBackend* backend_ = nullptr;
    FrameworkElement* root_ = nullptr;
    Base::Vector<Detail::VisualLease> dirty_;
    RenderPlan currentPlan_;
    DispatcherFrameHookHandle phaseHook_;
    RenderNodeId nextNodeId_ = 1U;
    std::uint64_t commitVersion_ = 0U;
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
        RenderPlan& plan) noexcept;
    static void RenderCommitHook(void* context) noexcept;
};

} // namespace Aero::Presentation
