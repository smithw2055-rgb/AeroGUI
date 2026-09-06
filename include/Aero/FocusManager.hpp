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

    AERO_ATTACHED_PROPERTY(bool, IsFocusScope);
    AERO_ATTACHED_PROPERTY(Ref<Base::Object>, FocusedElement);
};
} // namespace Aero::Input
