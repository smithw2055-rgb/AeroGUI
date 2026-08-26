#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/InputScope.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Resources.hpp>
#include <Aero/HorizontalAlignment.hpp>
#include <Aero/Layout.hpp>
#include <Aero/UIElement.hpp>

#include <cstdint>


namespace Aero {

using Meta::PropertyInvalidationFlags;
using Meta::TypeId;

class Style;
namespace Controls { class Viewbox; }
class FrameworkElement;
namespace Media {
class DrawingContext;
class Brush;
class Transform;
class FontFamily;
}

class FrameworkElementChildRange {
public:
    class Iterator {
    public:
        Iterator(const ::Aero::Media::Visual* owner, std::uint32_t index) noexcept : owner_(owner), index_(index) { Advance(); }
        FrameworkElement* operator*() const noexcept;
        Iterator& operator++() noexcept { ++index_; Advance(); return *this; }
        bool operator!=(const Iterator& other) const noexcept { return owner_ != other.owner_ || index_ != other.index_; }
        bool operator==(const Iterator& other) const noexcept { return !(*this != other); }

    private:
        const ::Aero::Media::Visual* owner_ = nullptr;
        std::uint32_t index_ = 0U;
        void Advance() noexcept;
    };

    explicit FrameworkElementChildRange(const ::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}
    Iterator begin() const noexcept { return Iterator(owner_, 0U); }
    Iterator end() const noexcept { return Iterator(owner_, ::Aero::Media::VisualTreeHelper::GetChildrenCount(*owner_)); }
    bool Empty() const noexcept { return begin() == end(); }
    std::uint32_t Size() const noexcept;

private:
    const ::Aero::Media::Visual* owner_ = nullptr;
};

class AERO_GUI_API FrameworkElement : public UIElement {
    AERO_DECLARE_TYPE(FrameworkElement, UIElement)
public:
    explicit FrameworkElement(TypeId runtimeType) noexcept;
    ~FrameworkElement() override;

    DependencyObject* GetParent() const noexcept { return GetLogicalParent(); }

    bool GetUseLayoutRounding() const noexcept;
    bool GetSnapsToDevicePixels() const noexcept;
    double GetDpiScale() const noexcept { return dpiScale_; }
    bool GetHasWidth() const noexcept;
    bool GetHasHeight() const noexcept;
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
    Ref<Media::Transform> GetLayoutTransform() const noexcept;
    Base::ProjectiveTransform2D GetLocalVisualTransform() const noexcept;
    Result<Value> GetDataContextResult() const noexcept;
    Ref<Media::FontFamily> GetFontFamily() const noexcept {
        return GetValueOr(
            FontFamilyProperty, Ref<Media::FontFamily>{});
    }
    FlowDirection GetFlowDirection() const noexcept {
        return GetValueOr(FlowDirectionProperty, FlowDirection::LeftToRight);
    }
    Base::Object* FindName(StringView name) noexcept;
    template<class T>
    T* FindName(StringView name) noexcept {
        return static_cast<T*>(FindNameObject(name, T::StaticTypeId()));
    }
    Result<ResourceValue> FindResource(const ResourceKey& key) const noexcept;
    Result<ResourceValue> FindResource(StringView key) const noexcept;
    Result<ResourceValue> TryFindResource(const ResourceKey& key) const noexcept;
    Result<ResourceValue> TryFindResource(StringView key) const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(
        Ref<ResourceDictionary> value) noexcept;
    DependencyObject* GetTemplatedParent() const noexcept {
        return templatedParent_;
    }
    HorizontalAlignment GetHorizontalAlignment() const noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept;
    Value GetDataContext() const noexcept {
        Result<Value> value = GetDataContextResult();
        return value ? value.Value() :
            Value::NullObject(Meta::TypeOf<Base::Object>());
    }

    inline static constexpr DependencyProperty<Value> DataContextProperty{"DataContext"};
    // A common inherited owner lets Window, controls and text elements share
    // the same WPF-style FontFamily value through the visual tree.
    inline static constexpr DependencyProperty<Ref<Media::FontFamily>> FontFamilyProperty{"FontFamily"};
    inline static constexpr DependencyProperty<FlowDirection> FlowDirectionProperty{"FlowDirection"};
    // Cursor names use the WPF built-in names (for example, "Hand"). The
    // platform input bridge consumes this inherited value when choosing the
    // native pointer cursor.
    inline static constexpr DependencyProperty<String> CursorProperty{"Cursor"};
    // When true, this element's Cursor takes precedence over the cursor
    // chosen by the input hit target, matching FrameworkElement.ForceCursor.
    inline static constexpr DependencyProperty<bool> ForceCursorProperty{"ForceCursor"};
    inline static constexpr DependencyProperty<Ref<Style>> StyleProperty{"Style"};
    // WPF-compatible application payload. It deliberately has no layout or
    // rendering effect and accepts the markup value without coercion.
    inline static constexpr DependencyProperty<Value> TagProperty{"Tag"};
    inline static constexpr DependencyProperty<Value> ToolTipProperty{"ToolTip"};
    inline static constexpr DependencyProperty<Input::InputScope> InputScopeProperty{"InputScope"};
    inline static constexpr DependencyProperty<Length> WidthProperty{"Width"};
    inline static constexpr DependencyProperty<Length> HeightProperty{"Height"};
    inline static constexpr ReadOnlyDependencyProperty<double> ActualWidthProperty{"ActualWidth"};
    inline static constexpr ReadOnlyDependencyProperty<double> ActualHeightProperty{"ActualHeight"};
    inline static constexpr DependencyProperty<double> MinWidthProperty{"MinWidth"};
    inline static constexpr DependencyProperty<double> MaxWidthProperty{"MaxWidth"};
    inline static constexpr DependencyProperty<double> MinHeightProperty{"MinHeight"};
    inline static constexpr DependencyProperty<double> MaxHeightProperty{"MaxHeight"};
    inline static constexpr DependencyProperty<Thickness> MarginProperty{"Margin"};
    inline static constexpr DependencyProperty<HorizontalAlignment> HorizontalAlignmentProperty{"HorizontalAlignment"};
    inline static constexpr DependencyProperty<VerticalAlignment> VerticalAlignmentProperty{"VerticalAlignment"};
    inline static constexpr DependencyProperty<bool> UseLayoutRoundingProperty{"UseLayoutRounding"};
    inline static constexpr DependencyProperty<bool> SnapsToDevicePixelsProperty{"SnapsToDevicePixels"};
    inline static constexpr DependencyProperty<Ref<Media::Transform>> LayoutTransformProperty{"LayoutTransform"};
    inline static constexpr DependencyProperty<Ref<Media::Brush>> ForegroundProperty{"Foreground"};

