# Facet Pattern — Implementation Progress

> **Historical / not the current contract.**
> Phases A–E below record a Facet/Access migration that has been **reverted**.
> The kernel no longer has `Core::Facet`, `GetFacet`, `ElementFacet`,
> `ElementHost`, or panel layout facets. Current architecture:
> `docs/SOURCE_ARCHITECTURE.md`.

Status: **Superseded.** The Facet bags and Access facades were removed in favor
of a WPF + View-hub kernel.

## Objective
Realize the WPF Facet Pattern per `docs/WPF_Facet_Pattern_Architecture_Whitepaper.md` in AeroGUI-R:
- Eliminate the old `Access` pattern.
- Replace `void*` `ElementHost` fields with typed `Core::Facet` subclasses, resolved through a unified `Core::GetFacet<T>`.
- Per-element facet bag with activated lifecycle (`OnAttached`/`OnDetached`) + subclassable `LayoutFacet`.
- Full whitepaper form, with the static `*Facet` facade headers retired gradually.

## Completed work

### Phase A — View-affine facet matrix (`ElementHost`)
- `src/gui/core/State.hpp`: `ElementHost` is now a pure non-owning registry
  `Core::Facet* facets_[FacetCount]`, with `GetFacet<T>()` / `SetFacet<T>()`.
- Unified free functions `Core::GetFacet<T>(const UIElement& / Visual& / ContentElement& / Freezable& / DependencyObject&)` defined in `State.hpp`.
- Removed 7 engine-specific `GetFacet` specializations from `EventRouter.hpp`.
- `src/gui/View.cpp` (`CreateUiEngines`) registers engines via `SetFacet`.
- `src/gui/core/ElementTree.cpp` engine accessors use `host->GetFacet<...>()`.

### Phase B — Typed service facets
- `src/gui/core/facets/ServiceFacets.hpp`: `TemplateEngineFacet`, `VisualStateServiceFacet`,
  `TextLayoutServiceFacet`, `ControlBehaviorFacet`, `MeshResourceFacet`, `NameScopeFacet`
  (each `: public Core::Facet`, with `FacetTrait` specializations at `FacetId` 17–22).
- `FacetId` in `src/gui/core/Facet.hpp` extended to `Count = 23`.
- `src/gui/View.cpp` `ViewState` gained 6 facet members + registration; all `void*` reads/writes
  (`elementHost.*`, `state_->elementHost.*`, `data.elementHost.*`, `Path.cpp:1043`) migrated.
- `ElementHost` no longer carries `void*` fields, `nameScopeContext`, or `findName`.

### Phase C — Per-element facet bag
- `include/Aero/UIElement.hpp`: `static constexpr ElementFacetCount = 16U`,
  `Core::Facet* elementFacets_[16] = {}`, plus `ElementFacet<T>()` (public read) and
  `SetElementFacet<T>()` (protected, subclass-only registration), and public
  `AttachElementFacets()` / `DetachElementFacets()`.
- Definitions live in `State.hpp`; lifecycle driven from `ElementTree::AttachElement`
  / `DetachNode`.

### Phase D — Subclassable layout facets
- `src/gui/core/facets/LayoutFacet.hpp`: abstract `LayoutFacet` (constructible; stores `owner_`),
  holds the `*LayoutCalculator` kernels.
- `src/gui/core/facets/LayoutFacets.hpp`: `StackLayoutFacet`, `GridLayoutFacet`, `DockLayoutFacet`,
  `CanvasLayoutFacet`, `WrapLayoutFacet` (each `: public LayoutFacet`) + `DefaultLayoutFacet`.

### Phase E — Layout dispatch + deep panel migration
- `UIElement` ctor creates a `DefaultLayoutFacet` and registers it; dtor frees it.
- `LayoutEngine::MeasureElement` / `ArrangeElement` (`src/gui/controls/Layout.cpp`) now dispatch
  through `element.ElementFacet<Core::LayoutFacet>()->Measure/Arrange(...)`, with a safe static
  fallback when no facet is present.
- All six panels register their concrete layout facet in their constructors
  (`src/gui/controls/Panels.cpp`): `StackLayoutFacet`, `DockLayoutFacet`, `WrapLayoutFacet`,
  `GridLayoutFacet` (×`UniformGrid` and `Grid`), `CanvasLayoutFacet`. The three formerly-inline
  panel ctors (`DockPanel`/`WrapPanel`/`UniformGrid`) were moved out of
  `include/Aero/Controls/StackPanel.hpp` into `Panels.cpp`.

#### E.1 — Per-element layout state relocated into `LayoutFacet`
- `include/Aero/UIElement.hpp`: removed the 14 layout state fields
  (`desiredSize_`, `untransformedDesiredSize_`, `renderSize_`, `previousMeasureConstraint_`,
  `layoutSlot_`, `layoutClip_`, `layoutRevision_`, `layoutAttached_`, `measureValid_`,
  `arrangeValid_`, `measureQueued_`, `arrangeQueued_`, `measuring_`, `arranging_`). The 14 layout
  getters are now out-of-line declarations.
