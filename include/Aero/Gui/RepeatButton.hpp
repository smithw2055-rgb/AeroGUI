#pragma once

#include <Aero/Gui/ButtonBase.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API RepeatButton : public ButtonBase {
    AERO_DECLARE_TYPE(RepeatButton, ButtonBase)
public:
    RepeatButton() noexcept : RepeatButton(StaticTypeId()) {}
    ~RepeatButton() override = default;

    std::uint32_t GetDelay() const noexcept;
    std::uint32_t GetInterval() const noexcept;
    void SetDelay(std::uint32_t value) noexcept;
    void SetInterval(std::uint32_t value) noexcept;

    inline static constexpr DependencyProperty<std::uint32_t> DelayProperty{"Delay"};
    inline static constexpr DependencyProperty<std::uint32_t> IntervalProperty{"Interval"};

protected:
    explicit RepeatButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}
};

} // namespace Aero::Controls::Primitives
