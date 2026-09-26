#pragma once

#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
class AERO_GUI_API UserControl : public ContentControl {
    AERO_DECLARE_TYPE(UserControl, ContentControl)
public:
    UserControl() noexcept : ContentControl(StaticTypeId()) {}
    ~UserControl() override = default;
protected:
    explicit UserControl(TypeId runtimeType) noexcept : ContentControl(runtimeType) {}
};

// Navigable content surface. WPF Page derives FrameworkElement and owns Content
// itself, so Page styles do not match UserControl. Text, objects, and a
// ContentTemplate are hosted by an inner content control.
class AERO_GUI_API Page : public FrameworkElement {
    AERO_DECLARE_TYPE(Page, FrameworkElement)
public:
    Page() noexcept : FrameworkElement(StaticTypeId()) {}
    ~Page() override {
        if (host_ && host_->GetVisualParent() == this) { RemoveVisualChild(host_.Get()); }
    }
    UIElement* GetContent() const noexcept;
    Value GetContentValue() const noexcept;
    void SetContent(UIElement* content) noexcept;
    void SetContent(Ref<UIElement> content) noexcept;
    void SetContent(StringView text) noexcept;
    void SetContent(Value value) noexcept;
    Ref<Base::Object> GetContentTemplate() const noexcept;
    void SetContentTemplate(Ref<Base::Object> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Value, Content);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ContentTemplate);

protected:
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        return host_ && host_->GetVisualParent() == this ? 1U : 0U;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        if (index != 0U || !host_ || host_->GetVisualParent() != this) { return nullptr; }
        return host_.Get();
    }
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    void EnsureHost() noexcept;
    Ref<ContentControl> host_;
    bool synchronizingContent_ = false;
};

} // namespace Aero::Controls