- `src/gui/core/facets/LayoutFacet.hpp`: those 14 fields now live in the facet (const/mutable
  getters). The static state accessors (`LayoutAttached`, `MeasureValid`, `ArrangeValid`,
  `MeasureQueued`, `ArrangeQueued`, `Measuring`, `Arranging`, `DesiredSize`, `RenderSize`,
  `UntransformedDesiredSize`, `PreviousMeasureConstraint`, `LayoutSlot`, `LayoutClip`,
  `LayoutRevision`) are now **reference-returning** declarations.
- `src/gui/controls/Layout.cpp`: added the 14 static forwarder definitions, each resolving
  `element.ElementFacet<LayoutFacet>()-><field>` (with a non-null `AERO_ASSERT`). Also rerouted the
  `UIElement` ctor/dtorder + tree-attach state writes through these forwarders.
- `src/gui/core/UIElement.cpp`: 14 getters now forward to `ElementFacet<Core::LayoutFacet>()`; the
  handful of direct field accesses (child/parent `layoutAttached_`, `measureValid_`, `arrangeValid_`)
  were rerouted through the static forwarders so nothing reads the removed `UIElement` members.

#### E.2 — Panel layout algorithms moved into their facets
- `src/gui/controls/Panels.cpp`: removed the six panels'
  `MeasureOverride`/`ArrangeOverride` definitions and added out-of-line
  `StackLayoutFacet`/`DockLayoutFacet`/`WrapLayoutFacet`/`CanvasLayoutFacet`/`GridLayoutFacet`
  `Measure`/`Arrange` bodies, porting each algorithm verbatim (qualified through
  `static_cast<Panel*>(GetOwner())->...`). `DefaultLayoutFacet` keeps delegating to the owner's
  remaining `MeasureOverride` (Border/ContentControl/Decorator/Viewbox/Popup/Expander/TabPanel/
  ToolBarPanel/VirtualizingStackPanel/TextBlock/Shape/Image/Scroll).
- `GridLayoutFacet` is shared by `Grid` **and** `UniformGrid` (no RTTI, so it dispatches on
  `GetOwner()->RuntimeType() == Grid::StaticTypeId()`): the Grid branch ports the grid track math;
  the else branch ports the uniform-grid math. `Grid` and `UniformGrid` both declare
  `friend class ::Aero::Core::GridLayoutFacet;` (`include/Aero/Controls/Grid.hpp`,
  `include/Aero/Controls/StackPanel.hpp`) so the facet can reach their private members.
- `include/Aero/Controls/StackPanel.hpp`, `Grid.hpp`: removed the panels' now-dead
  `MeasureOverride`/`ArrangeOverride` declarations.

#### E.3 — Static `*Facet` facade retained as a thin forwarder layer
- The static `LayoutFacet` accessors are kept (not deleted) and forward to the per-element facet.
  This keeps call sites unchanged and avoids a sweeping migration of every `LayoutFacet::X(e)`
  caller; the facade header effectively becomes a thin shim over `ElementFacet<T>()`.

## Key design decisions
- `ElementHost` is a pure non-owning registry; engines + service facets are owned by `ViewState`
  members. The element-level facet bag lives on `UIElement`.
- `FacetTrait<T>` primary template is empty; each concrete type must specialize `Id`/`Type`.
- `Core::Facet` base provides `virtual ~Facet()`, `OnAttached(UIElement*)`, `OnDetached()`, and
  `owner_` (protected). `LayoutFacet` ctor stores `owner_` but does **not** call `OnAttached` —
  the lifecycle hook fires only from `ElementTree` attach/detach (avoids double-invoke on a
  partially-constructed derived panel).
- `ElementFacet<T>()` is public (needed by `LayoutEngine`); `SetElementFacet<T>()` is protected so
  only `UIElement` and its subclasses (panels) can register.
- Current facet `Measure`/`Arrange` implementations **contain** the panel layout algorithms directly
  (ported from the former `MeasureOverride`/`ArrangeOverride`); runtime behavior is preserved. The
  `*LayoutCalculator` kernels remain available as an alternative compute path but are not required
  for the swap.

## Build / verification status
- Project is MSVC/Windows (`.vcxproj` under `build/`); cannot be compiled locally (only `g++` on a
  Linux box). All changes are pending the user's Windows build.
- Compilation was confirmed passing for Phases A–D. This round's wiring (layout dispatch + panel
  adoption) has **not yet** been re-verified by a build.

## Remaining steps
1. **Build & smoke test (Windows/MSVC).** Confirm the full facet refactor (Phases A–E + the
   `GetFacet<LayoutEngine>` migration + retired static facades + void* architecture guard) compiles
   and that layout, visual states, templates, binding, and triggers behave identically. Run the
   layout/golden/perf + `FrameworkConformanceTests` suites.

### Done — `void*` architecture lock
- Added a CMake guard in `cmake/CheckArchitecture.cmake` that forbids the untyped regression
  signature `void* facets_` in `src/gui/core/State.hpp`. The `ElementHost` facet matrix is typed
  (`Core::Facet* facets_[]`), so the guard passes today and blocks any return to a raw `void*` bag.
