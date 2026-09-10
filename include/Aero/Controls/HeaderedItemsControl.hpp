#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/DataTemplate.hpp>

#include <utility>

namespace Aero::Controls {

class AERO_GUI_API HeaderedItemsControl : public ItemsControl {
    AERO_DECLARE_TYPE(HeaderedItemsControl, ItemsControl)
public:
    HeaderedItemsControl() noexcept : ItemsControl(StaticTypeId()) {}
    ~HeaderedItemsControl() override = default;

    Value GetHeader() const noexcept {
        return GetValue(HeaderProperty);
    }
    void SetHeader(Value value) noexcept {
        SetValue(HeaderProperty, std::move(value));
    }
    void SetHeader(StringView value) noexcept {
        Result<Value> boxed = Value::TryFromString(
            Meta::TypeOf<String>(), value);
        if (!boxed) { AERO_ASSERT(false); return; }
        SetHeader(std::move(boxed).Value());
    }
    Ref<DataTemplate> GetHeaderTemplate() const noexcept {
        return GetValue(HeaderTemplateProperty);
    }
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept {
        SetValue(HeaderTemplateProperty, std::move(value));
    }

    AERO_DEPENDENCY_PROPERTY(Value, Header);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, HeaderTemplate);

protected:
    explicit HeaderedItemsControl(Meta::TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
    virtual void OnHeaderChanged(
        const Value& oldHeader,
        const Value& newHeader) {
        (void)oldHeader;
        (void)newHeader;
    }
    virtual void OnHeaderTemplateChanged(
        const Ref<DataTemplate>& oldTemplate,
        const Ref<DataTemplate>& newTemplate) {
        (void)oldTemplate;
        (void)newTemplate;
    }
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override {
        ItemsControl::OnPropertyChanged(args);
        if (args.GetProperty() == HeaderProperty) {
            OnHeaderChanged(args.GetOldValue(), args.GetNewValue());
        } else if (args.GetProperty() == HeaderTemplateProperty) {
            const auto toTemplate = [](const Value& v) -> Ref<DataTemplate> {
                if (v.Kind() == Meta::ValueKind::Object && v.AsObject()) {
                    if (auto* dt = TryCast<DataTemplate>(v.AsObject().Get())) {
                        return Ref<DataTemplate>::FromBorrowed(*dt);
                    }
                }
                return {};
            };
            OnHeaderTemplateChanged(
                toTemplate(args.GetOldValue()),
                toTemplate(args.GetNewValue()));
        }
    }
};

} // namespace Aero::Controls
