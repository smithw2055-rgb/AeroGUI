# Gui provider ownership convergence

S2B makes `Gui` the single owner of process-level XAML, texture and font
provider configuration.

## Public model

Hosts register providers before initialization:

```cpp
Aero::Gui gui;
gui.AddXamlProvider(xamlProvider, "app");
gui.AddTextureProvider(textureProvider);
gui.AddFontProvider(fontProvider);
gui.Initialize();

auto view = gui.CreateView(options).Value();
```

`Integration::ViewOptions` now contains only per-View platform, text and
behavior options. It no longer contains provider routes or provider override
pointers.

## Runtime ownership

`Gui::Impl` owns:

- the frozen schema;
- the shared compiled document cache;
- the canonical XAML provider registry;
- the process-level texture provider;
- the process-level font provider.

Each View retains its Gui state. Its local XAML registry links to the Gui
registry as a parent and contains only View-owned fallback providers such as the
embedded built-in source and file provider. Resolution preserves route
specificity across the two registries: a specific View fallback can beat a
catch-all Gui route, while a Gui route wins over a View route of equal
specificity.

No provider route strings are copied during `Gui::CreateView()`.

The View constructor now receives only the retained Gui state. It derives its
schema, document cache and provider registry from that state instead of
accepting parallel constructor arguments.

## Removed paths

The following configuration paths no longer exist:

- `ViewOptions::xamlProviders`;
- `ViewOptions::textureProvider`;
- `ViewOptions::fontProvider`;
- `XamlProviderRoute`;
- the provider merge and override pass in `Gui::CreateView()`.

This prevents two Views created from the same Gui from silently using different
resource-provider domains and removes route allocation and copying from every
View creation.
