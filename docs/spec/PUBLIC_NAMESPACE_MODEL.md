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
- `<Aero/App.hpp>` adds `Application`, `Window`, `App::Launcher` and optional App
  services for a default desktop application lifetime.
- `<Aero/Integration.hpp>` exposes renderer, host and native-window integration
  for engines and existing application frameworks.
- `<Aero/ModuleSdk.hpp>` layers `Meta` authoring and module registration over the
  normal GUI class library for custom-control authors.

The current CMake targets still use the transitional Runtime, ModuleSdk and
IntegrationSdk names. Renaming and packaging them as Gui, App and Integration
must happen atomically with the install/export configuration so a public header
never promises a link target that does not provide its implementation.

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
| Host and renderer integration | `Aero::Integration` | `RenderEndpoint`, `ViewHost`, native integration APIs |
| Default application framework | `Aero::App` | `Launcher`, `LaunchOptions`, `Services` |
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

`Aero::Application` and `Aero::Window` are WPF-facing XAML objects.
`Aero::App::Launcher` owns the optional default application lifetime: platform
window creation, event pumping, endpoint selection and View orchestration.
Embedded engines may consume the GUI and Integration surfaces without using
Launcher.

Optional platform services belong to `Aero::App::Services`, which is owned by
Launcher. `Aero::Application` does not own audio, graphics, native-window or
other platform device services. Constructing an Application must therefore
remain valid in headless and embedded runtimes.

During the first migration phase:

- `Aero::Application` and `Aero::Window` are the real type definitions.
- `Aero::App::Application` and `Aero::App::Window` are compatibility aliases.
- `Aero::App::Launcher` is the canonical application entry point and composes
  the existing low-level `ApplicationHost` implementation.
- Launcher owns `Aero::App::Services`; audio is created lazily through that
  service boundary rather than through Application.
- Application and Window metadata are owned by the App module. Controls does not
  include or register App types, so the dependency direction remains
  `Core/UI -> Controls -> App` without duplicate schema registration.
- New code and documentation use the canonical names.

The compatibility names may be removed before the first stable ABI release.

## Migration order

1. Application and Window namespace boundary.
2. Launcher composition and App services.
3. Establish the Gui, App, Integration and Module authoring product headers.
4. Split metadata bootstrap and move Application metadata ownership to App.
5. Controls primitives and WPF-aligned public control domains.
6. Data, Media, Animation, Input, Documents and Shapes.
7. Meta authoring API and private Facet projection.
8. Align CMake install/export targets with the public product headers.
9. Remove compatibility aliases before stable SDK release.
