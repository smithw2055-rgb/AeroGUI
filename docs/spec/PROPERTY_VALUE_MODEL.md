# AeroGUI Property Value Model

## Status

This specification defines the authoritative dependency-property value model for
AeroGUI. WPF observable behavior is the contract; Meta, Facet, Style compilers
and Template runtimes are providers of that one property system.

Phase 2 established ranked token storage, Phase 3 moved Style, ThemeStyle and
ControlTemplate onto manager-owned sessions, and Phase 4 made the
`DependencyObject` entry authoritative. Phase 5 removes the transitional
provider bridge and makes full source information available in change events.

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

Origins `1` and `2` are fixed engine-owned identities for local/expression and
animation diagnostics. Manager-owned provider sessions allocate unique origins
from `FirstCanonicalProviderOrigin`; there are no compatibility origins or token
normalization rules. Reusing an exact token replaces that contribution, while
distinct ordinals represent simultaneous setters or triggers.

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

## Property change source diagnostics

`DependencyPropertyChangedEventArgs` retains the legacy `oldSource` and
`newSource` fields and also carries complete `oldSourceInfo` and
`newSourceInfo` snapshots. Handlers can inspect rank, provider token, expression
kind, inheritance, animation, coercion, current-value state and revision without
performing a second lookup.

## Unified effective entry

Phase 4 makes `DependencyObject::EffectiveValueEntry` authoritative for provider
contributions, local values and expressions, inherited and animated values, the
unanimated base value, the coerced effective value and `PropertyValueSourceInfo`.

`EffectiveValueEngine` now retains only scheduling records and the semantic
inheritance-parent graph. It no longer owns value state or lands values through
`SetCurrentValue()`.

## Migration progress

1. Common value-rank, provider-token and source-info types. **Done.**
2. WPF-specific setter and trigger ranks. **Done.**
3. Token-scoped contribution storage and targeted removal. **Done.**
4. Simultaneous trigger contributions through stable session ordinals. **Done.**
5. Engine-wide canonical origin allocation. **Done.**
6. Style, ThemeStyle and Template manager-owned sessions. **Done.**
7. Move source diagnostics into the `DependencyObject` effective entry. **Done.**
8. Move provider, expression and animation storage into that entry. **Done.**
9. Preserve the unanimated base value and coerced effective value. **Done.**
10. Route Binding and DynamicResource through the shared expression state. **Done.**
11. Remove legacy Engine provider APIs and compatibility origins. **Done.**
12. Add complete source snapshots to dependency-property change events. **Done.**
13. Remove public-header type-shaping branches. **Done.**

## Invariants

- `GetValue()` and source diagnostics ultimately read the same entry.
- `ClearValue()` removes only the local literal/expression contribution.
- `SetCurrentValue()` preserves the active source identity.
- Multiple active triggers may contribute to one property simultaneously.
- Provider removal is token- or origin-scoped and deterministic.
- Manager sessions never reuse origins allocated by another session.
- Animation retains the base value beneath the animated value.
- Coercion never destroys the uncoerced base value.
