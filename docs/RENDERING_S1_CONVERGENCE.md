# Rendering S1 convergence

Baseline: `codex/local-no-samples-tests` at
`74de982fa8dac9605dbf699430db8f3b0b88916d`.

## Result

S1 removes the second dispatch and lifetime system that existed between the
public rendering API and the private GPU command implementation.

Before S1:

```text
RenderDevice
  -> void* state + RenderDeviceFunctions
       -> backend state
            -> GraphicsDevice
            -> DeviceRenderer

RenderSurface
  -> void* state + RenderSurfaceFunctions
       -> native surface state
```

After S1:

```text
RenderDevice
  -> NativeRenderDevice
       -> private Graphics::Device + DeviceRenderer

RenderSurface
  -> NativeRenderTarget
```

`NativeRenderDevice` and `NativeRenderTarget` are source-private interfaces.
They use ordinary C++ virtual dispatch, have one destructor/lifetime contract,
and are never installed as SDK types. The public `RenderDevice` remains the
only device object visible to a View or host. The private `Graphics::Device`
is an implementation component for resource handles and command submission;
it is not a second product device or a separately managed host object.

## Ownership

- A public `RenderDevice` exclusively owns one `NativeRenderDevice`.
- D3D11 and embedded OpenGL targets are independently owned by their
  `RenderSurface` wrappers and retain the public device strongly.
- The remaining OpenGL window implementation supplies one borrowed default
  target from its combined context object. This is an App hosting detail, not a
  second function-table path.
- Device loss is reported by `NativeRenderDevice`; target loss is reported by
  `NativeRenderTarget`. A target failure does not automatically poison a ready
  device.
- The host still owns frame scheduling. S1 adds no queue or render worker.

## Removed concepts

The following source-private migration concepts must not return:

```text
RenderDeviceMode
RenderDeviceFunctions
RenderSurfaceFunctions
DeviceFunctionsFor<T>()
SurfaceFunctionsFor<T>()
void* render backend state gateway
```

## Deliberately retained

S1 does not perform the S2 product and directory migration. The following
remain temporarily so this patch can land as one reviewable rendering stage:

- `Aero::Integration` product packaging;
- the public `RenderSurface` spelling;
- App-owned window and presentation setup;
- private `Graphics::Device`, `SurfaceSession`, and native presenters;
- D3D11/OpenGL backend source locations.

Those retained implementation types no longer form parallel public lifetimes.
The next rendering cleanup can rename `RenderSurface` to `RenderTarget` and move
window resize/present recovery behind the App `RenderContext` without another
device-model rewrite.

## Validation

The architecture gate now rejects the removed manual function tables and
requires both private polymorphic contracts. The conformance probe implements
the same native interfaces as production backends, covering ordinary frame
failure, target loss, device loss, restore, and fatal backend health.
