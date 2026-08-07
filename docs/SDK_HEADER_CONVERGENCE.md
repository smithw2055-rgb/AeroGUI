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
| Onscreen/embedded target | `Aero/RenderTarget.hpp` |
| D3D11 factory | `Aero/Render/D3D11.hpp` |
| OpenGL 3.3 factory | `Aero/Render/OpenGL33.hpp` |
| XAML provider | `Aero/Markup/XamlProvider.hpp` |
| XAML reload | `Aero/Markup/ReloadCoordinator.hpp` |
| Texture provider | `Aero/Media/TextureProvider.hpp` |
| Font provider | `Aero/Text/FontProvider.hpp` |

`Aero/Gui.hpp` is the small embeddable runtime entry header rather than a
catch-all umbrella. WPF types are included explicitly (or through
`Aero/Controls.hpp`), and backend factories remain explicit opt-in includes.

## Source closure

S5A-S5D completed the implementation migration. Repository sources include the
ownership-oriented headers directly, and the former `src/compat/include`
forwarding tree has been removed. The build does not add a compatibility include
path.

External code using a retired include fails immediately, the public header
whitelist contains only canonical paths, and architecture checks reject a
recreated compatibility tree or old include spelling. No `AeroIntegration`
library or `Aero::Integration` CMake target is recreated.
