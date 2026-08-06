# View access convergence

This stage narrows all repository-owned access to `Aero::View` private
operations through one source-only gateway:

```text
Markup::XamlReader
Integration::ReloadCoordinator
App::DesktopHost
            \
             -> Runtime::Detail::ViewAccess -> View
```

## Why the reader remains View-bound

Schema descriptors, provider routing and the document cache are process-level
data owned by `Gui`, but a successfully instantiated `XamlDocument` currently
contains View-affine object-writer effects, namescope state, dynamic-resource
subscriptions and deferred template state. Changing `XamlReader` to accept
only `Gui&` before separating those two lifetimes would make documents appear
portable while retaining references to the creating View.

The supported public behavior therefore remains unchanged in this stage:

```cpp
Markup::XamlReader reader(view);
auto document = reader.Load("app:///MainWindow.xaml");
view.SetContent(std::move(document).Value(), size);
```

## Completed boundary

- `View` grants private access to only `Runtime::Detail::ViewAccess` and the
  renderer/composition implementation that directly owns frame execution.
- `XamlReader` no longer has direct friendship with `View`.
- `ReloadCoordinator` no longer calls View-private loader, cache or mount
  operations directly.
- `DesktopHost` no longer depends on View-private type inspection or unmount
  operations.
- The gateway is a source header and is not installed or exported as an SDK
  concept.

`ViewAccess` is deliberately a set of semantic operations rather than getters
for metadata, layout, binding, input, render-tree or template engines. It must
not become a service locator.

## Next ownership step

The next convergence stage can now move immutable loading inputs to `Gui`
without giving each caller separate access to View internals:

```text
Gui
|- frozen schema and metadata
|- XAML provider routes
|- document source cache
|- process-level font/texture providers

View
|- object activation/effect lifetime
|- mounted namescope and resource environment
|- layout, input, animation and render state
```

After the loader is split into a Gui-owned blueprint phase and a View-owned
activation phase, a Gui-bound reader can be introduced without changing the
lifetime guarantees of existing XAML objects.
