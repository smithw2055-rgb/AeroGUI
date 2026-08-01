#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Threading.hpp>
#include <Aero/DrawingContext.hpp>
#include <Aero/Input/Values.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Resources.hpp>

#include <cstdint>

namespace Aero {

using namespace Aero::Core;

enum class FontWeight : std::uint8_t { Normal = 0U, SemiBold, Bold };

class Style;

} // namespace Aero

namespace Aero::Media {

class AERO_API FontFamily final : public Base::Object {
    AERO_DECLARE_TYPE(FontFamily, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::StringView Source() const noexcept { return source_.View(); }
    Base::Result<void> SetSource(Base::StringView value) noexcept { return source_.TryAssign(value); }
private:
    Base::String source_;
};

} // namespace Aero::Media

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

namespace Aero {

using namespace Aero::Core;

namespace Render {
class RenderTree;
}

class FrameworkElement;

class FrameworkElementChildRange final {
public:
    class Iterator final {
    public:
        Iterator(const Visual* owner, std::uint32_t index) noexcept : owner_(owner), index_(index) { Advance(); }
        FrameworkElement* operator*() const noexcept;
        Iterator& operator++() noexcept { ++index_; Advance(); return *this; }
        bool operator!=(const Iterator& other) const noexcept { return owner_ != other.owner_ || index_ != other.index_; }
        bool operator==(const Iterator& other) const noexcept { return !(*this != other); }

    private:
        const Visual* owner_ = nullptr;
        std::uint32_t index_ = 0U;
        void Advance() noexcept;
    };

    explicit FrameworkElementChildRange(const Visual& owner) noexcept : owner_(&owner) {}
    Iterator begin() const noexcept { return Iterator(owner_, 0U); }
    Iterator end() const noexcept { return Iterator(owner_, VisualTreeHelper::GetChildrenCount(*owner_)); }
    bool Empty() const noexcept { return begin() == end(); }
    std::uint32_t Size() const noexcept;

private:
    const Visual* owner_ = nullptr;
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
    FrameworkElement* GetRenderParent() const noexcept {
        Visual* parent = GetVisualParent();
        return parent != nullptr ? parent->AsFrameworkElement() : nullptr;
    }
    DependencyObject* GetParent() const noexcept { return GetLogicalParent(); }
    FrameworkElementChildRange GetRenderChildren() const noexcept {
        return FrameworkElementChildRange(*this);
    }

    bool GetUseLayoutRounding() const noexcept;
    double GetDpiScale() const noexcept { return dpiScale_; }
    bool HasWidth() const noexcept;
    bool HasHeight() const noexcept;
    double GetWidth() const noexcept;
    double GetHeight() const noexcept;
    double GetActualWidth() const noexcept {
        return GetValueOr(
            ActualWidthProperty, 0.0);
    }
    double GetActualHeight() const noexcept {
        return GetValueOr(
            ActualHeightProperty, 0.0);
    }
    Size GetMinSize() const noexcept;
    Size GetMaxSize() const noexcept;
    Thickness GetMargin() const noexcept;
    Base::Ref<Media::Transform> GetLayoutTransform() const noexcept;
    Base::Transform2D GetLocalVisualTransform() const noexcept;
    Base::Result<Base::Ref<Base::Object>> GetDataContextResult() const noexcept;
    Base::StringView GetFontFamily() const noexcept {
        Base::StringView family = GetValueOr(
            FontFamilyProperty, Base::StringView{});
        const FrameworkElement* parent =
            GetRenderParent();
        while (family.Empty() &&
               parent != nullptr) {
            family = parent->GetValueOr(
                FontFamilyProperty,
                Base::StringView{});
            parent = parent->GetRenderParent();
        }
        return family;
    }
    ResourceDictionary& GetResources() noexcept {
        return resources_;
    }
    const ResourceDictionary& GetResources() const noexcept {
        return resources_;
    }
    Base::Result<void> SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;
    DependencyObject* GetTemplatedParent() const noexcept {
        return templatedParent_;
    }
    HorizontalAlignment GetHorizontalAlignment() const noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept;
    Base::Ref<Base::Object> GetDataContext() const noexcept {
        Base::Result<Base::Ref<Base::Object>> value = GetDataContextResult();
        return value ? value.Value() : Base::Ref<Base::Object>{};
    }

