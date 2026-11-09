#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/InputScope.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Resources.hpp>
#include <Aero/ElementEnums.hpp>
#include <Aero/Layout.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Data/BindingExpression.hpp>

#include <cstdint>
#include <utility>


namespace Aero {

namespace Data {
class Binding;
}

using Meta::PropertyInvalidationFlags;
using Meta::TypeId;

class Style;
class LayoutEngine;
class AnimationEngine;
class BindingEngine;
class InteractivityEngine;
class StoryboardHost;
class ElementTree;
namespace Controls {
class Viewbox;
class TemplateBuilder;
class TemplateEngine;
class ItemsControl;
}
namespace Interactivity { class StyleInteraction; }
namespace Diagnostics { class Inspector; }
class FrameworkElementSeams;
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
    using DependencyObject::SetValue;

    FrameworkElement() noexcept : FrameworkElement(StaticTypeId()) {}
    explicit FrameworkElement(TypeId runtimeType) noexcept;
    ~FrameworkElement() override;

    Base::ProjectiveTransform2D GetLocalVisualTransform() const noexcept;
    bool HasLocalVisualTransform() const noexcept;
    bool TryGetViewboxTransform(Base::Transform2D& matrix) const noexcept;
    Base::Object* FindName(StringView name) noexcept;
    void RegisterName(StringView name, Base::Object& scopedElement) noexcept;
    template<class T> T* FindName(StringView name) noexcept {
        return static_cast<T*>(FindNameObject(name, T::StaticTypeId()));
    }
    Result<ResourceValue> FindResource(const ResourceKey& key) const noexcept;
    Result<ResourceValue> FindResource(StringView key) const noexcept;
    // WPF TryFindResource: missing key yields empty/null, not a Result failure.
    ResourceValue TryFindResource(const ResourceKey& key) const noexcept;
    ResourceValue TryFindResource(StringView key) const noexcept;
    void InvalidateVisual() noexcept;
    void Render(::Aero::Media::DrawingContext& context) noexcept { OnRender(context); }
    // WPF/Noesis-shaped code-side binding attach. Real work lives in
    // BindingOperations → BindingEngine::Attach (same path as {Binding}).
    Data::BindingExpression SetBinding(
        DependencyPropertyHandle property,
        const Data::Binding& binding) noexcept;
    template<class TOwner, class TValue>
    Data::BindingExpression SetBinding(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const Data::Binding& binding) noexcept {
        return SetBinding(property.Handle(), binding);
    }
    Data::BindingExpression SetBinding(
        DependencyPropertyHandle property,
        StringView path) noexcept;
    template<class TOwner, class TValue>
    Data::BindingExpression SetBinding(
        const DependencyPropertyRef<TOwner, TValue>& property,
        StringView path) noexcept {
        return SetBinding(property.Handle(), path);
    }
    void ClearBinding(DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    void ClearBinding(
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        ClearBinding(property.Handle());
    }

    DependencyObject* GetParent() const noexcept { return GetLogicalParent(); }
    bool GetUseLayoutRounding() const noexcept { return layoutScalars_.useLayoutRounding; }
    void SetUseLayoutRounding(bool enabled, double dpiScale = 1.0) noexcept;
    bool GetSnapsToDevicePixels() const noexcept { return layoutScalars_.snapsToDevicePixels; }
    void SetSnapsToDevicePixels(bool enabled) noexcept { SetValue(SnapsToDevicePixelsProperty, enabled); }
    bool GetHasWidth() const noexcept { return layoutScalars_.hasWidth; }
    bool GetHasHeight() const noexcept { return layoutScalars_.hasHeight; }
    double GetWidth() const noexcept { return layoutScalars_.hasWidth ? static_cast<double>(layoutScalars_.width) : 0.0; }
    void SetWidth(double value) noexcept { SetValue(WidthProperty, Length::Pixels(value)); }
    void ClearWidth() noexcept { ClearValue(WidthProperty); }
    double GetHeight() const noexcept { return layoutScalars_.hasHeight ? static_cast<double>(layoutScalars_.height) : 0.0; }
    void SetHeight(double value) noexcept { SetValue(HeightProperty, Length::Pixels(value)); }
    void ClearHeight() noexcept { ClearValue(HeightProperty); }
    double GetActualWidth() const noexcept { return static_cast<double>(GetRenderSize().width); }
    double GetActualHeight() const noexcept { return static_cast<double>(GetRenderSize().height); }
    Size GetMinSize() const noexcept {
        return layoutScalars_.hasMinMax ? GetMinSizeSlow() : Size{0.0f, 0.0f};
    }
    void SetMinSize(Size value) noexcept;
    Size GetMaxSize() const noexcept {
        return layoutScalars_.hasMinMax ? GetMaxSizeSlow() : Size{1.0e12f, 1.0e12f};
    }
    void SetMaxSize(Size value) noexcept;
    Thickness GetMargin() const noexcept { return layoutScalars_.margin; }
    void SetMargin(Thickness value) noexcept { SetValue(MarginProperty, value); }
    Ref<Media::Transform> GetLayoutTransform() const noexcept;
    void SetLayoutTransform(Ref<Media::Transform> value) noexcept;
    Ref<Media::FontFamily> GetFontFamily() const noexcept { return GetValue(FontFamilyProperty); }
    void SetFontFamily(Ref<Media::FontFamily> value) noexcept { SetValue(FontFamilyProperty, std::move(value)); }
    void SetFontFamily(StringView value) noexcept;
    FlowDirection GetFlowDirection() const noexcept { return layoutScalars_.flowDirection; }
    void SetFlowDirection(FlowDirection value) noexcept { SetValue(FlowDirectionProperty, value); }
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Ref<ResourceDictionary> value) noexcept;
    HorizontalAlignment GetHorizontalAlignment() const noexcept { return layoutScalars_.horizontalAlignment; }
    void SetHorizontalAlignment(HorizontalAlignment value) noexcept { SetValue(HorizontalAlignmentProperty, value); }
    VerticalAlignment GetVerticalAlignment() const noexcept { return layoutScalars_.verticalAlignment; }
    void SetVerticalAlignment(VerticalAlignment value) noexcept { SetValue(VerticalAlignmentProperty, value); }
    Value GetDataContext() const noexcept { return GetValue(DataContextProperty); }
    void SetDataContext(Value value) noexcept;
    void SetDataContext(Ref<Base::Object> value) noexcept {
        SetDataContext(Value::FromObject(Meta::TypeOf<Base::Object>(), std::move(value)));
    }
    void ClearDataContext() noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> LoadedEvent{"Loaded"};
    Event<RoutedEventArgs> Loaded() noexcept { return GetEvent(LoadedEvent); }
    inline static constexpr RoutedEvent<RoutedEventArgs> UnloadedEvent{"Unloaded"};
    Event<RoutedEventArgs> Unloaded() noexcept { return GetEvent(UnloadedEvent); }

