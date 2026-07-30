#pragma once

#include <Aero/Presentation/AnimationXaml.hpp>

namespace Aero::Media {

// The existing XAML animation object model is already isolated from the
// Presentation runtime records. Publish it under the WPF semantic namespace
// while source migration proceeds.
namespace Animation = ::Aero::Animation;

} // namespace Aero::Media