    inline static constexpr Members::Property<Base::Ref<Base::Object>> DataContextProperty{"DataContext"};
    // A common inherited owner lets Window, controls and text elements share
    // the same WPF-style FontFamily value through the visual tree.
    inline static constexpr Members::Property<Base::String> FontFamilyProperty{"FontFamily"};
    // Cursor names use the WPF built-in names (for example, "Hand"). The
    // platform input bridge consumes this inherited value when choosing the
    // native pointer cursor.
    inline static constexpr Members::Property<Base::String> CursorProperty{"Cursor"};
    // When true, this element's Cursor takes precedence over the cursor
    // chosen by the input hit target, matching FrameworkElement.ForceCursor.
    inline static constexpr Members::Property<bool> ForceCursorProperty{"ForceCursor"};
    inline static constexpr Members::Property<Base::Ref<Style>> StyleProperty{"Style"};
    // WPF-compatible application payload. It deliberately has no layout or
    // rendering effect and accepts the markup value without coercion.
    inline static constexpr Members::Property<Core::Value> TagProperty{"Tag"};
    inline static constexpr Members::Property<Core::Value> ToolTipProperty{"ToolTip"};
    inline static constexpr Members::Property<Input::InputScope> InputScopeProperty{"InputScope"};
    inline static constexpr Members::Property<Length> WidthProperty{"Width"};
    inline static constexpr Members::Property<Length> HeightProperty{"Height"};
    inline static constexpr Members::ReadOnlyProperty<double> ActualWidthProperty{"ActualWidth"};
    inline static constexpr Members::ReadOnlyProperty<double> ActualHeightProperty{"ActualHeight"};
    inline static constexpr Members::Property<double> MinWidthProperty{"MinWidth"};
    inline static constexpr Members::Property<double> MaxWidthProperty{"MaxWidth"};
    inline static constexpr Members::Property<double> MinHeightProperty{"MinHeight"};
    inline static constexpr Members::Property<double> MaxHeightProperty{"MaxHeight"};
    inline static constexpr Members::Property<Thickness> MarginProperty{"Margin"};
    inline static constexpr Members::Property<HorizontalAlignment> HorizontalAlignmentProperty{"HorizontalAlignment"};
    inline static constexpr Members::Property<VerticalAlignment> VerticalAlignmentProperty{"VerticalAlignment"};
    inline static constexpr Members::Property<bool> UseLayoutRoundingProperty{"UseLayoutRounding"};
    inline static constexpr Members::Property<Base::Ref<Media::Transform>> LayoutTransformProperty{"LayoutTransform"};

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
    Base::Result<void> SetFontFamily(
        Base::StringView value) noexcept {
        return SetValue(FontFamilyProperty, value);
    }
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
    Base::Result<void> SetLayoutTransform(
        Base::Ref<Media::Transform> value) noexcept;
    Base::Result<void> TryAddAuthoredTrigger(
        Base::Ref<Base::Object> trigger) noexcept;
    Base::Result<void> ClearAuthoredTriggers() noexcept;
    Base::Span<const Base::Ref<Base::Object>>
    AuthoredTriggers() const noexcept {
        return {
            authoredTriggers_.Data(),
            authoredTriggers_.Size()};
    }

    Base::RenderNodeId NodeId() const noexcept { return nodeId_; }
    bool IsRenderValid() const noexcept { return renderValid_; }
    std::uint64_t RenderRevision() const noexcept {
        return renderRevision_;
    }
    Base::Result<void> InvalidateRender() noexcept;

protected:
    virtual std::uint32_t GetLogicalChildrenCountCore() const noexcept { return VisualTreeHelper::GetChildrenCount(*this); }
    virtual DependencyObject* GetLogicalChildCore(std::uint32_t index) const noexcept { return LogicalTreeHelper::GetChild(static_cast<const Visual&>(*this), index); }
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual Base::Result<void> OnRender(
        DrawingContext& context) noexcept;

private:
    friend class LogicalTreeHelper;
    friend class Render::RenderTree;
    void* renderRuntime_ = nullptr;
    double dpiScale_ = 1.0;
    Base::RenderNodeId nodeId_ = Base::InvalidRenderNodeId;
    std::uint64_t renderRevision_ = 0U;
    bool renderAttached_ = false;
    bool renderValid_ = false;
    bool renderQueued_ = false;
    bool rendering_ = false;
    DependencyObject* templatedParent_ = nullptr;
    ResourceDictionary resources_;
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers_;
};

} // namespace Aero
