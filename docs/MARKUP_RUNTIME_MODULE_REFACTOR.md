# AeroGUI Root Runtime, Module and Markup Refactor

## Status

Accepted architecture direction for the next AeroGUI refactor.

## Goals

- Keep `Aero::Markup` focused on declarative document processing.
- Move the UI composition root and module registration entry points to the root `Aero` namespace.
- Register control, presentation and application capabilities once through metadata descriptors and facets.
- Remove XAML-specific duplicate registration paths.
- Separate declarative content construction from Presentation tree mounting.
- Remove slice-era compatibility and validation APIs that are no longer used by production code.

## Final ownership model

### Root `Aero`

The following are top-level framework entry points and live directly under `include/Aero` and `src`:

- `Aero::RuntimeHost`
- `Aero::RuntimeHostOptions`
- `Aero::RuntimeFrameResult`
- `Aero::ModuleRegistration`
- `Aero::ModuleCatalog`
- built-in module composition entry points

No `Aero::Runtime` namespace and no `include/Aero/Runtime` or `src/runtime` directory are introduced.

### `Aero::Markup`

Markup owns only the declarative loading pipeline:

```text
UTF-8 XML
 -> XmlTokenizer
 -> XamlNodeReader
 -> XamlSchemaContext
 -> XamlObjectWriter
 -> XamlLoadResult
```

Compiled XAML reuses the same schema and object-writer semantics.

### `Aero::Presentation`

Presentation owns runtime graph and UI services:

- logical and visual object-tree mounting
- layout-root attachment and resizing
- render invalidation
- NameScope and ResourceDictionary runtime ownership
- Binding and DynamicResource runtime behavior

### `Aero::Controls`

Controls own their concrete metadata and facets:

- type factories
- property accessors
- content facets
- initialization facets
- style/template declaration object facets
- interaction behavior

Markup consumes these sealed capabilities. It does not register control adapters itself.

## Module registration

`XamlModuleManifest` and `XamlModuleCatalog` are replaced by root-level module concepts.

```cpp
namespace Aero {

struct ModuleRegistration final {
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    Core::MetadataModuleRegisterCallback registerModule = nullptr;
    void* context = nullptr;
};

class ModuleCatalog final {
public:
    Base::Result<void> TryAdd(const ModuleRegistration&) noexcept;
    Base::Result<void> Register(Core::MetadataDomain&) const noexcept;
    Base::Result<void> Freeze() noexcept;
};

}
```

Rules:

1. A module registers descriptors and facets in one metadata transaction.
2. Built-in UI is a normal module, not a boolean special case.
3. Modules may be added only before runtime initialization.
4. The catalog freezes when initialization starts.
5. There is no second `configureXaml` phase.
6. Host-specific activation overrides are runtime options, not static module metadata.

## Facet-driven declarative behavior

The following behavior must be available through sealed metadata/facets instead of XAML-specific registries:

- object creation
- object initialization
- property access
- content write/clear
- collection content
- markup-extension value providers
- NameScope ownership
- ResourceDictionary ownership
- object-kind adaptation such as `Base::Object` to `DependencyObject`

`XamlSchemaContext` remains responsible for XAML syntax and schema resolution, but does not own duplicate control capability registries.

## RuntimeHost

`RuntimeHost` is the application/UI composition root. It is not a Markup type.

It composes:

- Dispatcher
- MetadataDomain and MetadataRuntime
- EffectiveValueEngine
- ObjectTree
- LayoutManager
- RenderManager
- BindingManager
- RoutedEventManager
- CommandManager
- TemplateManager and VisualStateManager
- input and interaction managers
- XAML schema and object-loading services

Public API must expose workflow-oriented operations rather than unrestricted internal service access:

```cpp
RuntimeHost::AddModule(...)
RuntimeHost::Initialize(...)
RuntimeHost::LoadXaml(...)
RuntimeHost::LoadCompiledXaml(...)
RuntimeHost::Mount(...)
RuntimeHost::Resize(...)
RuntimeHost::Unmount()
RuntimeHost::DispatchPointer(...)
RuntimeHost::DispatchKeyboard(...)
RuntimeHost::DispatchText(...)
RuntimeHost::RunFrame()
RuntimeHost::FindNamed<T>(...)
```

Low-level service access is retained only where required by advanced integration and tests.

## Visual content and mounting

`XamlVisualTreeHost` currently combines two responsibilities:

1. Markup-time content staging using `ContentFacet`.
2. Presentation-time logical, visual, layout and render mounting.

It is split into:

```text
Markup::XamlLoadResult / VisualContentPlan
Presentation::VisualTreeMount
```

The object writer produces an ordered content plan. Presentation mounts in declaration order and unmounts in reverse order. This removes repeated graph scans and keeps Markup independent from long-lived layout/render ownership.

`RuntimeHost` connects the load result to the Presentation mount service.

## NameScope and resources

`XamlObjectWriter` must not remain alive only to retain committed NameScope and ResourceDictionary state.

A successful load returns ownership in `XamlLoadResult`:

```cpp
struct XamlLoadResult final {
    Base::Ref<Base::Object> root;
    NameScope names;
    ResourceDictionary resources;
    VisualContentPlan visualContent;
};
```

The exact ownership namespace may move to Presentation as part of the implementation slice, but the writer remains a loading session rather than a mounted-view owner.

## Activation

Default object construction uses `TypeFactoryFacet` through `MetadataRuntime`.

Host-specific overrides are supplied through `RuntimeHostOptions`. Thread-local ambient activation state is removed; the object writer receives an explicit load/activation context.

## Compiled-XAML identity

`MetadataDomain::ComputeSchemaHash()` is the single schema identity when it covers:

- module IDs and schema versions
- descriptors
- sealed facets and their ABI/format versions

After all XAML-affecting behavior is represented by facets, the separate `moduleManifestHash` is removed from schema context and compiled-XAML cache identity.

## Theme and style

### Theme

`XamlTheme` must not maintain a second XML parser and hard-coded control-name switch.

Default themes use the standard XAML pipeline or compiled XAML. A root-level or Presentation `ThemeManager` selects and applies the resulting resource/style/template package.

### Style

Style and Setter declaration objects become normal metadata-driven objects. They are constructible through type factories and writable through property/content facets. Markup no longer installs a large private set of Style-specific member adapters and activation providers.

## Files to remove or relocate

### Relocate

- `include/Aero/Markup/RuntimeHost.hpp` -> `include/Aero/RuntimeHost.hpp`
- runtime implementation from `src/markup/RuntimeHost.inc` -> `src/RuntimeHost.cpp`
- module catalog from `XamlModuleSdk.*` -> root `Module.*`
- queued render backend -> Presentation or Render
- tree mounting logic -> Presentation

### Remove after migration

- `RuntimeWindow.inc`
- `RuntimeServices.hpp/.inc` compatibility facade and unused slice state models
- unused `RuntimeSafety.hpp/.inc` public APIs, or relocate genuinely shared utilities to Core/Presentation
- `configureXaml` and `xamlSchemaVersion`
- mutable module access after initialization
- thread-local active XAML activation
- duplicate DependencyObject cast callbacks in Binding, Style and DynamicResource
- `XamlTheme` private XML DOM/parser

## Implementation slices

### Slice A — root entry points and translation units

- add root `Module.hpp/.cpp`
- move `RuntimeHost` to root namespace and paths
- convert `.inc` implementation aggregation into normal `.cpp` files
- move queued render backend out of Markup
- freeze module catalog at initialization

### Slice B — one registration path

- represent XAML-affecting control behavior as metadata facets
- remove `configureXaml`
- make built-in UI a normal module
- remove XAML activation registration for ordinary type construction

### Slice C — load result and tree mount split

- add `XamlLoadResult` and ordered visual content plan
- transfer NameScope/resources out of the writer
- add Presentation-owned `VisualTreeMount`
- remove long-lived public Writer/VisualTree access from RuntimeHost

### Slice D — schema and activation cleanup

- replace thread-local activation with explicit load context
- remove redundant generic providers where facets cover the behavior
- unify object-kind adaptation
- simplify compiled-XAML identity

### Slice E — style, resources and theme

- metadata-driven Style/Setter declaration objects
- standard XAML/compiled-XAML theme loading
- remove the private Theme parser and hard-coded type mapping

### Slice F — cleanup and documentation

- delete obsolete Runtime compatibility/safety APIs
- migrate tests to the owning layer
- update architecture documentation and samples
- verify no duplicate XAML-specific registration remains

## Acceptance criteria

- `Aero::Markup` contains no UI composition root.
- Runtime and modules are root-level `Aero` APIs.
- Built-in and external controls use one metadata/facet registration path.
- XAML schema consumes sealed facets and does not duplicate control registration.
- Runtime and compiled XAML share object-writer semantics.
- Visual content planning is separate from Presentation mounting.
- default themes use the standard XAML pipeline.
- no `.inc` file is used to hide unrelated implementation units.
- all existing tests pass after migration and focused architecture tests enforce the new dependency direction.
