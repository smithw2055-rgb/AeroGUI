#pragma once

#include <Aero/Presentation/Style.hpp>

namespace Aero {

// WPF System.Windows style authoring surface. Runtime plans and managers remain
// in the Presentation implementation namespace.
using Setter = Presentation::Setter;
using TriggerBase = Presentation::TriggerBase;
using Trigger = Presentation::PropertyTrigger;
using DataTrigger = Presentation::DataTrigger;
using Condition = Presentation::Condition;
using MultiTrigger = Presentation::MultiTrigger;
using MultiDataTrigger = Presentation::MultiDataTrigger;
using Style = Presentation::Style;

} // namespace Aero
