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

    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Ref<ResourceDictionary> value) noexcept;

    Value GetDataContext() const noexcept {
        return GetValue(DataContextProperty);
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
        return GetValue(StyleProperty);
    }
    void SetStyle(Ref<Style> value) noexcept {
        SetValue(StyleProperty, std::move(value));
    }

    bool GetIsEnabled() const noexcept {
        return GetValue(IsEnabledProperty);
    }
    void SetIsEnabled(bool value) noexcept {
        SetValue(IsEnabledProperty, value);
    }
    bool GetIsMouseOver() const noexcept {
        return GetValue(IsMouseOverProperty);
    }
    StringView GetCursor() const noexcept {
        return GetValue(CursorProperty);
    }
    void SetCursor(StringView value) noexcept {
        SetValue(CursorProperty, value);
    }
    bool GetOverridesDefaultStyle() const noexcept {
        return GetValue(OverridesDefaultStyleProperty);
    }
    void SetOverridesDefaultStyle(bool value) noexcept {
        SetValue(OverridesDefaultStyleProperty, value);
    }

    AERO_DEPENDENCY_PROPERTY(Value, DataContext);
    AERO_DEPENDENCY_PROPERTY(Ref<Style>, Style);
    AERO_DEPENDENCY_PROPERTY(Value, Tag);
    AERO_DEPENDENCY_PROPERTY(bool, IsEnabled);
    AERO_READONLY_PROPERTY(bool, IsMouseOver);
    AERO_DEPENDENCY_PROPERTY(String, Cursor);
    AERO_DEPENDENCY_PROPERTY(bool, OverridesDefaultStyle);

protected:
    virtual std::uint32_t GetLogicalChildrenCount() const noexcept { return 0U; }
    virtual DependencyObject* GetLogicalChild(std::uint32_t) const noexcept { return nullptr; }

private:
    friend class ResourceResolver;
    void AddAuthoredTrigger(
        Ref<Base::Object> trigger) noexcept;
    void ClearAuthoredTriggers() noexcept;
    Span<const Ref<Base::Object>>
    AuthoredTriggers() const noexcept;
    const ResourceDictionary* LocalResources() const noexcept {
        return resources_;
    }
    mutable ResourceDictionary* resources_ = nullptr;
    struct FrameworkContentRare;
    FrameworkContentRare* EnsureFrameworkContentRare() noexcept;
    FrameworkContentRare* frameworkRare_ = nullptr;
};

} // namespace Aero
