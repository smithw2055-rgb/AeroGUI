#include "gui/core/EnumRegistration.hpp"

#include <Aero/Data/SortDescription.hpp>

namespace Aero {

Base::Result<void> PopulateDataEnums(
    Meta::Registration& context) noexcept {
    using namespace Data;

    Base::Result<void> status;

    AERO_REGISTER_ENUM(
        ListSortDirection,
        "ListSortDirection",
        description
            .Value("Ascending", ListSortDirection::Ascending)
            .Value("Descending", ListSortDirection::Descending););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
