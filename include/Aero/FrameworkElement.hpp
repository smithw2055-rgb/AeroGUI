#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Threading.hpp>
#include <Aero/DrawingContext.hpp>
#include <Aero/Input.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Resources.hpp>

#include <cstdint>


namespace Aero {

using Meta::PropertyInvalidationFlags;
using Meta::TypeId;

enum class FontWeight : std::uint8_t { Normal = 0U, SemiBold, Bold };

class Style;

} // namespace Aero

namespace Aero::Media {

class AERO_API FontFamily : public Base::Object {
    AERO_DECLARE_TYPE(FontFamily, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::StringView GetSource() const noexcept { return source_.View(); }
    void SetSource(Base::StringView value) noexcept { (void)source_.Assign(value); }
private:
    Base::String source_;
};

} // namespace Aero::Media

namespace Aero::Meta {

template<>
struct TypeTraits<Base::Color> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Color"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Color"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Meta

namespace Aero {


class FrameworkElement;

class FrameworkElementChildRange {
public:
    class Iterator {
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
    struct Impl;

    explicit FrameworkElement(TypeId runtimeType) noexcept;
    ~FrameworkElement() override;

    FrameworkElement* AsFrameworkElement() noexcept override { return this; }
    const FrameworkElement* AsFrameworkElement() const noexcept override {
        return this;
    }
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
    Base::Ref<Media::Transform> GetLayoutTransform() const noexcept;
    Base::Transform2D GetLocalVisualTransform() const noexcept;
    Base::Result<Value> GetDataContextResult() const noexcept;
    Base::Ref<Media::FontFamily> GetFontFamily() const noexcept {
        return GetValueOr(
            FontFamilyProperty, Base::Ref<Media::FontFamily>{});
    }
    FlowDirection GetFlowDirection() const noexcept {
        return GetValueOr(FlowDirectionProperty, FlowDirection::LeftToRight);
    }
    Base::Object* FindName(Base::StringView name) noexcept;
    template<class T>
    T* FindName(Base::StringView name) noexcept {
        return static_cast<T*>(FindNameObject(name, T::StaticTypeId()));
    }
    ResourceDictionary& GetResources() noexcept {
        return resources_;
    }
    const ResourceDictionary& GetResources() const noexcept {
        return resources_;
    }
    void SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;
    DependencyObject* GetTemplatedParent() const noexcept {
        return templatedParent_;
    }
    HorizontalAlignment GetHorizontalAlignment() const noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept;
    Value GetDataContext() const noexcept {
        Base::Result<Value> value = GetDataContextResult();
        return value ? value.Value() :
            Value::NullObject(Meta::TypeOf<Base::Object>());
    }

    inline static constexpr Members::Property<Value> DataContextProperty{"DataContext"};
    // A common inherited owner lets Window, controls and text elements share
    // the same WPF-style FontFamily value through the visual tree.
    inline static constexpr Members::Property<Base::Ref<Media::FontFamily>> FontFamilyProperty{"FontFamily"};
    inline static constexpr Members::Property<FlowDirection> FlowDirectionProperty{"FlowDirection"};
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
    inline static constexpr Members::Property<Value> TagProperty{"Tag"};
    inline static constexpr Members::Property<Value> ToolTipProperty{"ToolTip"};
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
    inline static constexpr Members::Property<bool> SnapsToDevicePixelsProperty{"SnapsToDevicePixels"};
    inline static constexpr Members::Property<Base::Ref<Media::Transform>> LayoutTransformProperty{"LayoutTransform"};

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
    void SetDataContext(Base::Ref<Base::Object> value) noexcept {
        SetDataContext(Value::FromObject(
            Meta::TypeOf<Base::Object>(), std::move(value)));
    }
    void SetFontFamily(
        Base::Ref<Media::FontFamily> value) noexcept {
        SetValue(FontFamilyProperty, std::move(value));
    }
    Base::Result<void> SetFontFamily(
        Base::StringView value) noexcept {
        Base::Result<Base::Ref<Media::FontFamily>> family =
            Base::MakeRef<Media::FontFamily>();
        if (!family) return family.GetStatus();
        family.Value()->SetSource(value);
        SetFontFamily(std::move(family).Value());
        return {};
    }
    void SetFlowDirection(FlowDirection value) noexcept {
        SetValue(FlowDirectionProperty, value);
    }
    void ClearDataContext() noexcept;
    void SetHorizontalAlignment(
        HorizontalAlignment value) noexcept;
    void SetVerticalAlignment(
        VerticalAlignment value) noexcept;
    void SetLayoutTransform(
        Base::Ref<Media::Transform> value) noexcept;
    Base::Result<void> InvalidateVisual() noexcept;

protected:
    virtual std::uint32_t GetLogicalChildrenCountCore() const noexcept { return VisualTreeHelper::GetChildrenCount(*this); }
    virtual DependencyObject* GetLogicalChildCore(std::uint32_t index) const noexcept { return LogicalTreeHelper::GetChild(static_cast<const Visual&>(*this), index); }
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual void OnRender(
        DrawingContext& context) noexcept;

private:
    FrameworkElement* GetRenderParent() const noexcept {
        Visual* parent = GetVisualParent();
        return parent != nullptr ? parent->AsFrameworkElement() : nullptr;
    }
    FrameworkElementChildRange GetRenderChildren() const noexcept {
        return FrameworkElementChildRange(*this);
    }
    void SetTemplatedParent(
        DependencyObject* value) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        templatedParent_ = value;
        return;
    }
    Base::Result<void> AddAuthoredTrigger(
        Base::Ref<Base::Object> trigger) noexcept;
    void ClearAuthoredTriggers() noexcept;
    Base::Span<const Base::Ref<Base::Object>>
    AuthoredTriggers() const noexcept {
        return {
            authoredTriggers_.Data(),
            authoredTriggers_.Size()};
    }
    Base::Result<void> AddAuthoredBehavior(
        Base::Ref<Base::Object> behavior) noexcept;
    void ClearAuthoredBehaviors() noexcept;
    Base::Span<const Base::Ref<Base::Object>>
    AuthoredBehaviors() const noexcept {
        return authoredBehaviors_.AsSpan();
    }
    Base::Result<void> AddStyleBehaviorPrototype(
        Base::Ref<Base::Object> behavior) noexcept;
    void ClearStyleBehaviorPrototypes() noexcept;
    Base::Span<const Base::Ref<Base::Object>>
    StyleBehaviorPrototypes() const noexcept {
        return styleBehaviorPrototypes_.AsSpan();
    }
    Base::Result<void> AddStyleTriggerPrototype(
        Base::Ref<Base::Object> trigger) noexcept;
    void ClearStyleTriggerPrototypes() noexcept;
    Base::Span<const Base::Ref<Base::Object>>
    StyleTriggerPrototypes() const noexcept {
        return styleTriggerPrototypes_.AsSpan();
    }

    Base::Object* FindNameObject(
        Base::StringView name,
        Meta::TypeId expectedType) noexcept;

    friend class LogicalTreeHelper;
    friend struct ::Aero::Visual::Impl;
    friend struct ::Aero::UIElement::Impl;
    double dpiScale_ = 1.0;
    DependencyObject* templatedParent_ = nullptr;
    ResourceDictionary resources_;
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers_;
    Base::Vector<Base::Ref<Base::Object>> authoredBehaviors_;
    Base::Vector<Base::Ref<Base::Object>> styleBehaviorPrototypes_;
    Base::Vector<Base::Ref<Base::Object>> styleTriggerPrototypes_;
};

} // namespace Aero
