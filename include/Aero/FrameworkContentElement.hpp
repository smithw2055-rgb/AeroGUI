#pragma once

#include <Aero/ContentElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>

namespace Aero {

// WPF-shaped non-visual content node with resources, DataContext, Style and
// logical-tree participation. TextElement and other document nodes derive here.
class AERO_GUI_API FrameworkContentElement : public ContentElement {
    AERO_DECLARE_TYPE(FrameworkContentElement, ContentElement)
public:
    explicit FrameworkContentElement(Meta::TypeId runtimeType) noexcept;
    ~FrameworkContentElement() override;

    ResourceDictionary& GetResources() noexcept { return resources_; }
    const ResourceDictionary& GetResources() const noexcept { return resources_; }
    void SetResources(Ref<ResourceDictionary> value) noexcept;

    Value GetDataContext() const noexcept {
        return GetValueOr(
            DataContextProperty,
            Value::NullObject(Meta::TypeOf<Base::Object>()));
    }
    void SetDataContext(Value value) noexcept {
        SetValue(DataContextProperty, std::move(value));
    }
    void SetDataContext(Ref<Base::Object> value) noexcept {
        SetDataContext(Value::FromObject(
            Meta::TypeOf<Base::Object>(), std::move(value)));
    }
    void ClearDataContext() noexcept {
        ClearValue(DataContextProperty);
    }

    Ref<Style> GetStyle() const noexcept {
        return GetValueOr(StyleProperty, Ref<Style>{});
    }
    void SetStyle(Ref<Style> value) noexcept {
        SetValue(StyleProperty, std::move(value));
    }

    bool GetIsEnabled() const noexcept {
        return GetValueOr(IsEnabledProperty, true);
    }
    void SetIsEnabled(bool value) noexcept {
        SetValue(IsEnabledProperty, value);
    }
    bool GetIsMouseOver() const noexcept {
        return GetValueOr(IsMouseOverProperty, false);
    }
    StringView GetCursor() const noexcept {
        return GetValueOr(CursorProperty, StringView{});
    }
    void SetCursor(StringView value) noexcept {
        SetValue(CursorProperty, value);
    }
    bool GetOverridesDefaultStyle() const noexcept {
        return GetValueOr(OverridesDefaultStyleProperty, false);
    }
    void SetOverridesDefaultStyle(bool value) noexcept {
        SetValue(OverridesDefaultStyleProperty, value);
    }

    inline static constexpr DependencyProperty<Value> DataContextProperty{"DataContext"};
    inline static constexpr DependencyProperty<Ref<Style>> StyleProperty{"Style"};
    inline static constexpr DependencyProperty<Value> TagProperty{"Tag"};
    inline static constexpr DependencyProperty<bool> IsEnabledProperty{"IsEnabled"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsMouseOverProperty{"IsMouseOver"};
    inline static constexpr DependencyProperty<String> CursorProperty{"Cursor"};
    inline static constexpr DependencyProperty<bool> OverridesDefaultStyleProperty{"OverridesDefaultStyle"};

protected:
    virtual std::uint32_t GetLogicalChildrenCount() const noexcept { return 0U; }
    virtual DependencyObject* GetLogicalChild(std::uint32_t) const noexcept { return nullptr; }

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
    Result<void> AddAuthoredTrigger(
        Ref<Base::Object> trigger) noexcept;
    void ClearAuthoredTriggers() noexcept;
    Span<const Ref<Base::Object>>
    AuthoredTriggers() const noexcept {
        return authoredTriggers_.AsSpan();
    }
    ResourceDictionary resources_;
    Base::Vector<Ref<Base::Object>> authoredTriggers_;
};

} // namespace Aero
