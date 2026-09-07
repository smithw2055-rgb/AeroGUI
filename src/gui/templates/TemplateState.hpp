#pragma once

// Former TemplatePrivate static API now lives on FrameworkTemplateState
// (TemplateProgram.hpp). Keep this header as the stable include surface used
// by controls/markup TUs; *TemplateState POD storage remains unchanged.

#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>

namespace Aero {
class AnimationEngine;
}

namespace Aero::Controls {
class TemplateEngine;
}
