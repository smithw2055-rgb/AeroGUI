# S6 render-private namespace convergence

All host-facing contracts already use their owning public namespaces. S6 now
removes the final C++ implementation namespace left behind by the former
Integration product.

Private render state belongs to `Aero::Render::Detail`:

- immutable retained `RenderFrame` snapshots and diagnostics;
- native render devices and targets;
- backend health and surface health state;
- backend adoption and headless-device factories;
- D3D11 and OpenGL implementation entry points.

Other retired Integration aliases are normalized to their owning domains:
`Aero::Input`, `Aero::Platform`, `Aero::Markup`, `Aero::Media`, `Aero::Text`,
`Aero::Render`, or the root `Aero` namespace. Old Integration include paths are
removed from source and conformance consumers as well.

`Aero::RenderSurface` and `Aero::RenderDevice` remain the host-facing objects.
D3D11/OpenGL creation remains under the opt-in public `Aero::Render` factory
headers. The `src/integration` directory is only a temporary physical source
location and no longer defines an Integration namespace or product boundary.

`src/integration/D3D11Device.cpp` is removed in this stage. It is an obsolete,
unreferenced pre-convergence duplicate: it includes the already retired public
Integration header and calls the superseded render-device adoption signature.
The active Windows backend remains `D3D11Shared.cpp`.

Architecture checks reject `namespace Aero::Integration`,
`::Aero::Integration`, any `Integration::` spelling, and any
`Aero/Integration/...` include in C++ source. A later physical-layout stage may
move the remaining source files without changing API or implementation
ownership again.
