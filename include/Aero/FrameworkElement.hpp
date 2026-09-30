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
namespace Detail { class FrameworkElementSeams; }
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
    double GetActualWidth() const noexcept { return GetValue(ActualWidthProperty); }
    double GetActualHeight() const noexcept { return GetValue(ActualHeightProperty); }
    Size GetMinSize() const noexcept;
    Size GetMaxSize() const noexcept;
    Thickness GetMargin() const noexcept;
    Ref<Media::Transform> GetLayoutTransform() const noexcept;
    Base::ProjectiveTransform2D GetLocalVisualTransform() const noexcept;
    bool TryGetViewboxTransform(Base::Transform2D& matrix) const noexcept;
    Ref<Media::FontFamily> GetFontFamily() const noexcept { return GetValue(FontFamilyProperty); }
    FlowDirection GetFlowDirection() const noexcept { return GetValue(FlowDirectionProperty); }
    Base::Object* FindName(StringView name) noexcept;
    Result<void> RegisterName(StringView name, Base::Object& scopedElement) noexcept;
    template<class T> T* FindName(StringView name) noexcept {
        return static_cast<T*>(FindNameObject(name, T::StaticTypeId()));
    }
    Result<ResourceValue> FindResource(const ResourceKey& key) const noexcept;
    Result<ResourceValue> FindResource(StringView key) const noexcept;
    // WPF TryFindResource: missing key yields empty/null, not a Result failure.
    ResourceValue TryFindResource(const ResourceKey& key) const noexcept;
    ResourceValue TryFindResource(StringView key) const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Ref<ResourceDictionary> value) noexcept;
    DependencyObject* GetTemplatedParent() const noexcept { return templatedParent_; }
    HorizontalAlignment GetHorizontalAlignment() const noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept;
    Value GetDataContext() const noexcept {
        Result<Value> value = GetDataContextResult();
        return value ? value.Value() : Value::NullObject(Meta::TypeOf<Base::Object>());
    }

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

    inline static constexpr RoutedEvent<RoutedEventArgs> LoadedEvent{"Loaded"};
    Event<RoutedEventArgs> Loaded() noexcept { return GetEvent(LoadedEvent); }

    void SetUseLayoutRounding(bool enabled, double dpiScale = 1.0) noexcept;
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
        SetDataContext(Value::FromObject(Meta::TypeOf<Base::Object>(), std::move(value)));
    }
    void SetFontFamily(Ref<Media::FontFamily> value) noexcept { SetValue(FontFamilyProperty, std::move(value)); }
    void SetFontFamily(StringView value) noexcept;
    void SetFlowDirection(FlowDirection value) noexcept { SetValue(FlowDirectionProperty, value); }
    void ClearDataContext() noexcept;
    void SetHorizontalAlignment(HorizontalAlignment value) noexcept;
    void SetVerticalAlignment(VerticalAlignment value) noexcept;
    void SetLayoutTransform(Ref<Media::Transform> value) noexcept;
    void InvalidateVisual() noexcept;

    // WPF/Noesis-shaped code-side binding attach. Real work lives in
    // BindingOperations → BindingEngine::Attach (same path as {Binding}).
    Result<Data::BindingExpression> SetBinding(
        DependencyPropertyHandle property,
        const Data::Binding& binding) noexcept;
    template<class TOwner, class TValue>
    Result<Data::BindingExpression> SetBinding(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const Data::Binding& binding) noexcept {
        return SetBinding(property.Handle(), binding);
    }
    Result<Data::BindingExpression> SetBinding(
        DependencyPropertyHandle property,
        StringView path) noexcept;
    template<class TOwner, class TValue>
    Result<Data::BindingExpression> SetBinding(
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

protected:
    virtual std::uint32_t GetLogicalChildrenCount() const noexcept { return GetVisualChildrenCount(); }
    virtual DependencyObject* GetLogicalChild(std::uint32_t index) const noexcept { return GetVisualChild(index); }
    void OnPropertyInvalidated(PropertyInvalidationFlags flags) noexcept override;
    void SyncLayoutScalars() noexcept;
    // NOTE: empty default kept intentionally. Viewbox spacer and other
    // non-visual FrameworkElements rely on zero desired size; UIElement's
    // default (return available) would inflate them. Panel/Control override.
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
    struct LayoutScalars {
        Length width{};
        Length height{};
        double minWidth = 0.0;
        double maxWidth = 1.0e12;
        double minHeight = 0.0;
        double maxHeight = 1.0e12;
        Thickness margin{};
    };
    LayoutScalars layoutScalars_{};

    FrameworkElement* GetRenderParent() const noexcept;
    FrameworkElementChildRange GetRenderChildren() const noexcept { return FrameworkElementChildRange(*this); }

public:
    void Render(::Aero::Media::DrawingContext& context) noexcept { OnRender(context); }

private:
    friend class LogicalTreeHelper;
    friend class Controls::Viewbox;
    friend class ResourceResolver;
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
    friend class Detail::FrameworkElementSeams;

    void SetAnimatedWidth(Length value) noexcept { layoutScalars_.width = value; }
    void SetAnimatedHeight(Length value) noexcept { layoutScalars_.height = value; }
    void SetAnimatedMinWidth(double value) noexcept { layoutScalars_.minWidth = value; }
    void SetAnimatedMinHeight(double value) noexcept { layoutScalars_.minHeight = value; }
    void SetAnimatedMaxWidth(double value) noexcept { layoutScalars_.maxWidth = value; }
    void SetAnimatedMaxHeight(double value) noexcept { layoutScalars_.maxHeight = value; }
    void SetAnimatedMargin(Thickness value) noexcept { layoutScalars_.margin = value; }
    Result<Value> GetDataContextResult() const noexcept;
    void SetActualSize(double width, double height) noexcept {
        Meta::PropertyValue widthVal(width);
        Meta::PropertyValue heightVal(height);
        SetReadOnlyCurrentValue(ActualWidthProperty.Handle(), widthVal);
        SetReadOnlyCurrentValue(ActualHeightProperty.Handle(), heightVal);
    }
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

    const ResourceDictionary* LocalResources() const noexcept { return resources_; }

    Base::Object* FindNameObject(StringView name, Meta::TypeId expectedType) noexcept;
    Base::Object* FindRegisteredName(StringView name) const noexcept;
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


namespace Detail {

// Src/metadata free-function bridge. Engine classes are friends and call
// FrameworkElement private seams directly; do not grow this helper.
class FrameworkElementSeams {
public:
    static void SetAnimatedWidth(FrameworkElement& e, Length value) noexcept { e.SetAnimatedWidth(value); }
    static void SetAnimatedHeight(FrameworkElement& e, Length value) noexcept { e.SetAnimatedHeight(value); }
    static void SetAnimatedMinWidth(FrameworkElement& e, double value) noexcept { e.SetAnimatedMinWidth(value); }
    static void SetAnimatedMinHeight(FrameworkElement& e, double value) noexcept { e.SetAnimatedMinHeight(value); }
    static void SetAnimatedMaxWidth(FrameworkElement& e, double value) noexcept { e.SetAnimatedMaxWidth(value); }
    static void SetAnimatedMaxHeight(FrameworkElement& e, double value) noexcept { e.SetAnimatedMaxHeight(value); }
    static void SetAnimatedMargin(FrameworkElement& e, Thickness value) noexcept { e.SetAnimatedMargin(value); }
    static Result<Value> GetDataContextResult(const FrameworkElement& e) noexcept { return e.GetDataContextResult(); }
    static void SetActualSize(FrameworkElement& e, double width, double height) noexcept { e.SetActualSize(width, height); }
    static void SetTemplatedParent(FrameworkElement& e, DependencyObject* value) noexcept { e.SetTemplatedParent(value); }
    static void AddAuthoredTrigger(FrameworkElement& e, Ref<Base::Object> trigger) noexcept { e.AddAuthoredTrigger(std::move(trigger)); }
    static void ClearAuthoredTriggers(FrameworkElement& e) noexcept { e.ClearAuthoredTriggers(); }
    static Span<const Ref<Base::Object>> AuthoredTriggers(const FrameworkElement& e) noexcept { return e.AuthoredTriggers(); }
    static void AddAuthoredBehavior(FrameworkElement& e, Ref<Base::Object> behavior) noexcept { e.AddAuthoredBehavior(std::move(behavior)); }
    static void ClearAuthoredBehaviors(FrameworkElement& e) noexcept { e.ClearAuthoredBehaviors(); }
    static Span<const Ref<Base::Object>> AuthoredBehaviors(const FrameworkElement& e) noexcept { return e.AuthoredBehaviors(); }
    static void AddStyleBehaviorPrototype(FrameworkElement& e, Ref<Base::Object> behavior) noexcept { e.AddStyleBehaviorPrototype(std::move(behavior)); }
    static void ClearStyleBehaviorPrototypes(FrameworkElement& e) noexcept { e.ClearStyleBehaviorPrototypes(); }
    static Span<const Ref<Base::Object>> StyleBehaviorPrototypes(const FrameworkElement& e) noexcept { return e.StyleBehaviorPrototypes(); }
    static void AddStyleTriggerPrototype(FrameworkElement& e, Ref<Base::Object> trigger) noexcept { e.AddStyleTriggerPrototype(std::move(trigger)); }
    static void ClearStyleTriggerPrototypes(FrameworkElement& e) noexcept { e.ClearStyleTriggerPrototypes(); }
    static Span<const Ref<Base::Object>> StyleTriggerPrototypes(const FrameworkElement& e) noexcept { return e.StyleTriggerPrototypes(); }
};

} // namespace Detail

} // namespace Aero

namespace Aero::Media {
inline constexpr auto FrameworkElementForegroundProperty = ::Aero::FrameworkElement::ForegroundProperty;
}
