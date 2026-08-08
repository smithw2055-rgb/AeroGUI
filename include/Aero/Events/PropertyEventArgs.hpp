#pragma once

// Dependency-property change arguments are owned by the property contract;
// this event-specific entry point keeps event consumers from depending on a
// larger UI aggregate while preserving the canonical Aero/Meta definitions.
#include <Aero/Gui/DependencyProperty.hpp>
