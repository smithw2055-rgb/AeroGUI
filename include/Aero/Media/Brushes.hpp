#pragma once

#include <Aero/Presentation/Brushes.hpp>

namespace Aero::Media {

// Transitional semantic projection from the existing Presentation
// implementation namespace. XAML local names and runtime TypeId values are
// unchanged while declarations are moved incrementally.
using Color = ::Aero::Presentation::Color;
using TileMode = ::Aero::Presentation::TileMode;
using BrushMappingMode = ::Aero::Presentation::BrushMappingMode;

using Brush = ::Aero::Presentation::Brush;
using SolidColorBrush = ::Aero::Presentation::SolidColorBrush;
using GradientStop = ::Aero::Presentation::GradientStop;
using GradientStopCollection =
    ::Aero::Presentation::GradientStopCollection;
using GradientBrush = ::Aero::Presentation::GradientBrush;
using LinearGradientBrush =
    ::Aero::Presentation::LinearGradientBrush;
using RadialGradientBrush =
    ::Aero::Presentation::RadialGradientBrush;
using ImageBrush = ::Aero::Presentation::ImageBrush;

// Aero extensions remain in Media but are not part of the WPF compatibility
// contract.
using MonochromeBrush = ::Aero::Presentation::MonochromeBrush;
using ConicGradientBrush =
    ::Aero::Presentation::ConicGradientBrush;
using WavesBrush = ::Aero::Presentation::WavesBrush;

} // namespace Aero::Media
