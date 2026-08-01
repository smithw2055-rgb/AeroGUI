> 2026-07 更新：产品级 XAML 内核已统一到 URI/provider、tokenizer、node、
> load session/object writer、资源/样式/模板和 View 的单一路径。
> 当前公共契约见 [XAML_RUNTIME_API.md](XAML_RUNTIME_API.md)，架构与迁移见
> [XAML_RUNTIME_ARCHITECTURE.md](XAML_RUNTIME_ARCHITECTURE.md) 和
> [XAML_MIGRATION.md](XAML_MIGRATION.md)。本文后续内容保留为早期模块拆分
> 背景，不作为最新 API 定义。

Accepted and implemented through the root runtime/module, explicit load context, visual content plan, content writer, provider-resolver cleanup, and final style/theme object-model slices.

Implemented boundaries:

1. Product composition lives at the Aero root through `RuntimeEnvironment` and
   `View`; the catalog and dependency ordering used to freeze modules are
   private implementation details shared with host tools.
2. Markup no longer owns runtime composition, window/runtime service compatibility shims, or duplicate module manifests.
3. Metadata descriptors and Core facets remain the structural source of truth; XAML-only behavior is centralized in one frozen `XamlFacetStore` instead of feature-local adapter registries.
4. Compiled XAML cache identity tracks the metadata schema rather than a second XAML module manifest hash.
5. Loading returns `XamlLoadResult`; runtime hosts keep the root object, name scope, document resources, and visual content plan instead of using the writer as long-lived view state.
6. `XamlLoadResult` contains the authored visual/content edges. Markup records those edges during loading; `View` applies them through the private `GuiContext` attachment transaction.
7. `XamlContentWriter` translates visual `ContentFacet` assignments directly into the current `XamlLoadResult::visualContent`; it owns no staging or mounting state.
8. UI runtime consumes the returned content plan through `GuiContext`; Markup does not expose a parallel tree host or root-only visual load path.
9. `View::Writer()` and `View::VisualTree()` are removed from the public API.
10. Slice-era `RuntimeServices`, `MountService`, and `VisualTreeMount` compatibility APIs are removed. View owns the single attachment path.
11. Runtime-oriented tests now live under `tests/runtime` and architecture checks reject the removed Markup/Runtime compatibility headers and `.inc` files.
12. Binding, DynamicResource, and Style providers resolve effective-value targets through `XamlSchemaContext::ResolvePropertyTarget()` and the sealed metadata domain; Markup no longer exposes a DependencyObject cast bridge.
13. `UiObjectModel` registers the complete Style/Setter/Trigger/Template object model in one step. `Setter.Value` and `Trigger.Value` accept normal `XamlValue` payloads, so object-valued setters, template resources, and trigger setters flow through the same metadata/property validation path as scalar setters.
14. `ResourceDictionary` stores `Core::Value`, so scalar, custom-value, null-object, and object resources share one lookup contract. Local document resources override the optional application/module dictionary supplied through `XamlLoadContext`.
15. Metadata value types are valid XAML value elements. For example, `<Color x:Key="Accent" Value="#FF0067C0"/>` converts through the registered value converter and enters the ordinary resource dictionary without an object wrapper.
16. Built-in `ResourceDictionary`, `ControlTemplate`, `VisualStateGroup`, `VisualState`, and `Setter` objects are registered by the `Aero.Markup` metadata module. `Generic.xaml`, `Light.xaml`, and `Dark.xaml` are loaded by `XamlObjectWriter`; the private theme DOM/parser and `XamlThemeResources.cpp` are removed.
17. Template prototypes are compiled from metadata-created visual objects, ordinary dependency-property local values, content facets, and XAML name scopes. Runtime template instances are recreated through `MetadataRuntime::CreateObject`, not a control-kind switch.
18. Built-in and application XAML now share the same schema resolution, object creation, member assignment, resource lookup, value conversion, and initialization transaction semantics.

19. XAML behavior registration is centralized in a frozen `XamlFacetStore`; `XamlSchemaContext` no longer maintains separate member/type/extension arrays.
20. Resource traversal and implicit keys are supplied by type facets, so the internal `Loader` does not branch on Style or Template concrete types.
21. Markup extensions use explicit Value/Handled/Expression results, and session rollback owns extension side effects.
22. `XamlObjectWriter` is reusable configuration while `XamlLoadSession` has its own header and owns all one-shot mutable state.
23. `AeroMarkupKernel` is a real Core-only target for parsing and base compiled IR; UI integration remains in `AeroMarkup`.
24. Built-in theme generation supports a host `aero-xamlc` and deterministic embedded-source fallback for cross builds.
25. View-owned Style/Template application policy lives in `ViewUiServices`, not in `View::Impl`.

The remaining theme work is feature expansion only: richer template bindings, style composition, transitions, and additional controls must extend these metadata objects and existing UI plans. Do not add another theme DOM, parser, resource wrapper, activation registry, or feature-local property system.
