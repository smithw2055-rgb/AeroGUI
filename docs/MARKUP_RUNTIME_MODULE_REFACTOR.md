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
14. Metadata value types are valid XAML value elements. For example, `<Color x:Key="Accent" Value="#FF0067C0"/>` converts through the registered value converter and enters the ordinary resource dictionary without an object wrapper.
15. Built-in `ResourceDictionary`, `ControlTemplate`, `VisualStateGroup`, `VisualState`, and `Setter` objects are registered by the `Aero.Markup` metadata module. `Generic.xaml`, `Light.xaml`, and `Dark.xaml` are loaded by `XamlObjectWriter`; the private theme DOM/parser and `XamlThemeResources.cpp` are removed.
16. Template prototypes are compiled from metadata-created visual objects, ordinary dependency-property local values, content facets, and XAML name scopes. Runtime template instances are recreated through `MetadataRuntime::CreateObject`, not a control-kind switch.
17. Built-in and application XAML now share the same schema resolution, object creation, member assignment, resource lookup, value conversion, and initialization transaction semantics.

The remaining theme work is feature expansion only: richer template bindings, style composition, transitions, and additional controls must extend these metadata objects and existing Presentation plans. Do not add another theme DOM, parser, resource wrapper, activation registry, or feature-local property system.
