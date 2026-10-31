#include "gui/core/EnumRegistration.hpp"

#include <Aero/Data/Binding.hpp>
#include <Aero/Data/SortDescription.hpp>

namespace Aero {

void PopulateDataEnums(
    Meta::Registration& context) noexcept {
    using namespace Data;

    AERO_AUTO_REGISTER_ENUM(ListSortDirection);
    AERO_AUTO_REGISTER_ENUM(BindingMode);
    AERO_AUTO_REGISTER_ENUM(UpdateSourceTrigger);
    AERO_AUTO_REGISTER_ENUM(RelativeSourceMode);
}

} // namespace Aero
