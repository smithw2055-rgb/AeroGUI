# AeroGUI Public Namespace Model

## Status

This specification defines the public C++ namespace model used while AeroGUI
converges on a WPF-compatible C++ SDK. XAML type names remain independent of
C++ namespaces and continue to use the registered XAML namespace URI plus local
name as their stable identity.

## Principles

1. WPF/XAML observable semantics define the public product model.
2. C++ namespaces group stable semantic domains and prevent a single oversized
   `Aero` namespace.
3. CMake targets describe deployment and linking boundaries, not C++ namespace
   ownership.
4. Meta and Facet are implementation mechanisms. They must not replace familiar
   WPF-facing type names or force application authors to assemble controls from
   raw behavioral facets.
5. Each public type has one canonical C++ qualified name. Compatibility aliases
   are temporary migration boundaries and must not become a second documented
   API.

## Product headers

The public API is organized around three product surfaces and one authoring
extension:

- `<Aero/Gui.hpp>` is the retained-mode WPF/XAML class library. It contains the
  dependency-object spine, routed events, layout values, styles, resources,
  controls, data binding, input, media and shapes.
- `<Aero/App.hpp>` adds the optional default desktop lifetime to
  `Application` and `Window`; `App::RunOptions` configures optional host details.
- `<Aero/Integration.hpp>` exposes renderer, host and native-window integration
  for engines and existing application frameworks.
- `<Aero/Meta.hpp>` and `<Aero/Module.hpp>` layer typed metadata and module
  authoring over the normal GUI class library for custom-control authors.

The matching CMake targets are `Aero::Gui`, `Aero::App`, `Aero::Integration`
and `Aero::Meta`. Compatibility product targets do not form a second supported
SDK.

## Public namespaces

| WPF semantic area | Aero C++ namespace | Examples |
| --- | --- | --- |
| `System.Windows` | `Aero` | `Application`, `Window`, `DependencyObject`, `UIElement`, `FrameworkElement`, `Style`, `ResourceDictionary` |
| `System.Windows.Controls` | `Aero::Controls` | `Button`, `Grid`, `TextBox`, `ItemsControl` |
| `System.Windows.Controls.Primitives` | `Aero::Controls::Primitives` | `ButtonBase`, `Selector`, `RangeBase`, `Thumb` |
| `System.Windows.Data` | `Aero::Data` | `Binding`, `BindingMode`, `IValueConverter` |
| `System.Windows.Media` | `Aero::Media` | `Brush`, `Geometry`, `Transform`, `ImageSource` |
| `System.Windows.Media.Animation` | `Aero::Media::Animation` | `Timeline`, `Storyboard`, `DoubleAnimation` |
| `System.Windows.Input` | `Aero::Input` | `ICommand`, `RoutedCommand`, `KeyBinding` |
| `System.Windows.Documents` | `Aero::Documents` | `Run`, `Span`, `Hyperlink` |
| `System.Windows.Shapes` | `Aero::Shapes` | `Rectangle`, `Ellipse`, `Path` |
| `System.Windows.Markup` | `Aero::Markup` | `MarkupExtension`, `XamlReader` |
| `System.Windows.Threading` | `Aero::Threading` | `Dispatcher`, `DispatcherObject` |
| Aero metadata authoring | `Aero::Meta` | `Context`, `Describe`, `TypeId` |
| Host and renderer integration | `Aero::Integration` | `ViewOptions`, `RenderEndpoint`, native integration APIs |
| Default application framework | `Aero::App` | `RunOptions`, generated `App::Run()` bootstrap |
| Private implementation | `Aero::Detail` or domain `Detail` | property, style, template, XAML and render runtimes |

## Root namespace rule

The root `Aero` namespace is reserved for types corresponding to the WPF
`System.Windows` layer and top-level Aero product entry points. Standard
controls, bindings, media objects, animations and commands do not receive root
aliases.

Canonical examples:

```cpp
Aero::Application
Aero::Window
Aero::DependencyObject
Aero::UIElement
Aero::FrameworkElement
Aero::RoutedEventArgs
Aero::Style
Aero::ResourceDictionary

Aero::Controls::Button
Aero::Data::Binding
Aero::Media::Brush
Aero::Media::Animation::Storyboard
Aero::Input::ICommand
```

The SDK must not expose both `Aero::Button` and `Aero::Controls::Button` as
permanent alternatives.

## XAML mapping

C++ namespace placement does not alter the default XAML surface:

```xml
<Window xmlns="urn:aero">
    <Grid>
        <Button Content="Start" />
    </Grid>
</Window>
```

The XAML schema maps `{urn:aero}Button` to
`Aero::Controls::Button`, `{urn:aero}SolidColorBrush` to
`Aero::Media::SolidColorBrush`, and `{urn:aero}Window` to
`Aero::Window`. Type identity must not be derived from a C++ qualified name.

Third-party controls use their own XAML URI:

```xml
<Window xmlns="urn:aero"
        xmlns:game="urn:mygame:ui">
    <game:AbilityButton />
</Window>
```

Schema freeze must reject two registrations that claim the same XAML URI and
local name.

## Application framework boundary

`Aero::Application` and `Aero::Window` are the WPF-facing XAML objects.
`Application::Run()` owns the ordinary code-first desktop lifetime. The free
`Aero::App::Run()` function is only the generated/XAML bootstrap that loads
`App.xaml` before entering the same private host.

The native window, event loop, View and endpoint composition are private under
`src/app`. Backend and allocator selection use the value-type
`Aero::App::RunOptions`; there is no public launcher, host peer, App service
locator, or duplicate `Aero::App::Application` / `Window` type.

`Aero::Application` owns application resources and startup/shutdown policy only.
It does not become a graphics or platform service locator. Embedded engines may
consume the Gui and Integration products without using the default App product.

Application and Window metadata are owned by the App module. Controls does not
include or register App types, preserving the dependency direction
`Core/UI -> Controls -> App` without duplicate schema registration.

## Migration order

1. Application and Window namespace boundary.
2. `Application::Run()` with a private desktop host and no public launcher.
3. Gui, App, Integration and Meta product targets.
4. App-owned Application/Window metadata.
5. Controls primitives and WPF-aligned public control domains.
6. Data, Media, Animation, Input, Documents and Shapes.
7. Meta authoring API and private Facet projection.
8. Install/export verification against the product headers.
