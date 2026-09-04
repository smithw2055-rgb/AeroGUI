#pragma once

#include <Aero/Controls/ListBoxItem.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>

namespace Aero::Controls {

class AERO_GUI_API ListBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ListBox, Primitives::Selector)
public:

    ListBox() noexcept : Primitives::Selector(StaticTypeId()) {}
    ~ListBox() override;

    Result<bool> BringIntoView(
        std::uint32_t index) noexcept;

protected:
    explicit ListBox(TypeId runtimeType) noexcept
        : Primitives::Selector(runtimeType) {}
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item) noexcept override;
};

} // namespace Aero::Controls