    AERO_DEPENDENCY_PROPERTY(Value, DataContext);
    // A common inherited owner lets Window, controls and text elements share
    // the same WPF-style FontFamily value through the visual tree.
    AERO_DEPENDENCY_PROPERTY(Ref<Media::FontFamily>, FontFamily);
    AERO_DEPENDENCY_PROPERTY(FlowDirection, FlowDirection);
    // Cursor names use the WPF built-in names (for example, "Hand"). The
    // platform input bridge consumes this inherited value when choosing the
    // native pointer cursor.
    AERO_DEPENDENCY_PROPERTY(String, Cursor);
    // When true, this element's Cursor takes precedence over the cursor
    // chosen by the input hit target, matching FrameworkElement.ForceCursor.
    AERO_DEPENDENCY_PROPERTY(bool, ForceCursor);
    AERO_DEPENDENCY_PROPERTY(Ref<Style>, Style);
    // WPF-compatible application payload. It deliberately has no layout or
    // rendering effect and accepts the markup value without coercion.
    AERO_DEPENDENCY_PROPERTY(Value, Tag);
    AERO_DEPENDENCY_PROPERTY(Value, ToolTip);
    AERO_DEPENDENCY_PROPERTY(Input::InputScope, InputScope);
    AERO_DEPENDENCY_PROPERTY(Length, Width);
    AERO_DEPENDENCY_PROPERTY(Length, Height);
    AERO_READONLY_PROPERTY(double, ActualWidth);
    AERO_READONLY_PROPERTY(double, ActualHeight);
    AERO_DEPENDENCY_PROPERTY(double, MinWidth);
    AERO_DEPENDENCY_PROPERTY(double, MaxWidth);
    AERO_DEPENDENCY_PROPERTY(double, MinHeight);
    AERO_DEPENDENCY_PROPERTY(double, MaxHeight);
    AERO_DEPENDENCY_PROPERTY(Thickness, Margin);
    AERO_DEPENDENCY_PROPERTY(HorizontalAlignment, HorizontalAlignment);
    AERO_DEPENDENCY_PROPERTY(VerticalAlignment, VerticalAlignment);
    AERO_DEPENDENCY_PROPERTY(bool, UseLayoutRounding);
    AERO_DEPENDENCY_PROPERTY(bool, SnapsToDevicePixels);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Transform>, LayoutTransform);
    AERO_DEPENDENCY_PROPERTY(Ref<Media::Brush>, Foreground);

    double GetDpiScale() const noexcept { return static_cast<double>(layoutScalars_.dpiScale); }
    DependencyObject* GetTemplatedParent() const noexcept { return templatedParent_; }

