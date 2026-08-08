# S13 WPF public API closure

This stage tightens the WPF-facing SDK surface without adding another runtime,
service, access, contract, or rendering layer.

## Completed in this patch

- `TextAlignment` uses WPF names: `Left`, `Center`, `Right`, `Justify`.
- Text layout treats `Left` and `Right` as absolute alignment values.
- `GridLength` and `GridUnitType` are canonical `Aero` types, matching WPF's
  `System.Windows` ownership.
- `GridLength` exposes `GetValue()`, `GetGridUnitType()`, `GetIsAbsolute()`,
  `GetIsAuto()`, and `GetIsStar()`.
- AXB2 compiled-XAML serialization uses the canonical `Aero::GridLength` type.
- `Application::SetMainWindowBorrowed()` is removed from the ordinary SDK
  surface; the desktop host reaches it through the source-private
  `Application::Impl` bridge.
- `AERO_DECLARE_TYPE_NAMED` exposes owner-aware aliases named
  `DependencyProperty`, `ReadOnlyDependencyProperty`, `AttachedProperty`, and
  `RoutedEvent`. Existing `Members::*` spellings remain only until the S14
  declaration-owner migration.

## Next closure stages

- nullable `ToggleButton::IsChecked` and removal of `IsIndeterminateProperty`;
- S14 declaration ownership out of `Core.hpp`, `Items.hpp`, `Panels.hpp`, and
  `Primitives.hpp`;
- S15 Noesis-style `View` input convenience APIs;
- S16 `Renderer -> RenderBatch -> RenderDevice` plus desktop `RenderContext`.

The rule remains: WPF names and semantics at the SDK boundary, Noesis-style C++
method conventions, and no WPF implementation-layer cloning.
