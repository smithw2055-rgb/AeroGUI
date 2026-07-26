Accepted and implemented through the second refactor slice.
## Implemented migration slices

The current codebase implements these changes:

1. `RuntimeHost`, `ModuleRegistration`, and `ModuleCatalog` are root `Aero::*`
   APIs.
2. Runtime implementation files live directly under `src/` and are compiled by
   the `AeroRuntime` target.
3. `XamlModuleManifest`, `XamlModuleCatalog`, `configureXaml()`,
   `xamlSchemaVersion`, and `moduleManifestHash` are removed.
4. `QueuedRenderBackend` is now a Presentation service instead of a Runtime or
   Markup type.
5. `XamlVisualTreeHost` keeps Markup-time content staging only; Presentation
   owns logical, visual, layout, render, resize, and detach sequencing through
   `Presentation::VisualTreeMount`.
6. XAML activation now uses an explicit `XamlLoadContext` passed to
   `XamlObjectWriter`; the previous thread-local active activation state is
   removed.

The next slices should complete:
1. Make `XamlObjectWriter` return a first-class `XamlLoadResult` that owns the
   root, NameScope, resources, and visual content plan, so RuntimeHost no longer
   exposes long-lived writer/visual-tree internals.
2. Convert Binding, DynamicResource, Style, and remaining activation adapters
   into descriptor/facet-driven registrations where practical.