    void SetUseLayoutRounding(
        bool enabled, double dpiScale = 1.0) noexcept;
    void SetSnapsToDevicePixels(bool enabled) noexcept { SetValue(SnapsToDevicePixelsProperty, enabled); }
    void SetWidth(double value) noexcept;
    void ClearWidth() noexcept;
    void SetHeight(double value) noexcept;
    void ClearHeight() noexcept;
    void SetMinSize(Size value) noexcept;
    void SetMaxSize(Size value) noexcept;
    void SetMargin(Thickness value) noexcept;
    void SetDataContext(Value value) noexcept;
    void SetDataContext(Ref<Base::Object> value) noexcept {
        SetDataContext(Value::FromObject(
            Meta::TypeOf<Base::Object>(), std::move(value)));
    }
    void SetFontFamily(
        Ref<Media::FontFamily> value) noexcept {
        SetValue(FontFamilyProperty, std::move(value));
    }
    Result<void> SetFontFamily(StringView value) noexcept;
    void SetFlowDirection(FlowDirection value) noexcept {
        SetValue(FlowDirectionProperty, value);
    }
    void ClearDataContext() noexcept;
    void SetHorizontalAlignment(
        HorizontalAlignment value) noexcept;
    void SetVerticalAlignment(
        VerticalAlignment value) noexcept;
    void SetLayoutTransform(
        Ref<Media::Transform> value) noexcept;
    Result<void> InvalidateVisual() noexcept;

protected:
    virtual std::uint32_t GetLogicalChildrenCount() const noexcept { return GetVisualChildrenCount(); }
    virtual DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept { return GetVisualChild(index); }
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept;

private:
    FrameworkElement* GetRenderParent() const noexcept;
    FrameworkElementChildRange GetRenderChildren() const noexcept {
        return FrameworkElementChildRange(*this);
    }
    void SetTemplatedParent(
        DependencyObject* value) noexcept {
        Result<void> access = VerifyAccess();
        if (!access) return;
        templatedParent_ = value;
        return;
    }
    Result<void> AddAuthoredTrigger(
        Ref<Base::Object> trigger) noexcept;
    void ClearAuthoredTriggers() noexcept;
    Span<const Ref<Base::Object>>
    AuthoredTriggers() const noexcept;
    Result<void> AddAuthoredBehavior(
        Ref<Base::Object> behavior) noexcept;
    void ClearAuthoredBehaviors() noexcept;
    Span<const Ref<Base::Object>>
    AuthoredBehaviors() const noexcept;
    Result<void> AddStyleBehaviorPrototype(
        Ref<Base::Object> behavior) noexcept;
    void ClearStyleBehaviorPrototypes() noexcept;
    Span<const Ref<Base::Object>>
    StyleBehaviorPrototypes() const noexcept;
    Result<void> AddStyleTriggerPrototype(
        Ref<Base::Object> trigger) noexcept;
    void ClearStyleTriggerPrototypes() noexcept;
    Span<const Ref<Base::Object>>
    StyleTriggerPrototypes() const noexcept;

    const ResourceDictionary* LocalResources() const noexcept {
        return resources_;
    }

    Base::Object* FindNameObject(
        StringView name,
        Meta::TypeId expectedType) noexcept;

    friend class LogicalTreeHelper;
    friend class Controls::Viewbox;
    friend class ResourceResolver;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
    double dpiScale_ = 1.0;
    DependencyObject* templatedParent_ = nullptr;
    mutable ResourceDictionary* resources_ = nullptr;
    // Authored triggers/behaviors, style prototypes, and viewbox projection
    // live off the hot instance. Empty elements pay one pointer.
    struct FrameworkRare;
    FrameworkRare* EnsureFrameworkRare() noexcept;
    void DropRareIfUnused() noexcept;
    bool SetViewboxTransform(const Base::Transform2D& matrix) noexcept;
    void ClearViewboxTransform() noexcept;
    FrameworkRare* frameworkRare_ = nullptr;
};

} // namespace Aero

namespace Aero::Media {
inline constexpr auto FrameworkElementForegroundProperty =
    ::Aero::FrameworkElement::ForegroundProperty;
}
