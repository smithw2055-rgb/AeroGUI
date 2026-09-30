# Public Header Model

## Status

Accepted for the `0.3` SDK convergence line.

This specification defines the physical C++ header boundary. WPF determines
public type names and observable behavior; it does not require AeroGUI to copy
.NET assembly boundaries or create one C++ header per type.

## Product entry points

The supported product umbrellas are:

- `<AeroPCH.hpp>` — single precompiled-header/umbrella for new hosts;
- `<Aero/Gui.hpp>` — retained WPF/XAML authoring surface;
- `<AeroApp/App.hpp>` — `Application::Run()` and optional default desktop lifetime;
- `<Aero/Meta.hpp>` — typed metadata and custom-module authoring
  (`Aero/Module.hpp` is its lightweight composition header).

Embedding uses the installed `RenderDevice`/`RenderTarget` contracts plus the
opt-in `<AeroRender/D3D11.hpp>` and `<AeroRender/OpenGL33.hpp>` factories.
`<AeroRender/BackendCommon.hpp>` owns the shared state-preservation policy so
the two backend headers cannot drift.
Advanced headers under `Aero/Markup`, provider contracts under their owning
domains, and `Input/Platform.hpp` are specialist surfaces. Native
Win32/X11 adapters remain private. These specialist headers are not transitively
included by ordinary WPF-style application code.

## Canonical declaration ownership

Each public type has one declaration owner. The SDK does not retain a parallel
set of old-path forwarding headers or namespace-projection headers. The former
`Aero/App/Launcher.hpp` surface is removed; desktop hosting is private and
configured with the value-type `App::RunOptions` declared by `App.hpp`.

The root WPF spine is owned by:

```text
Aero/DependencyObject.hpp
Aero/Visual.hpp
Aero/UIElement.hpp
Aero/FrameworkElement.hpp
AeroApp/Application.hpp
AeroApp/Window.hpp
Aero/View.hpp
```

The corresponding class declaration is physically present in that header. For
example, `UIElement.hpp` does not forward to `Layout.hpp`, and
`FrameworkElement.hpp` does not forward to a rendering implementation header.
`View` is likewise owned by `<Aero/View.hpp>`; the former
`Integration/View.hpp` forwarding path and `ViewHost` facade are removed.
`Gui::CreateView(options)` is the single integration factory.

Domain values are grouped by stable authoring concepts rather than by internal
subsystems:

```text
Aero/Layout.hpp
Aero/Resources.hpp
Aero/Style.hpp
Aero/DataTemplate.hpp
Aero/HierarchicalDataTemplate.hpp
Aero/Markup/MarkupExtension.hpp
Aero/Media/FreezableCollection.hpp
Aero/Data/Binding.hpp            Binding class; siblings in Data/<Type>.hpp
                                 (NotifyPropertyChanged, MultiBinding, converters)
Aero/Input.hpp                   Value types only; command types in Aero/<Type>.hpp
Aero/Documents.hpp
Aero/Shapes.hpp
Aero/TextFormatting.hpp
Aero/Media/Animation.hpp          Umbrella for Timeline, Storyboard, easing, key frames, and animations.
Aero/Media/Brushes.hpp            Brush plus stops, shaders, and concrete brushes.
Aero/Media/Effects.hpp            Effect plus the concrete effects.
Aero/Media/Images.hpp             ImageSource, BitmapImage, CroppedBitmap, Stretch.
Aero/Media/Fonts.hpp
Aero/Media/Geometries.hpp         Geometry, segments, figures, stream context, and concretes.
                                 ToStreamData is serialize/debug only.
Aero/Media/Transform2D.hpp      Transform plus the 2D concretes.
Aero/Media/Transform3D.hpp      Transform3D plus the 3D concretes.
```

Controls and templates use their WPF namespaces as their physical domains;
`ControlTemplate` is owned by `Controls/ControlTemplate.hpp`, while the root
`DataTemplate` and `Style` types have root declaration owners. This keeps the
existing include graph explicit without recreating compatibility umbrellas.

`Input.hpp` owns input value types (`Key`, `PointerInput`, `InputScope`, …).
Command and navigation objects (`ICommand`, `RoutedCommand`, `KeyBinding`,
`KeyboardNavigation`, `FocusManager`) each have a type-named header under
`Aero/`. `Input.hpp` does not include those command headers, so the UIElement
spine does not compile the command object model.

Installed spine headers keep include-closure deliberate: diagnostics types
(`SourceSpan`, `EffectiveValueSource`, `PropertyProviderSet`, layout/render
stats) live in the single `<Aero/Diagnostics.hpp>` umbrella — there is no
`include/Aero/Diagnostics/` directory. `DependencyProperty.hpp` and
`Resources.hpp` include that umbrella; `HashMap` stays in internal headers;
`DependencyObject.hpp` must not include `Threading.hpp`.
`CheckArchitecture.cmake` budgets public include-closure line counts for
`Controls/Button.hpp`, `Controls/TextBlock.hpp`, and `Controls/Panel.hpp`.

Media is a specialist surface made up of family headers such as
`Media/Brushes.hpp`, `Media/Effects.hpp`, `Media/Fonts.hpp`,
`Media/Geometries.hpp`, `Media/Images.hpp`, `Media/Transform2D.hpp`, and
`Media/Transform3D.hpp`. Each family header owns its base and the concrete types.
WPF-visible formatting values
are owned by `<Aero/TextFormatting.hpp>`; the text provider, shaping and
editing implementation remains private under `src/gui/text`.
Generic `Media.hpp` and `Text/Text.hpp` aggregation headers are not part of
the installed SDK.

