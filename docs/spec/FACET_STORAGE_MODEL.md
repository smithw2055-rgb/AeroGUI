# AeroGUI Facet Storage Model

> **Scope:** XAML metadata type capabilities (`XamlFacets` / TypeRecord
> masks). This is **not** the deleted `Core::Facet` / `ElementHost` element
> bag. Engine and element kernel storage is described in
> `docs/SOURCE_ARCHITECTURE.md`.

## Status

This specification defines the internal XAML type-capability storage and
lookup rules used by the XAML runtime. These capabilities are implementation
hooks for markup; they are not a second public type system and do not alter
WPF/XAML type identity. They must not be confused with the retired
`Aero::Core::Facet` ECS matrix.

## Canonical storage

The runtime stores one compact record for each metadata `TypeId`. A record has
optional, narrowly typed slots for:

- lifecycle;
- name scope;
- resource scope;
- deferred visual content;
- implicit resource key;
- dependency-property target resolution;
- markup-extension value provider.

A type without a capability leaves that slot empty. The store therefore has one
vector and, after freeze, one `TypeId -> index` table rather than a separate
container and index for every capability.

`XamlTypeFacet` is only a compatibility registration DTO. Registration copies
its populated capabilities into the corresponding slots of the per-type
record. The aggregate DTO is never retained, queried, or frozen as a second
runtime representation.

## Atomic registration

Aggregate registration validates the complete request before it changes the
store. Invalid capabilities and overlaps with existing slots fail without
modifying an existing record. Allocation of a new per-type record happens once;
only after that allocation succeeds are its validated slots populated.

Single-capability registration uses the same per-type record and rejects an
already populated slot. There is no aggregate/narrow duplication and no
multi-container rollback protocol.

## Inheritance policies

Each facet declares one lookup policy:

- `ExactOnly`: only the concrete metadata type may provide the capability.
- `NearestBase`: use the nearest registration on the semantic Meta base chain.
- `ComposeBaseToDerived`: collect all registrations from the semantic base type
  to the concrete type.

Markup-extension providers are `ExactOnly`. Name, resource, deferred-content,
implicit-key, and property-target facets use `NearestBase`. Lifecycle facets use
`ComposeBaseToDerived`.

## Lifecycle order

For a concrete type with lifecycle facets on multiple semantic base types:

1. `BeginInit` runs base to derived.
2. `EndInit` runs derived to base.
3. `AbortInit` runs derived to base.

The reverse close/abort order preserves nested initialization ownership. A
callback failure stops the current phase and is returned to ObjectWriter, whose
transaction cleanup invokes abort processing.

## Freeze and lookup

Before freeze, exact lookup is linear so registration remains simple. Schema
freeze builds one index for all per-type records. Exact lookup then uses that
index; inherited lookup walks the semantic Meta base chain and inspects the
requested optional slot at each type.

## Invariants

- A metadata type has one canonical Facet record.
- A capability has at most one slot on that record.
- Aggregate and narrow copies of the same capability never coexist.
- C++ inheritance does not control Facet lookup; the Meta semantic base graph
  does.
- Registration failure leaves existing records unchanged.
- Markup extensions never inherit a provider accidentally.
- Lifecycle composition order is deterministic.
- Facet storage is private runtime implementation and is not exposed through
  the normal control-authoring SDK.
