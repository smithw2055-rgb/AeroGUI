Accepted and implemented through the root runtime/module, explicit load context, visual content plan, content writer, provider-resolver cleanup, and final style/theme object-model slices.

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
12. `XamlStyleExtension` now exposes a complete Style/Setter/Trigger object-model entry point. `Setter.Value` and `Trigger.Value` accept normal `XamlValue` payloads, so object-valued setters, template resources, and trigger setters flow through the same metadata/property validation path as scalar setters.
13. `ResourceDictionary` stores `Core::Value`, so scalar, custom-value, null-object, and object resources share one lookup contract. Local document resources override the optional application/module dictionary supplied through `XamlLoadContext`.
14. The built-in theme parser and DTOs are private implementation details. They are compiled as a normal translation unit; public `ThemeResourceDictionaryObject` and `.cpp` inclusion shortcuts are prohibited by architecture checks.
15. Built-in palette XAML (`Light.xaml` and `Dark.xaml`) is loaded by `XamlObjectWriter` through private metadata-registered `ResourceDictionary` and `Color` objects. `x:Key`, resource scopes, member conversion, and duplicate-key validation are no longer implemented by the theme parser.

Remaining theme work must remove the private bootstrap parser incrementally:

1. model `ControlTemplate`, visual states, and template content as metadata-created objects;
2. load `Generic.xaml` through `XamlObjectWriter` and the same `ResourceDictionary/Core::Value` pipeline already used by palette XAML;
3. delete the remaining `ThemeXamlDocument` parser and then `XamlThemeResources.cpp` once template materialization no longer needs DTOs.

Migration invariant: `XamlObjectWriter`, metadata facets, and `Core::Value` remain the only object construction, member assignment, and resource-value pipeline. Theme code may adapt the resulting object graph, but must not introduce a parallel parser, writer, property system, or resource wrapper.