`<Aero/Media/Animation.hpp>` is an umbrella only. Family headers own the
clock (`Timeline.hpp`), storyboard chain (`Storyboard.hpp`), easing
(`EasingFunctions.hpp`), key frames (`KeyFrames.hpp`), and from/to animations
(`Animations.hpp`). `EventTrigger` lives in `Triggers.hpp`. Timer and
storyboard-completed triggers stay under `Media/Animation/`. Actions stay in
`StoryboardActions.hpp` and `MediaActions.hpp`. The animation umbrella must
not include those trigger or action headers. From/to animations are
`AnimationBase<T>` and `Animation<T>`; the XAML names (`DoubleAnimation`,
`ColorAnimation`, and the rest) are those specializations. `EasingFunctionBase`
derives `Freezable`, and `KeyFrameBase` derives `Freezable` with template
`KeyFrame<T>` underneath the WPF-named key-frame types.

## Controls headers

`Aero/Controls.hpp` is the broad umbrella. Type-named headers such as
`Controls/Button.hpp`, `Controls/Grid.hpp`, `Controls/Selectors.hpp`, and
`Controls/TextBox.hpp` physically own their declarations. Button leaves live
in `Controls/Buttons.hpp`, and the GridView presenters live in
`Controls/GridViews.hpp`. Foundational family
types such as `Control`, `Panel`, `ContentControl`, `ButtonBase`, and
`ItemsControl` follow the same rule. The SDK contains no parallel `Aero/Gui/*`
paths and no include-only `Text.hpp` compatibility facade. Foundational
family types are owned by `Controls/Primitives/*` with no flat forwarding
headers under `Controls/`.

Trigger ownership follows WPF semantics: `<Aero/Triggers.hpp>` owns Style/Template
triggers, `<Aero/Triggers.hpp>` owns `EventTrigger` (with Style/Template triggers), `Aero/Interactivity/*`
owns Blend behaviors/actions, and `Media/Animation/` owns storyboard and media
actions plus the timer and storyboard-completed triggers.
`<Aero/Media/Animation.hpp>` stays trigger-free.

Property declarations use `AERO_DEPENDENCY_PROPERTY` /
`AERO_ATTACHED_PROPERTY` / `AERO_READONLY_PROPERTY` from
`<Aero/DependencyProperty.hpp>`; hand-written `XxxProperty` refs and
`Nullable::GetValueOr` are retired. Nullable values use explicit
`GetHasValue() ? GetValue() : fallback` at the call site.

## Private implementation placement

There is no installed `Aero/Detail` directory. The following concepts are
private and live under `src/`:

- object-tree and mount transactions;
- layout, input, binding, style and template runtime coordination;
- effective-value provider sessions;
- XAML facets and frozen runtime plans;
- display lists, render commands, GPU resource identifiers and backend state;
- native Win32/X11 window, clipboard and IME implementations;
- built-in metadata bootstrap and runtime safety checks.

A private header should normally be shared by at least two translation units.
A helper used by one translation unit belongs in that `.cpp` file. Private
`Access`, `Fwd`, `Internal`, `Session`, `Store` and `Manager` files are not
created merely to mirror conceptual layers.

## Installation boundary

`cmake/AeroPublicHeaders.cmake` is the explicit SDK whitelist. The physical
`include/Aero` tree must match it exactly. `install(DIRECTORY include/Aero ...)`
is prohibited because moving a private file under `include/` must never
silently publish it.

The architecture check enforces:

- no installed legacy metadata, platform, or detail tree;
- no public `Aero/Detail` headers;
- one canonical declaration owner for each public type;
- the canonical namespace manifest, with no public `using namespace` directives;
- no product-layer `Detail` namespace in installed headers;
- no duplicate direct includes in public headers;
- no retired forwarding/compatibility paths;
- no native Win32/X11 adapter types in the public platform-service contract;
- no manager, mount, display-list or typed runtime attachment leakage through
  WPF authoring headers;
- exact equality between the source public tree and the installation whitelist.

Property setters and WPF lifecycle hooks use direct values: public `SetXxx`,
`ClearXxx`, `Reset` and notification methods return `void`, `ApplyTemplate`
returns `bool`, and measure/arrange/render hooks use `Size`/`void`. `Result<T>`
is reserved for the canonical parsing/conversion `Try*` names, streams,
resources, registration and other boundaries where the caller must observe
failure. Dependency-property mutations (`SetValue`, `SetCurrentValue`,
`ClearValue`, `CoerceValue`) strictly follow WPF-shaped `void` methods;
validation and read-only checks are completed before commit so a rejected
assignment leaves the previous effective value unchanged. `Freezable` freezes
disallow subsequent mutations silently or via assertion.

`void` is the default for fallible-looking conveniences that the caller cannot
meaningfully handle: every property setter overload (including `StringView`
conversion overloads), collection `Add`/`Insert`/`Clear`/`Remove` operations,
and lifecycle hooks (`OnAttached`/`OnDetaching`/`OnLayoutUpdated`). Their
allocation/conversion failures assert via `AERO_ASSERT` and keep the previous
value (best-effort no-op in release). `Result<void>` stays reserved for `Try*`
names, streams/files, module registration, XAML/schema loading, backend
`Create*` factories, and codecs.

Adding a public header is therefore an API decision: it must update the
whitelist and namespace manifest when needed, fit an existing product/domain
model, and pass the public-header consumer build. Header and source file size
are organized by responsibility and dependency. The architecture check
budgets public include-closure line counts for Button, TextBlock, and Panel.
