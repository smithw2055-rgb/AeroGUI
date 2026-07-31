#pragma once

namespace Aero {

class Application;
class View;
class Window;

} // namespace Aero

namespace Aero::App {

// App owns launcher and default application-framework services. The canonical
// WPF-facing object types remain Aero::Application and Aero::Window.
class ApplicationHost;
class Launcher;

} // namespace Aero::App
