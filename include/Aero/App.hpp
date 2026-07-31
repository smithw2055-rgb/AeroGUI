#pragma once

// Optional default desktop application framework. Most WPF-style applications
// need only this header and Aero::App::Run(); advanced host/backend selection is
// explicitly available through the advanced host API.
#include <Aero/Gui.hpp>
#include <Aero/Application.hpp>
#include <Aero/Window.hpp>

namespace Aero::App {

AERO_API int Run() noexcept;

} // namespace Aero::App
