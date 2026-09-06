#pragma once

#include <Aero/DataTemplate.hpp>
#include <Aero/Controls/ContentControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API HeaderedContentControl
    : public ContentControl {
    AERO_DECLARE_TYPE(
        HeaderedContentControl,
        ContentControl)
public:
    Value GetHeader() const noexcept;
    void SetHeader(const Value& value) noexcept;
    void SetHeader(StringView value) noexcept;
    Ref<DataTemplate> GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept;

    // WPF headers are content, not just text. They can hold an element, a
    // resource object, a scalar, or x:Null and are consumed by a
    // ContentPresenter through ContentSource="Header".
    AERO_DEPENDENCY_PROPERTY(Value, Header);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, HeaderTemplate);

protected:
    explicit HeaderedContentControl(
        TypeId runtimeType) noexcept;
    ~HeaderedContentControl() override;
    void OnApplyTemplate() noexcept override;

private:
    void OnHeaderChanged(
        DependencyObject&,
        const DependencyPropertyChangedEventArgs&) noexcept;
    void ProjectHeaderContent() noexcept;
    DependencyPropertyChangedEventHandler headerChangedHandler_;
};

} // namespace Aero::Controls
