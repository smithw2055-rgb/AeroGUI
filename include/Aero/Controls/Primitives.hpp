#pragma once

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>

namespace Aero::Controls::Primitives {

// Transitional WPF-aligned namespace projection. Existing implementations are
// moved into this namespace incrementally; these aliases establish the stable
// public spelling without changing XAML identity or runtime TypeId values.
using ButtonBase = ::Aero::Controls::ButtonBase;
using RepeatButton = ::Aero::Controls::RepeatButton;
using ToggleButton = ::Aero::Controls::ToggleButton;
using RangeBase = ::Aero::Controls::RangeBase;
using ScrollBar = ::Aero::Controls::ScrollBar;
using Selector = ::Aero::Controls::Selector;
using Thumb = ::Aero::Controls::Thumb;
using Track = ::Aero::Controls::Track;
using Popup = ::Aero::Controls::Popup;
using PlacementMode = ::Aero::Controls::PlacementMode;
using PopupAnimation = ::Aero::Controls::PopupAnimation;

} // namespace Aero::Controls::Primitives
