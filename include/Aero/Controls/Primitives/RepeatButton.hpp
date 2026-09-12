#pragma once

#include <Aero/Controls/Primitives/ButtonBase.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API RepeatButton : public ButtonBase {
    AERO_DECLARE_TYPE(RepeatButton, ButtonBase)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    RepeatButton() noexcept : RepeatButton(StaticTypeId()) {}
    ~RepeatButton() override;

    std::uint32_t GetDelay() const noexcept;
    std::uint32_t GetInterval() const noexcept;
    void SetDelay(std::uint32_t value) noexcept;
    void SetInterval(std::uint32_t value) noexcept;

    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Delay);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Interval);

protected:
    explicit RepeatButton(TypeId runtimeType) noexcept;

    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;
    void OnKeyUp(KeyEventArgs& args) override;
};

} // namespace Aero::Controls::Primitives
