# AeroGUI Property Value Model

## Status

This specification defines the authoritative dependency-property value model for
AeroGUI. It is implemented incrementally after the public WPF namespace surface
has been established.

## Product rule

WPF observable semantics are the contract. Meta and Facet describe types and
behavior, but they do not create an alternative property system. Every value
source must converge on one `DependencyObject` property entry.

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

`origin` identifies the Style, Template, Trigger, expression, animation or
other provider instance. `ordinal` preserves declaration order within one
origin. Removing a trigger or template removes only contributions bearing its
origin token; it must not clear an entire precedence layer.

Origins below `FirstCanonicalProviderOrigin` are reserved for transitional
runtime adapters. New Style, Template and Trigger compilers allocate stable
origins from the canonical range.

### Legacy Style trigger bridge

The existing `StyleManager` still calls the old three-argument
`SetTriggerValue()` API once for each active trigger setter. That API maps to the
reserved token:

```text
rank    = StyleTrigger
origin  = LegacyStyleTriggerOrigin
ordinal = 0
```

`PropertyProviderSet` treats repeated writes of this reserved token as an
ordered compatibility contribution stack. The first setter keeps ordinal 0;
subsequent setters receive ordinal 1, 2, and so on. Consequently multiple active
Style triggers may contribute to one property and later declarations win without
collapsing the rank to one mutable slot.

The corresponding legacy clear removes only contributions with the same
`StyleTrigger` rank and reserved origin. It does not remove Template triggers,
Theme triggers, templated-parent triggers, or canonical token-aware Style
providers. This bridge is removed after `StyleManager` emits explicit tokens.

This compatibility bridge is active and covered by the Module SDK consumer: it
writes two Style trigger contributions to one provider set, verifies the second
contribution wins, and verifies the reserved origin is removed as one unit. The
remaining explicit-token migration is structural cleanup rather than a blocker
for simultaneous active Style triggers.

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

`EffectiveValueEngine` is an internal service operating on these entries. It
must not retain a second authoritative object/property table.

## Migration steps

1. Introduce common value-rank, provider-token and source-info types. **Done.**
2. Replace the single generic Trigger rank with WPF-specific trigger ranks.
   **Done in the provider model.**
3. Give every provider contribution a token and targeted removal operation.
   **Done in EffectiveValueEngine.**
4. Preserve simultaneous legacy Style trigger contributions during migration.
   **Done through the reserved compatibility origin.**
5. Emit explicit stable Style/Trigger/Setter tokens directly from the sealed
   Style plan and remove the reserved bridge.
6. Move source diagnostics into the `DependencyObject` effective entry.
7. Move the remaining provider table into the effective entry and delete the
   parallel engine-owned state.
8. Route inheritance, Binding, DynamicResource, Style, Template and Animation
   exclusively through the unified entry.

## Invariants

- `GetValue()` and source diagnostics read the same entry.
- `ClearValue()` removes only the local literal/expression contribution.
- `SetCurrentValue()` preserves the active source identity.
- Multiple active triggers may contribute to the same property simultaneously.
- Provider removal is token-scoped and deterministic.
- Animation retains the base value beneath the animated value.
- Coercion never destroys the uncoerced base value.
