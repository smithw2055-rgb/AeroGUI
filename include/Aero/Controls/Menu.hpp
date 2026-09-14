#pragma once

#include <Aero/Controls/MenuItem.hpp>
#include <Aero/Controls/ItemsControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Menu : public ItemsControl {
    AERO_DECLARE_TYPE(Menu, ItemsControl)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Menu() noexcept;
    ~Menu() override;

protected:
    explicit Menu(TypeId runtimeType) noexcept;
    Result<Ref<FrameworkElement>>
        GetContainerForItemOverride() const
            noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;

private:
    MenuItem* FindItem(Base::Object* source) const noexcept;
    Result<void> Invoke(MenuItem& item) noexcept;
};
} // namespace Aero::Controls
