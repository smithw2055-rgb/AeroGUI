#pragma once

#include <Aero/Controls/MenuItem.hpp>
#include <Aero/Controls/ItemsControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Menu : public ItemsControl {
    AERO_DECLARE_TYPE(Menu, ItemsControl)
public:

    Menu() noexcept
        : Menu(StaticTypeId()) {}
    ~Menu() override;

protected:
    explicit Menu(TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;
};
} // namespace Aero::Controls
