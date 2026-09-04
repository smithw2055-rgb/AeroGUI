#pragma once

// Private element state and direct Gui runtime declarations.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Visual.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>

#include "gui/core/VisualHandle.hpp"

#include <cstdint>
#include <utility>

namespace Aero::Controls {
class Control;
class Decorator;
class MenuItem;
class Panel;
class TreeViewItem;
}

namespace Aero { class VisualStateManager; }

namespace Aero::Controls::Primitives { class Selector; }

namespace Aero::Shapes { class Path; }

namespace Aero {

class EventRouter;
class InputRouter;
class LayoutEngine;
class BindingEngine;
class AnimationEngine;
class StyleEngine;

} // namespace Aero

// Tree and named view services
#include "gui/core/state/ElementTree.hpp"

// State headers
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/PropertyEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"

#include "gui/internal/AeroGuiInternal.hpp"
