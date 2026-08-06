# S5D compatibility-tree removal

Host-facing SDK contracts and repository sources now include their canonical
ownership headers directly. The source tree no longer contains
`src/compat/include`, and the build no longer injects a private include path that
can resolve retired `Aero/Integration/...` spellings.

Runtime and application code use the domain types directly:

- `Aero::ViewOptions` and `Aero::TextOptions`;
- `Aero::Input` clipboard and text-composition contracts;
- `Aero::Platform::NativeWindowHandle`;
- `Aero::Markup`, `Aero::Media` and `Aero::Text` providers;
- `Aero::RenderSurface` and `Aero::Render` backend factories.

Native D3D11/OpenGL implementation entry points remain source-private in
`Aero::Integration`. Their declarations are concentrated in
`src/integration/BackendApi.hpp`; this is an implementation boundary, not an
installed header, compatibility facade or second SDK product.

Architecture checks reject a recreated compatibility directory, a CMake include
path to it, retired public include spellings in C++ sources, and host-facing
`Integration` types in installed headers.
