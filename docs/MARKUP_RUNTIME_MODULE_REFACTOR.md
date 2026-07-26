Accepted and implemented through the root runtime/module, explicit load context, visual content plan, content writer, and provider-resolver cleanup slices.

Implemented boundaries:

1. Runtime/module composition lives at the Aero root through `RuntimeHost`, `ModuleCatalog`, and built-in module registration.
2. Markup no longer owns runtime composition, window/runtime service compatibility shims, or duplicate module manifests.
3. Metadata/facet registration is the single control and extension path; XAML-specific duplicate capability registration was removed.
4. Compiled XAML cache identity tracks the metadata schema rather than a second XAML module manifest hash.
5. Loading returns `XamlLoadResult`; runtime hosts keep the root object, name scope, document resources, and visual content plan instead of using the writer as long-lived view state.
6. `XamlLoadResult` contains `XamlVisualContentPlan`. Markup records content edges during loading; `RuntimeHost` then asks `Presentation::VisualTreeMount` to attach, resize, and detach that plan.
7. `XamlContentWriter` owns visual `ContentFacet` staging into the plan. `XamlVisualTreeHost` remains only as a compatibility/mounting shell.
8. `RuntimeHost::Writer()` and `RuntimeHost::VisualTree()` are removed from the public API.
9. Slice-era `RuntimeServices` compatibility APIs are removed. Code and tests use `Presentation::MountService` directly.
10. Runtime-oriented tests now live under `tests/runtime` and architecture checks reject the removed Markup/Runtime compatibility headers and `.inc` files.
11. Binding, DynamicResource, and Style providers share `XamlDependencyObjectResolver` instead of each provider carrying its own ad-hoc DependencyObject callback contract.

Remaining behavior-level work is intentionally isolated because it changes theme/style/resource semantics rather than only composition boundaries:

1. Replace the dedicated `XamlTheme` XML parser with standard XAML or compiled XAML loading once the theme object model can be represented by normal metadata/facets.
2. Expand Style/Setter support beyond the current deterministic subset if templates, triggers, or object-valued setters are required.
