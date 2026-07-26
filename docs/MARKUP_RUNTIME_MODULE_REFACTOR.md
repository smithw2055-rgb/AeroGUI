Accepted and implemented through the third refactor slice.
7. `XamlObjectWriter` exposes `XamlLoadResult` ownership APIs for full
   document loads. `RuntimeHost` stores the loaded root, document NameScope,
   and document resources itself instead of using the writer as long-lived
   view state.
8. `RuntimeHost::Writer()` and `RuntimeHost::VisualTree()` are removed from
   the public API. Named lookup remains workflow-oriented through
   `RuntimeHost::FindNamedObject()` / `FindNamed<T>()`, with
   `NamedObjectCount()` for diagnostics and samples.
1. Promote the current Markup-side staged visual edges into an explicit
   `VisualContentPlan` field on `XamlLoadResult`, then let
   `Presentation::VisualTreeMount` consume that plan directly.
