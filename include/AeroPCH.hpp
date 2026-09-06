#pragma once

// Single precompiled-header / umbrella entry for new hosts.
// Mirrors the NoesisPCH grouping so WPF/Noesis users find one include.
// Grouped, not transitive-bloated: advanced users keep including type headers
// directly (e.g. <Aero/Controls/Button.hpp>).

// Core values and ownership (cf. NsCore/NsMath/NsDrawing)
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Value.hpp>

// Dependency system and tree spine (cf. NsGui/Core)
#include <Aero/DependencyProperty.hpp>
#include <Aero/DependencyObject.hpp>
#include <Aero/Visual.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>

// Authoring and runtime entry (cf. NsGui/IntegrationAPI)
#include <Aero/Meta.hpp>
#include <Aero/Gui.hpp>
#include <Aero/View.hpp>
#include <Aero/IRenderer.hpp>

// Controls, data, media (cf. NsGui/Controls, Animation)
#include <Aero/Controls.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Animation.hpp>
