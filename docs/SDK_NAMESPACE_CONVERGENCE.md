# S5B backend and reload namespace convergence

The installed headers now expose host-facing contracts through their owning
C++ namespaces:

- `Aero::ViewOptions` and `Aero::RenderSurface`;
- `Aero::Input` for clipboard and text composition host seams;
- `Aero::Platform` for native window handles;
- `Aero::Markup` for XAML providers and reload coordination;
- `Aero::Media` for texture providers;
- `Aero::Text` for font providers;
- `Aero::Render` for D3D11 and OpenGL 3.3 factory APIs.

D3D11 and OpenGL factory declarations now belong directly to `Aero::Render`;
the product binary exports forwarding symbols in that namespace while the
existing backend implementations remain source-private. `ReloadCoordinator`
now has its real declaration and implementation in `Aero::Markup`.

The non-installed compatibility headers declare only the legacy symbols needed
by backend implementation files. They are no longer part of the public type
model. RenderSurface implementation migration and complete removal of the
compatibility tree remain the final S5C closure.
