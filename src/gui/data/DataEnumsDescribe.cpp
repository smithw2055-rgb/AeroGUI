#include "gui/core/EnumRegistration.hpp"

#include <Aero/Data/Binding.hpp>
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

    AERO_REGISTER_ENUM(
        BindingMode,
        "BindingMode",
        description
            .Value("OneTime", BindingMode::OneTime)
            .Value("OneWay", BindingMode::OneWay)
            .Value("TwoWay", BindingMode::TwoWay)
            .Value("OneWayToSource", BindingMode::OneWayToSource)
            .Value("Default", BindingMode::Default););

    AERO_REGISTER_ENUM(
        UpdateSourceTrigger,
        "UpdateSourceTrigger",
        description
            .Value("Default", UpdateSourceTrigger::Default)
            .Value("PropertyChanged", UpdateSourceTrigger::PropertyChanged)
            .Value("LostFocus", UpdateSourceTrigger::LostFocus)
            .Value("Explicit", UpdateSourceTrigger::Explicit););

    AERO_REGISTER_ENUM(
        RelativeSourceMode,
        "RelativeSourceMode",
        description
            .Value("PreviousData", RelativeSourceMode::PreviousData)
            .Value("TemplatedParent", RelativeSourceMode::TemplatedParent)
            .Value("Self", RelativeSourceMode::Self)
            .Value("FindAncestor", RelativeSourceMode::FindAncestor););

#undef AERO_REGISTER_ENUM
    return {};
}

} // namespace Aero
