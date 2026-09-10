#pragma once

#include <Aero/Controls/MenuItem.hpp>
#include <Aero/Controls/ItemsControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Menu : public ItemsControl {
    AERO_DECLARE_TYPE(Menu, ItemsControl)
public:

    Menu() noexcept;
    ~Menu() override;

protected:
    explicit Menu(TypeId runtimeType) noexcept;
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;
    virtual void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    virtual void OnKeyDown(KeyEventArgs& args);

private:
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    void HandleMouseDown(Base::Object* sender, MouseButtonEventArgs& args) noexcept;
    void HandleKeyDown(Base::Object* sender, KeyEventArgs& args) noexcept;
    MenuItem* FindItem(Base::Object* source) const noexcept;
    Result<void> Invoke(MenuItem& item) noexcept;
};
} // namespace Aero::Controls
