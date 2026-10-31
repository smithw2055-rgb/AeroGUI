#include "gui/core/EnumRegistration.hpp"

#include <Aero/InputScope.hpp>
#include <Aero/Input.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/KeyboardNavigation.hpp>

namespace Aero {

void PopulateInputEnums(
    Meta::Registration& context) noexcept {
    using namespace Input;

    AERO_AUTO_REGISTER_ENUM(InputScope);
    AERO_AUTO_REGISTER_ENUM(DragDropEffects);
    AERO_AUTO_REGISTER_ENUM(CursorType);
    AERO_AUTO_REGISTER_ENUM(KeyboardNavigationMode);
}

} // namespace Aero
