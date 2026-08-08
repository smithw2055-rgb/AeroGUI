#pragma once

#include <Aero/Gui/ItemsControl.hpp>

#include <utility>

namespace Aero::Controls {

class AERO_API HeaderedItemsControl : public ItemsControl {
    AERO_DECLARE_TYPE(HeaderedItemsControl, ItemsControl)
public:
    HeaderedItemsControl() noexcept : ItemsControl(StaticTypeId()) {}
    ~HeaderedItemsControl() override = default;

    Value GetHeader() const noexcept {
        return GetValueOr(
            HeaderProperty,
            Value::NullObject(Meta::TypeOf<Base::Object>()));
    }
    void SetHeader(Value value) noexcept {
        SetValue(HeaderProperty, std::move(value));
    }
    Base::Result<void> SetHeader(Base::StringView value) noexcept {
        Base::Result<Value> boxed = Value::TryFromString(
            Meta::TypeOf<Base::String>(), value);
        if (!boxed) return boxed.GetStatus();
        SetHeader(std::move(boxed).Value());
        return {};
    }
    Base::Ref<DataTemplate> GetHeaderTemplate() const noexcept {
        return GetValueOr(
            HeaderTemplateProperty, Base::Ref<DataTemplate>{});
    }
    void SetHeaderTemplate(Base::Ref<DataTemplate> value) noexcept {
        SetValue(HeaderTemplateProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};

protected:
    explicit HeaderedItemsControl(Meta::TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
};

} // namespace Aero::Controls
