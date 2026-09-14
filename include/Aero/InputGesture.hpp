#pragma once

#include <Aero/Input.hpp>
#include <Aero/Base/Object.hpp>

namespace Aero::Input {

class AERO_GUI_API InputGesture : public Base::Object {
    AERO_DECLARE_TYPE(InputGesture, Base::Object)
public:
    ~InputGesture() override = default;
    virtual bool Matches(const KeyboardInput& input) const noexcept = 0;
    virtual bool MatchesPointer(const PointerInput&) const noexcept {
        return false;
    }

protected:
    InputGesture() noexcept = default;
};
} // namespace Aero::Input
