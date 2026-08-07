#pragma once

// Source-only compatibility spelling for backend files. Renderer is the single
// semantic device renderer; this alias has no separate state or lifetime.
#include "Renderer.hpp"

namespace Aero::Render {
using DeviceRenderer = Renderer;
}
