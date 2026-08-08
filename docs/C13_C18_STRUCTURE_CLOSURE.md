# C13-C18 structure closure

This pass finishes the simplification work after C8-C12 without introducing new
Manager/Service/Runtime product layers.

## C13 - closure correctness

- fixes the two false-positive architecture gates introduced by C8-C12;
- fixes the stale `Graphics::ISurfaceBackend` OpenGL spelling;
- retires the remaining `Aero::Runtime::Detail` source namespace;
- DataTemplate trigger runtime state now belongs to `Aero::Controls::Detail`;
- View-local viewport helpers remain under `Aero::ViewDetail`.

## C14 - private render core

`Graphics::Device` is retained only as a source-private resource/command core
under `src/render`. It is not a second SDK RenderDevice and is not installed.
The direct native backends built in-tree are D3D11 and OpenGL 3.3; `Null` is a
validation backend and `Sokol` is an optional host-supplied bridge. Private
`GraphicsBackendKind` and `ShaderLanguage` values may therefore describe the
host bridge's underlying capability family without advertising another installed
SDK backend or product layer.

The final FrameEncoder migration aliases are removed from the implementation;
`BatchComposer`, `RenderBatch`, and `FrameTarget` are the real low-level names.

## C15 - OpenGL window target lifetime

WGL/GLX context creation remains physically coupled to the OpenGL device, which
matches the native APIs. Presentation lifecycle no longer lives in the device:
`OpenGL33WindowTargetState` owns the frame serial and explicit lost state, while
the device owns context/functions/GPU resources. The target is therefore a real
lifetime participant instead of a forwarding-only proxy.

## C16 - View composition root

View remains a single composition root rather than being split into new service
objects. Existing Binding/Style/Template/Animation/Input engines own their
algorithms; View coordinates them. Generic `Runtime::Detail` spelling is gone.
Deep trigger coordination stays inside View where it is presentation-affine;
extracting another TriggerRuntime/Service layer is intentionally rejected.

## C17 - header/include visibility

Public WPF leaf headers and the six family aggregators remain source-compatible.
A family header may remain the declaration owner where moving a foundational
class would create include cycles; type-named forwarding headers are convenience
entry points until a leaf becomes an independent declaration owner.

High-fan-out Controls/Markup private contracts name the narrow GUI seams they
actually consume. The legacy source-only aggregation headers remain compatibility
entry points until every translation unit has been migrated; they are not SDK
products and are no longer used as architecture boundaries.

## C18 - final product boundary

The final installed product graph stays intentionally small:

```text
Aero::Base
   |
   +-- Aero::Gui
   |      |
   |      +-- Aero::Meta facade
   |
   +-- Aero::Audio (optional)

Aero::App -> Aero::Gui
```

`Aero::Meta` is a Gui facade. `Aero::Render` is a specialist C++ namespace in
AeroGui, not a separate binary. This is intentionally simpler than creating an
`AeroRender` wrapper around a renderer whose immutable RenderFrame is produced
by the UI tree.

All AeroGui source ownership now lives in `AeroGuiTargets.cmake`; composition
and rendering are no longer spread across target-extension files.
