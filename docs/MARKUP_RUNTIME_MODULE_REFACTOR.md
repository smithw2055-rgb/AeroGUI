Accepted and implemented through the root runtime/module, explicit load context, and visual content plan slices.
   document resources, and visual content plan itself instead of using the
   writer as long-lived view state.
8. `XamlLoadResult` now contains `XamlVisualContentPlan`. Markup records the
   content edges during loading; `RuntimeHost` then asks
   `Presentation::VisualTreeMount` to attach, resize, and detach that plan.
9. `RuntimeHost::Writer()` and `RuntimeHost::VisualTree()` are removed from
10. Slice-era `RuntimeServices` compatibility APIs are removed. Code and tests
    use `Presentation::MountService` directly.
11. Runtime-oriented tests now live under `tests/runtime` and architecture
    checks reject the removed Markup/Runtime compatibility headers and `.inc`
    files.
The refactor intentionally leaves these larger behavior changes for future
feature slices because they alter theme/style/resource semantics rather than
only composition boundaries:
1. Convert Binding, DynamicResource, Style, and remaining activation adapters
2. Replace the dedicated `XamlTheme` XML parser with standard XAML or compiled
   XAML loading once the theme object model can be represented by normal
   metadata/facets.
