# S4 SDK header convergence

## Installed layout

The installed SDK no longer contains `Aero/Integration.hpp` or an
`Aero/Integration/` directory. The former catch-all group is split by actual
ownership:

| Contract | Public header |
| --- | --- |
| View creation behavior | `Aero/ViewOptions.hpp` |
| Clipboard and IME host seams | `Aero/Input/Platform.hpp` |
| Native window handle | `Aero/Platform/NativeWindow.hpp` |
| Onscreen/embedded target | `Aero/RenderSurface.hpp` |
| D3D11 factory | `Aero/Render/D3D11.hpp` |
| OpenGL 3.3 factory | `Aero/Render/OpenGL33.hpp` |
| XAML provider | `Aero/Markup/XamlProvider.hpp` |
| XAML reload | `Aero/Markup/ReloadCoordinator.hpp` |
| Texture provider | `Aero/Media/TextureProvider.hpp` |
| Font provider | `Aero/Text/FontProvider.hpp` |

`Aero/Gui.hpp` is the normal embeddable product umbrella. Backend factories
remain explicit opt-in includes.

## Source transition

Existing implementation files are not all renamed in the same patch. A private
forwarding tree under `src/compat/include` maps retired include spellings to the
new public paths. CMake exposes that directory only while building AeroGUI; it
is never installed or exported.

This is intentionally narrower than public compatibility:

- external code using an old include fails immediately;
- the public header whitelist contains only the new paths;
- architecture checks reject any old path from installed headers;
- no `AeroIntegration` library or `Aero::Integration` CMake target is recreated.

The following stage can update implementation includes and migrate the C++
namespace without mixing that repository-wide rename into the physical SDK
layout change.
