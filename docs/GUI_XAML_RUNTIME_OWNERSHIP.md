# Gui-owned XAML runtime

The final Gui/XAML ownership model keeps immutable/shared loading state at Gui
scope and binds presentation-instance effects only when a document is mounted.

## Gui ownership

`Gui` owns the thread dispatcher, frozen `GuiSchema`, shared `DocumentCache`,
canonical `XamlProviderRegistry`, default embedded/file providers, and one
source-private `Markup::XamlRuntime`.

`Markup::XamlReader` is constructed from `Gui&`:

```cpp
Aero::Gui gui;
gui.Initialize();
Aero::Markup::XamlReader reader(gui);
auto document = reader.Load("app:///MainView.xaml").Value();
```

This is the advanced document-oriented path used by parsing, compiled XAML,
hot reload, and tooling. Ordinary engine integration uses
`gui.LoadXaml<T>(uri)` or `gui.LoadComponent(object, uri)` and passes the
returned root to `CreateView(root)`; Gui retains the complete pending document
until that mount occurs.

Loading and compiled replay create an unmounted object graph using Gui-owned
metadata and the Gui dispatcher. No View is required for schema lookup, provider
routing, source revision handling or document-cache access.

## View-affine activation

Binding, MultiBinding and DynamicResource records are emitted as deferred
effects. When `View::SetContent` or `XamlReader::Mount` commits the document, the
View injects its current dependency-value, binding, resource and lifetime
services before the effect plan is committed. ControlTemplate and DataTemplate
instances likewise receive the binding engine of the View that instantiates
them rather than capturing a View during XAML loading.

```text
Gui
  dispatcher
  schema + providers
  document cache
  XamlRuntime
       |
       | load / parse / compiled replay
       v
unmounted XamlDocument
  object graph + deferred effects
       |
       | SetContent / Mount
       v
View
  dependency values + binding engine
  resources + namescope
  layout/input/animation/render state
```

This makes a loaded document a Gui/thread-level construction result while
keeping mutable presentation subscriptions and mounted state View-affine.
`ViewAccess` and the old View-private Load/Parse/Compiled gateway are no longer
part of the source architecture.
