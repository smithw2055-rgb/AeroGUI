# S5A SDK namespace convergence

The installed headers now expose host-facing contracts through their owning
C++ namespaces:

- `Aero::ViewOptions` and `Aero::RenderSurface`;
- `Aero::Input` for clipboard and text composition host seams;
- `Aero::Platform` for native window handles;
- `Aero::Markup` for XAML providers and reload coordination;
- `Aero::Media` for texture providers;
- `Aero::Text` for font providers;
- `Aero::Render` for D3D11 and OpenGL 3.3 factory APIs.

The domain spellings are the canonical authoring surface. During S5A, selected
ABI-bearing declarations still use `Aero::Integration` internally and publish
domain aliases from the new headers. Repository sources that still use old
include paths consume private aliases from `src/compat/include`; that tree is
never installed.

S5B moves the remaining ABI-bearing declarations and implementations, removes
the legacy namespace spellings, and deletes the private forwarding tree.
