# Final refactor closure (S18-S24)

This note records the final invariants established by the implementation
refactor. The executable source of truth remains
`cmake/CheckArchitecture.cmake`.

## S18: symbol boundary

- `AeroBase`, `AeroAudio`, `AeroGui`, and `AeroApp` disable
  `WINDOWS_EXPORT_ALL_SYMBOLS` and use hidden default visibility.
- Each product defines only its own export macro while it is built.
- Files under `src` contain no product API macro.
- Every shared Windows product runs `cmake/CheckWindowsExports.cmake` against
  its generated DLL and rejects source-only or retired symbols.

## S19: public opaque closure

- Installed headers contain no `struct Impl` and no `AERO_INTERNAL_*` contract.
- XAML document, reader, provider, and service-provider responsibilities have
  separate headers.
- `RenderDevice` and `RenderTarget` are physically owned by
  `include/Aero/Render`; `IRenderer` remains under `include/Aero/Gui`.

## S20: source vocabulary closure

- No source filename contains `Internal` or `Private`.
- No domain has a `private`/`detail` directory or `Aero::<Domain>::Detail`
  namespace.
- `GuiPrivate`, `ViewDetail`, and `View::Operations` are retired.

## S21: View and state closure

- `ViewState` owns the view-affine component graph and the one
  `ViewRenderer`.
- Source-only heap Pimpl allocations are removed; delayed implementation data
  is direct or constructed in owner-provided inline storage.
- `View` has no operations bridge and no optional component plus duplicate raw
  pointer pair.

## S22: renderer ownership closure

- `ViewRenderer` is the only concrete renderer.
- Render devices are shareable and own no renderer, renderer token, or release
  protocol.
- `UiFrameEncoder` is owned directly by `ViewRenderer`.
- Each `RenderBatch` describes exactly one draw; uploads and target changes are
  performed directly rather than encoded as frame commands. D3D11 and OpenGL
  execute that batch through typed local operations, without a phase/command
  dispatcher.

## S23: native backend and presentation closure

- D3D11/OpenGL command-queue objects are removed; device implementations submit
  batches directly.
- App concrete contexts own swap chains or OpenGL platform contexts.
- `App::RenderContext` alone owns desktop Begin/End/Present state.
- Render targets remain drawable callbacks and contain no presentation flags.

## S24: verification closure

- Architecture checks cover the public whitelist, physical header equality,
  source vocabulary, renderer/device ownership, batch shape, and presentation
  ownership.
- SDK header consumers compile the public Base/Gui/App/Meta/provider/backend
  surfaces independently.
- Static and shared product builds are required; shared builds are inspected
  for the explicit export boundary.
- Offline schema/XAML tools are host executables from a static tools build;
  shared product builds keep them out of the DLL link graph and accept them
  through `AERO_HOST_XAMLC_EXECUTABLE` and
  `AERO_HOST_SCHEMA_GEN_EXECUTABLE` when precompiled assets are requested.
- The framework test and all four conformance executables build against the
  final Gui/App ownership model; retired Integration, command-queue and window
  render-context contracts are not test-only escape hatches.
- Current architecture is documented by `docs/ARCHITECTURE.md`, source
  ownership by `docs/SOURCE_ARCHITECTURE.md`, and desktop presentation by
  `docs/WINDOW_HOSTING.md`.
