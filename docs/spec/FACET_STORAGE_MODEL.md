# AeroGUI Facet Storage Model

## Status

This specification defines the internal Facet storage and lookup rules used by
the XAML runtime. Facets are implementation capabilities; they are not a second
public type system and do not alter WPF/XAML type identity.

## Canonical storage

The runtime stores one narrow collection per capability:

- lifecycle;
- name scope;
- resource scope;
- deferred visual content;
- implicit resource key;
- dependency-property target resolution;
- markup-extension value provider.

`XamlTypeFacet` is only a compatibility registration DTO. Registration projects
it atomically into the applicable narrow records. The aggregate DTO is never
retained, indexed, queried, or frozen as runtime state.

## Atomic registration

Projecting a compatibility DTO is a transaction. Each narrow collection is
checkpointed before registration. If any capability is invalid, duplicated, or
cannot be allocated, every record added by that projection is rolled back.

Single-capability registration should use the narrow `SchemaAccess` operation.
Multi-capability registration may continue using the compatibility DTO until a
typed registration-session DSL replaces it; this does not recreate aggregate
runtime storage.

## Inheritance policies

Each facet declares one lookup policy:

- `ExactOnly`: only the concrete metadata type may provide the capability.
- `NearestBase`: use the nearest registration on the semantic Meta base chain.
- `ComposeBaseToDerived`: collect all registrations from the semantic base type
  to the concrete type.

Markup-extension providers are `ExactOnly`. Name, resource, deferred-content,
implicit-key, and property-target facets currently use `NearestBase`.
Lifecycle facets use `ComposeBaseToDerived`.

## Lifecycle order

For a concrete type with lifecycle facets on multiple semantic base types:

1. `BeginInit` runs base to derived.
2. `EndInit` runs derived to base.
3. `AbortInit` runs derived to base.

The reverse close/abort order preserves nested initialization ownership. A
callback failure stops the current phase and is returned to ObjectWriter, whose
existing transaction cleanup invokes abort processing.

## Freeze and lookup

Schema freeze builds a `TypeId -> index` table for every narrow collection.
Before freeze, exact lookup is linear so registration remains simple. After
freeze, exact lookup uses the index; inherited lookup walks the semantic Meta
base chain and performs indexed exact lookup at each step.

## Invariants

- A capability has one canonical runtime record.
- Aggregate and narrow copies of the same capability never coexist.
- C++ inheritance does not control facet lookup; the Meta semantic base graph
  does.
- Registration failure leaves the store unchanged.
- Markup extensions never inherit a provider accidentally.
- Lifecycle composition order is deterministic.
- Facet stores are private runtime implementation and are not exposed through
  the normal control-authoring SDK.
