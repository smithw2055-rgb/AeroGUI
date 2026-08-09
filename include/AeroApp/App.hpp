#pragma once

// Optional default desktop application framework. WPF-style applications
// construct an Aero::Application and call Application::Run(). App::Run() is
// retained only as the generated/XAML bootstrap that loads App.xaml.
#include <Aero/Gui.hpp>
#include <AeroApp/Application.hpp>
#include <AeroApp/Window.hpp>
#include <Aero/View.hpp>
#include <Aero/Module.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Diagnostics { class IDiagnosticSink; }

namespace Aero::App {

enum class GraphicsBackend : std::uint8_t {
    Automatic = 0U,
    D3D11,
    OpenGL33
};

struct RunOptions  {
    // Used as the XAML bootstrap document by App::Run() and as the URI/font
    // resolution base by Application::Run().
    StringView applicationFile = "App.xaml";
    GraphicsBackend graphicsBackend = GraphicsBackend::Automatic;
    std::uint32_t defaultWidth = 0U;
    std::uint32_t defaultHeight = 0U;
    bool visible = true;
    bool resizable = true;
    bool automaticAnimationClock = true;
    bool loadBuiltInTheme = true;
    BuiltInTheme builtInTheme = BuiltInTheme::Light;
    Span<const ModuleRegistration> modules;
    Diagnostics::IDiagnosticSink* diagnostics = nullptr;
    Base::IAllocator* allocator = nullptr;
};

// Generated/XAML-only bootstrap. Ordinary C++ applications should call
// Application::Run() on their application instance.
AERO_APP_API int Run(const RunOptions& options = {}) noexcept;

} // namespace Aero::App
