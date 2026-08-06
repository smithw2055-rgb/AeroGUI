# S5C RenderSurface namespace convergence

The host-facing SDK contracts now have real declarations in their owning C++
namespaces. `Aero::RenderSurface`, `Aero::PresentMode`,
`Aero::RenderSurfaceKind` and `Aero::RenderSurfaceState` are no longer aliases
to `Aero::Integration` declarations.

The RenderSurface implementation, private state and per-View renderer call path
all use the canonical `Aero` type. Native backend targets remain source-private
under `Aero::Integration::Detail`; this is an implementation domain rather than
a public product or authoring namespace.

The non-installed compatibility headers now provide only source aliases needed
by the existing D3D11/OpenGL backend translation units. They do not create a
second class, vtable or exported ABI identity.

The final S5D closure removes those aliases by migrating the remaining platform
and backend translation units to their canonical headers and then deleting
`src/compat/include` and its CMake include path.
