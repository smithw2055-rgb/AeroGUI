#pragma once

// Public retained-mode WPF/XAML class library surface. This header contains UI
// semantics only; application lifetime lives in Aero/App.hpp, metadata authoring
// in Aero/Meta.hpp and engine/backend integration in Aero/Integration.hpp.
#include <Aero/DependencyObject.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/UIElement.hpp>

#include <Aero/Controls.hpp>
#include <Aero/Data.hpp>
#include <Aero/Drawing.hpp>
#include <Aero/Input.hpp>
#include <Aero/Media.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Style.hpp>
#include <Aero/Threading.hpp>
