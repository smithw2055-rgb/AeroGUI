#pragma once

#include <Aero/InputGesture.hpp>

namespace Aero::Input {

class AERO_GUI_API KeyGesture : public InputGesture {
    AERO_DECLARE_TYPE(KeyGesture, InputGesture)
public:
    KeyGesture() noexcept = default;
    KeyGesture(std::uint32_t key, std::uint32_t modifiers = 0U) noexcept
        : key_(key), modifiers_(modifiers) {}

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    std::uint32_t GetKey() const noexcept { return key_; }
    std::uint32_t GetModifiers() const noexcept { return modifiers_; }
    bool IsValid() const noexcept { return key_ != 0U; }
    bool Matches(const KeyboardInput& input) const noexcept override;

private:
    std::uint32_t key_ = 0U;
    std::uint32_t modifiers_ = 0U;
};
} // namespace Aero::Input
