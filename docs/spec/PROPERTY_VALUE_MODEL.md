# AeroGUI Property Value Model

## Status

This specification defines the authoritative dependency-property value model for
AeroGUI. WPF observable behavior is the contract; Meta, Facet, Style compilers
and Template runtimes are providers of that one property system.

Phase 2 established ranked token storage. Phase 3 moves the Style, ThemeStyle and
ControlTemplate managers onto manager-owned provider sessions with canonical
origins allocated by the shared `EffectiveValueEngine`.

## Product rule

Every value source converges on one dependency-property resolution model. A
Style, Template, Binding or animation service must not create an independent
current-value table with unrelated precedence or diagnostics.

## Precedence ranks

Base providers are ordered from strongest to weakest:

1. Local value or local expression (`Binding`, `DynamicResource`).
2. Templated-parent trigger.
3. Templated-parent setter.
4. Implicit style for the `Style` property.
5. Style trigger.
6. Template trigger.
7. Style setter.
8. Theme trigger.
9. Theme setter.
10. Inherited value.
11. Metadata default.

Animation and coercion are applied after the winning base provider is selected.
`SetCurrentValue()` changes the current value without replacing the winning
provider identity.

## Provider identity

Each contribution is identified by a stable token:

```cpp
struct PropertyProviderToken {
    PropertyValueRank rank;
    std::uint32_t origin;
    std::uint32_t ordinal;
};
```

- `rank` identifies the WPF precedence layer.
- `origin` identifies one active provider application.
- `ordinal` preserves deterministic declaration order inside that application.

Removing a trigger, template or style removes only contributions owned by its
token or origin. It must never clear an unrelated provider at the same rank.

## Origin allocation

`EffectiveValueEngine` owns one `PropertyProviderOriginAllocator`. All provider
sessions attached to that engine allocate through it, so Style, ThemeStyle and
Template origins cannot collide even when they target the same object/property.

Origins below `FirstCanonicalProviderOrigin` are reserved compatibility values.
Canonical manager-owned sessions allocate from the canonical range.

The remaining compatibility origins are isolated by semantic domain:

```text
ThemeStyle setter
Style setter
Templated-parent setter
Template trigger bridge
Theme trigger bridge
Legacy Style-trigger stack
```

A legacy setter call is normalized to its domain-specific reserved origin before
entering `PropertyProviderSet`; Style, ThemeStyle and Template setters no longer
share one anonymous origin.

## Manager-owned provider sessions

The internal runtime defines three adapters over the canonical contribution API:

```text
StyleProviderSession
  setter rank  = StyleSetter
  trigger rank = StyleTrigger

ThemeStyleProviderSession
  setter rank  = ThemeStyleSetter
  trigger rank = ThemeStyleTrigger

TemplatedParentProviderSession
  setter rank  = TemplatedParentSetter
  trigger rank = TemplatedParentTrigger
```

`StyleManager`, `ThemeStyleManager` and `TemplateManager` own these sessions.
Their existing application loops retain the familiar methods such as
`SetStyleValue`, `SetTemplateValue` and `SetTriggerValue`, but those methods now
emit canonical tokens rather than writing through the old anonymous engine
slots.

Each session tracks contributions per target object and property. It provides:

- stable per-object setter and trigger origins;
- monotonically ordered setter/trigger ordinals;
- exact token replacement for ordinary setters;
- multiple simultaneous trigger contributions;
- property-scoped clearing during reevaluation;
- object-scoped cleanup during template detachment.

The session type is private runtime infrastructure. It is not part of the normal
control-authoring SDK.

## Legacy Style-trigger bridge

The old three-argument `EffectiveValueEngine::SetTriggerValue()` API remains for
historical callers that have not adopted a provider session. Repeated writes use
the reserved `LegacyStyleTriggerOrigin` and are expanded into an ordered
contribution stack so multiple active legacy triggers still coexist.

The corresponding clear removes only that reserved Style-trigger origin. It does
not affect canonical Style sessions, template triggers, theme triggers or
templated-parent triggers.

`StyleManager` and `TemplateManager` no longer depend on this bridge. It can be
removed after repository-wide search confirms that no direct historical caller
remains.

## Effective entry

Conceptually every object/property pair owns one entry:

```text
base provider contributions
local expression state
animation state
coercion state
base value
current/effective value
winning source diagnostics
```

`EffectiveValueEngine` currently owns the provider-side entry while
`DependencyObject` owns the applied value entry. The next major stage merges
those records so `GetValue()` and source diagnostics always read the same state.

## Migration progress

1. Common value-rank, provider-token and source-info types. **Done.**
2. WPF-specific setter and trigger ranks. **Done.**
3. Token-scoped contribution storage and targeted removal. **Done.**
4. Simultaneous legacy Style trigger contributions. **Done.**
5. Engine-wide canonical origin allocation. **Done.**
6. Style, ThemeStyle and Template manager-owned sessions. **Done.**
7. Move source diagnostics into the `DependencyObject` effective entry.
8. Move provider storage into that entry and delete the parallel engine table.
9. Preserve the unanimated/uncoerced base value inside the unified entry.
10. Route Binding and DynamicResource invalidation exclusively through the
    unified entry.

## Invariants

- `GetValue()` and source diagnostics ultimately read the same entry.
- `ClearValue()` removes only the local literal/expression contribution.
- `SetCurrentValue()` preserves the active source identity.
- Multiple active triggers may contribute to one property simultaneously.
- Provider removal is token- or origin-scoped and deterministic.
- Manager sessions never reuse origins allocated by another session.
- Animation retains the base value beneath the animated value.
- Coercion never destroys the uncoerced base value.
