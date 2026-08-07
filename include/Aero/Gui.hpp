#pragma once

// Complete embeddable WPF/XAML product surface. Application lifetime remains
// optional in <Aero/App.hpp>; concrete D3D11/OpenGL factories are opt-in under
// <Aero/Render/>.
#include <Aero/DependencyObject.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Events.hpp>
#include <Aero/UIElement.hpp>

#include <Aero/Controls.hpp>
#include <Aero/Documents.hpp>
#include <Aero/Data.hpp>
#include <Aero/DrawingContext.hpp>
#include <Aero/Input.hpp>
#include <Aero/Input/Platform.hpp>
#include <Aero/Animation.hpp>
#include <Aero/Triggers/Triggers.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Text/FontProvider.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Threading.hpp>

#include <Aero/Markup.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Markup/ReloadCoordinator.hpp>
#include <Aero/View.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/RenderDevice.hpp>
#include <Aero/RenderTarget.hpp>
#include <Aero/Platform/NativeWindow.hpp>
