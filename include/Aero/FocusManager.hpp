#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Input {

class AERO_GUI_API FocusManager : public Base::Object {
    AERO_DECLARE_TYPE(FocusManager, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr AttachedProperty<bool> IsFocusScopeProperty{"IsFocusScope"};
    inline static constexpr AttachedProperty<Ref<Base::Object>> FocusedElementProperty{"FocusedElement"};
};
} // namespace Aero::Input
