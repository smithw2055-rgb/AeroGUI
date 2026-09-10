#pragma once

#include <Aero/Controls/ListBoxItem.hpp>
#include <Aero/Controls/Primitives/Selector.hpp>

namespace Aero::Controls {

class AERO_GUI_API ListBox : public Primitives::Selector {
    AERO_DECLARE_TYPE(ListBox, Primitives::Selector)
public:

    ListBox() noexcept;
    ~ListBox() override;

    Result<bool> BringIntoView(
        std::uint32_t index) noexcept;

protected:
    explicit ListBox(TypeId runtimeType) noexcept;
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item) noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;

private:
    std::uint32_t anchorIndex_ = UINT32_MAX;

    std::uint32_t FindContainerIndex(Base::Object* source) const noexcept;
    Result<bool> ApplyUserSelection(std::uint32_t index, std::uint32_t modifiers) noexcept;
};

} // namespace Aero::Controls