protected:
    virtual std::uint32_t GetLogicalChildrenCount() const noexcept { return GetVisualChildrenCount(); }
    virtual DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept { return GetVisualChild(index); }
    void OnPropertyInvalidated(PropertyInvalidationFlags flags) noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    // NOTE: empty default kept intentionally. Viewbox spacer and other
    // non-visual FrameworkElements rely on zero desired size; UIElement's
    // default (return available) would inflate them. Panel/Control override.
    void MeasureCore(LayoutEngine& layout, Size constraint) noexcept override;
    void ArrangeCore(LayoutEngine& layout, Rect slot) noexcept override;

    Size MeasureOverride(Size availableSize) noexcept override {
        static_cast<void>(availableSize);
        return Size{};
    }
    // WPF lifecycle extension points. Style/DataContext changes previously
    // required reading StyleEngine/BindingEngine internals; override these.
    virtual void OnInitialized() noexcept {}
    virtual void OnStyleChanged(const Style* oldStyle, const Style* newStyle) noexcept {
        static_cast<void>(oldStyle);
        static_cast<void>(newStyle);
    }
    virtual void OnDataContextChanged(const Value& oldValue, const Value& newValue) noexcept {
        static_cast<void>(oldValue);
        static_cast<void>(newValue);
    }
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    friend class LogicalTreeHelper;
    friend class Controls::Viewbox;
    friend class FrameworkResourceResolver;
    friend class LayoutEngine;
    friend class UIElement;
    friend class AnimationEngine;
    friend class BindingEngine;
    friend class InteractivityEngine;
    friend class StoryboardHost;
    friend class Interactivity::StyleInteraction;
    friend class Controls::TemplateBuilder;
    friend class Controls::TemplateEngine;
    friend class Controls::ItemsControl;
    friend class Diagnostics::Inspector;
    friend class FrameworkElementSeams;

    struct LayoutScalars {
        float width = 0.0f;
        float height = 0.0f;
        Thickness margin{};
        float dpiScale = 1.0f;
        HorizontalAlignment horizontalAlignment = HorizontalAlignment::Stretch;
        VerticalAlignment verticalAlignment = VerticalAlignment::Stretch;
        FlowDirection flowDirection = FlowDirection::LeftToRight;
        std::uint8_t hasWidth : 1;
        std::uint8_t hasHeight : 1;
        std::uint8_t useLayoutRounding : 1;
        std::uint8_t snapsToDevicePixels : 1;
        std::uint8_t hasLayoutTransform : 1;
        std::uint8_t hasMinMax : 1;

        LayoutScalars() noexcept
            : hasWidth(0U)
            , hasHeight(0U)
            , useLayoutRounding(0U)
            , snapsToDevicePixels(0U)
            , hasLayoutTransform(0U)
            , hasMinMax(0U) {}
    };
    // Authored triggers/behaviors, style prototypes, and viewbox projection
    // live off the hot instance. Empty elements pay one pointer.
    struct FrameworkRare;

    Size GetMinSizeSlow() const noexcept;
    Size GetMaxSizeSlow() const noexcept;
    FrameworkElement* GetRenderParent() const noexcept;
    FrameworkElementChildRange GetRenderChildren() const noexcept { return FrameworkElementChildRange(*this); }
    void SyncLayoutScalarProperty(DependencyPropertyHandle property, const Meta::PropertyValue& newValue) noexcept;
    static void OnLayoutScalarChanged(DependencyObject& object, const DependencyPropertyChangedEventArgs& args) noexcept;
    void SetActualSize(double width, double height) noexcept;
    void SetTemplatedParent(DependencyObject* value) noexcept {
        Result<void> access = VerifyAccess();
        if (!access) return;
        templatedParent_ = value;
        return;
    }
    void AddAuthoredTrigger(Ref<Base::Object> trigger) noexcept;
    void ClearAuthoredTriggers() noexcept;
    Span<const Ref<Base::Object>> AuthoredTriggers() const noexcept;
    void AddAuthoredBehavior(Ref<Base::Object> behavior) noexcept;
    void ClearAuthoredBehaviors() noexcept;
    Span<const Ref<Base::Object>> AuthoredBehaviors() const noexcept;
    void AddStyleBehaviorPrototype(Ref<Base::Object> behavior) noexcept;
    void ClearStyleBehaviorPrototypes() noexcept;
    Span<const Ref<Base::Object>> StyleBehaviorPrototypes() const noexcept;
    void AddStyleTriggerPrototype(Ref<Base::Object> trigger) noexcept;
    void ClearStyleTriggerPrototypes() noexcept;
    Span<const Ref<Base::Object>> StyleTriggerPrototypes() const noexcept;
    Base::Object* FindNameObject(StringView name, Meta::TypeId expectedType) noexcept;
    Base::Object* FindRegisteredName(StringView name) const noexcept;
    FrameworkRare* EnsureFrameworkRare() noexcept;
    void DropRareIfUnused() noexcept;
    bool SetViewboxTransform(const Base::Transform2D& matrix) noexcept;
    void ClearViewboxTransform() noexcept;

    LayoutScalars layoutScalars_{};
    const ResourceDictionary* LocalResources() const noexcept { return resources_; }
    mutable ResourceDictionary* resources_ = nullptr;
    DependencyObject* templatedParent_ = nullptr;

    FrameworkRare* frameworkRare_ = nullptr;
};

static_assert(sizeof(FrameworkElement) == 232, "FrameworkElement must be 232 bytes");


} // namespace Aero

namespace Aero::Media {
inline constexpr auto FrameworkElementForegroundProperty = ::Aero::FrameworkElement::ForegroundProperty;
}
