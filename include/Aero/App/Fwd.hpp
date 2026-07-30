#pragma once

namespace Aero {

class Application;
class View;
class Window;

} // namespace Aero

namespace Aero::App {

// Transitional source-compatibility names. The canonical WPF-facing types are
// Aero::Application and Aero::Window; the App namespace owns launcher and
// default application-framework services.
using Application = ::Aero::Application;
using Window = ::Aero::Window;

class ApplicationHost;
using Launcher = ApplicationHost;

} // namespace Aero::App
