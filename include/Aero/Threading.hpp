#pragma once

#include <Aero/Core/Dispatcher.hpp>

namespace Aero::Threading {

using Dispatcher = Core::Dispatcher;
using DispatcherObject = Core::DispatcherObject;
using DispatcherPriority = Core::DispatcherPriority;
using DispatcherOperationHandle = Core::DispatcherTaskHandle;
using DispatcherReentrancyGuard =
    Core::DispatcherReentrancyGuard;

} // namespace Aero::Threading
