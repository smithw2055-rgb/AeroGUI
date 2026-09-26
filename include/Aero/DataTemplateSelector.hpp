#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/DataTemplate.hpp>

namespace Aero {

class DependencyObject;

class AERO_GUI_API DataTemplateSelector : public Base::Object {
    AERO_DECLARE_TYPE(DataTemplateSelector, Base::Object)
public:
    DataTemplateSelector() noexcept = default;

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    virtual Ref<DataTemplate> SelectTemplate(
        Base::Object* item,
        DependencyObject* container) noexcept {
        (void)item;
        (void)container;
        return {};
    }
};

} // namespace Aero
