#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Value.hpp>
#include <cstdint>

namespace Aero::Data {

enum class ListSortDirection : std::uint8_t {
    Ascending = 0U,
    Descending
};

struct SortDescription {
    String propertyName;
    ListSortDirection direction = ListSortDirection::Ascending;
};

} // namespace Aero::Data

AERO_DECLARE_TYPE_ENUM(Aero::Data::ListSortDirection)
