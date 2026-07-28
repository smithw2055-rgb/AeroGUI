#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Resources.hpp>

#include <cstdint>

namespace Aero::Presentation {

class Style;

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
// to active UI objects while consuming an immutable frame snapshot.
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
#if !defined(AERO_SDK_SURFACE_ONLY)
    friend class RenderManager;
#endif
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

#if !defined(AERO_SDK_SURFACE_ONLY)
class RenderManager;
#endif
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
    AERO_DECLARE_TYPE(FrameworkElement, UIElement)
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
    ResourceDictionary& Resources() noexcept {
        return resources_;
    }
    const ResourceDictionary& Resources() const noexcept {
        return resources_;
    }
    Base::Result<void> SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;
    DependencyObject* TemplatedParent() const noexcept {
        return templatedParent_;
    }
    HorizontalAlignment GetHorizontalAlignment() const noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept;

    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        DataContextProperty{"DataContext"};
    inline static constexpr Members::Property<
        Base::Ref<Style>>
        StyleProperty{"Style"};
    inline static constexpr Members::Property<Length>
        WidthProperty{"Width"};
    inline static constexpr Members::Property<Length>
        HeightProperty{"Height"};
    inline static constexpr Members::Property<double>
        MinWidthProperty{"MinWidth"};
    inline static constexpr Members::Property<double>
        MaxWidthProperty{"MaxWidth"};
    inline static constexpr Members::Property<double>
        MinHeightProperty{"MinHeight"};
    inline static constexpr Members::Property<double>
        MaxHeightProperty{"MaxHeight"};
    inline static constexpr Members::Property<Thickness>
        MarginProperty{"Margin"};
    inline static constexpr Members::Property<
        HorizontalAlignment>
        HorizontalAlignmentProperty{"HorizontalAlignment"};
    inline static constexpr Members::Property<
        VerticalAlignment>
        VerticalAlignmentProperty{"VerticalAlignment"};
    inline static constexpr Members::Property<bool>
        UseLayoutRoundingProperty{"UseLayoutRounding"};

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
#if !defined(AERO_SDK_SURFACE_ONLY)
    friend class RenderManager;
    RenderManager* renderManager_ = nullptr;
#else
    void* renderOwner_ = nullptr;
#endif
    double dpiScale_ = 1.0;
    RenderNodeId nodeId_ = InvalidRenderNodeId;
    std::uint64_t renderRevision_ = 0U;
    bool renderAttached_ = false;
    bool renderValid_ = false;
    bool renderQueued_ = false;
    bool buildingDisplayList_ = false;
    DependencyObject* templatedParent_ = nullptr;
    ResourceDictionary resources_;
};

} // namespace Aero::Presentation