- The legitimate `void* context` parameter on `ElementTreeLifecycleHandler` was left untouched (it
  is not the facet storage and never matches `void* facets_`).
- The stale `void*` mention in the `ElementHost` comment was reworded (services are fully faceted
  now).

### Done after the GetFacet<LayoutEngine> migration
- `LayoutEngine` is now a first-class host-matrix facet: `ViewState::CreateUiEngines` registers the
  view's `Aero::LayoutEngine*` into `elementHost` via `elementHost.SetFacet(layout)` (at
  `FacetId::LayoutEngine`, matching `FacetTrait<LayoutEngine>`).
- Every `Core::LayoutFacet::LayoutManager(element)` call site was replaced with
  `Core::GetFacet<::Aero::LayoutEngine>(element)`, which resolves through
  `element.GetTree()->Host()->GetFacet<T>()` — i.e. the same per-tree engine the old
  `tree->Layout()` path returned, so the engine reachability and the cross-engine
  `VerifyElement`/`Detach` guards are preserved.
- The `void*`-returning `LayoutFacet::LayoutManager` static forwarder (declaration in
  `LayoutFacet.hpp`, definition in `Layout.cpp`) was deleted. `ElementTree::Layout()` /
  `AttachPresentation`'s `layout_` cache is now unused but left in place as a harmless accessor.

### Done — static `LayoutFacet` state forwarders retired
- All 14 reference-returning static `LayoutFacet::X(element)` forwarders (`LayoutAttached`,
  `MeasureValid`, `ArrangeValid`, `MeasureQueued`, `ArrangeQueued`, `Measuring`, `Arranging`,
  `DesiredSize`, `RenderSize`, `UntransformedDesiredSize`, `PreviousMeasureConstraint`, `LayoutSlot`,
  `LayoutClip`, `LayoutRevision`) were deleted (declarations in `LayoutFacet.hpp`, definitions in
  `Layout.cpp`).
- Every call site was inlined to a direct facet call `arg.ElementFacet<Aero::Core::LayoutFacet>()`
  (handling `*this`/`*item`/`*element`/`*root_`/`*childElement` argument forms) using the facet's
  existing getters/setters: reads → `IsX()`/`GetX()`, writes → `SetX(value)`, and
  `++LayoutRevision` → `BumpLayoutRevision()`. 64 sites updated across `Layout.cpp` (53),
  `UIElement.cpp` (8), `View.cpp` (3).
- Intentionally **kept** (not state forwarders): `LayoutFacet::MeasureOverride` / `ArrangeOverride`
  (used by `DefaultLayoutFacet` to fall back to `UIElement::MeasureOverride`) and
  `LayoutFacet::SetActualSize` (sets `ActualWidth`/`ActualHeight` read-only DPs).

## Relevant files
| File | Role |
|------|------|
| `src/gui/core/State.hpp` | `ElementHost` matrix, unified `Core::GetFacet<T>`, `UIElement` facet accessors (defs) |
| `src/gui/core/Facet.hpp` | `FacetId` (Count=23), `FacetType`, `Facet` base, `FacetTrait` primary |
| `src/gui/core/facets/ServiceFacets.hpp` | 6 typed service facet classes |
| `src/gui/core/facets/LayoutFacet.hpp` | abstract `LayoutFacet` + `*LayoutCalculator` kernels; relocated layout state + getters/setters (static forwarders removed) |
| `src/gui/core/facets/LayoutFacets.hpp` | `DefaultLayoutFacet` + 5 subclassable layout facets (Measure/Arrange now declared) |
| `src/gui/core/state/EventRouter.hpp` | engine `FacetTrait` specializations (7 `GetFacet` removed) |
| `src/gui/core/ElementTree.cpp` | engine/service accessors via `GetFacet<T>`; drives element-facet lifecycle |
| `include/Aero/UIElement.hpp` | element facet bag, `ElementFacet` (public) / `SetElementFacet` (protected); layout state fields removed |
| `src/gui/View.cpp` | `ViewState` facet members + registration; `SetFacet` for engines |
| `src/gui/controls/Layout.cpp` | `UIElement` facet lifecycle ownership; `LayoutEngine` dispatch via `ElementFacet`; inline facet state access at all call sites |
| `src/gui/core/UIElement.cpp` | 14 out-of-line layout getters + field-access reroutes through `ElementFacet` |
| `src/gui/controls/Panels.cpp` | six panel ctors register concrete layout facets; panel algorithms ported into facet Measure/Arrange |
| `include/Aero/Controls/StackPanel.hpp` | `DockPanel`/`WrapPanel`/`UniformGrid` ctors moved to `.cpp`; `UniformGrid` friendship for `GridLayoutFacet` |
| `include/Aero/Controls/Grid.hpp` | `Grid` friendship for `GridLayoutFacet` |
| `docs/WPF_Facet_Pattern_Architecture_Whitepaper.md` | design spec |
