# S7 physical source-layout convergence

S6 removed the final C++ `Aero::Integration` namespace. S7 removes the remaining
historical repository layout as well: no active source file lives under
`src/integration` after this stage.

The physical ownership model is now direct and intentionally shallow:

- `src/render/RenderDevice.cpp` implements the host-facing `Aero::RenderDevice`;
- `src/render/RenderSurface.cpp` implements `Aero::RenderSurface`;
- `src/render/private` owns native render-device/surface and backend factory
  contracts used only inside the product;
- `src/render/d3d11` owns D3D11 device/surface/factory implementation;
- `src/render/opengl33` owns OpenGL 3.3 device/surface/factory implementation;
- `src/markup/ReloadCoordinator.cpp` owns markup reload implementation.

The former low-level `src/render/RenderDevice.cpp/.hpp` pair and
`RenderDeviceResources.cpp` actually implement `Aero::Graphics::Device`. They
are renamed to `GraphicsDevice.cpp/.hpp` and `GraphicsDeviceResources.cpp` so
the source tree no longer contains two unrelated RenderDevice concepts.

`IntegrationPrivate.hpp` is deleted instead of being renamed. Its only job was
to aggregate the backend API and private RenderSurface contract; consumers now
include those two source-private headers directly.

No new SDK product, service layer, Runtime facade, or backend abstraction is
introduced. S7 is a repository-ownership cleanup only; public C++ names and
runtime behavior remain the S6 model.
