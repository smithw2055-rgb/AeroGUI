# Gui-owned XAML runtime

S2C moves source loading, parsing, compiled-document replay and shared cache
invalidation behind one Gui-owned XAML runtime.

## Ownership

`Gui::Impl` now owns one source-private `Markup::Detail::XamlRuntime`. It binds
together the three process-level XAML objects that already belong to Gui:

- the frozen `GuiSchema`;
- the shared `DocumentCache`;
- the canonical `XamlProviderRegistry`.

Views retain their Gui state and reference this runtime. A View no longer
constructs `Markup::Loader` directly and no longer performs provider revision
probing or compiled-cache invalidation itself.

## View-affine state

Object construction is intentionally still View-affine. Every load operation
passes the View's `Markup::LoadState` into the Gui-owned runtime. That state
contains the dispatcher, effective-value engine, resource environment,
name-scope and effect lifetime required by ObjectWriter.

This split keeps the ownership boundary precise:

```text
Gui XamlRuntime
  schema
  providers
  compiled document cache
  source revision and invalidation
        |
        | load/parse/compiled replay
        v
View LoadState
  dispatcher and object factory
  dependency-value engine
  resource environment
  effect lifetime
  mount transaction
```

The immutable compiled representation may be shared by all Views created from
the same Gui. The materialized object graph and side effects remain owned by the
View that requested the load.

## Removed View responsibilities

`View.cpp` no longer directly:

- creates `Markup::Loader` for documents or resource dictionaries;
- resolves an XAML provider to probe source revisions;
- hashes provider streams when a revision is unavailable;
- reads or invalidates the shared `DocumentCache`.

The private View methods remain thin compatibility entry points for
`ViewAccess`; they validate View state, build per-View reader settings and then
delegate to the Gui-owned runtime.

## Resulting load path

```text
XamlReader / ReloadCoordinator
  -> ViewAccess
  -> View validation and XamlReaderSettings
  -> Gui::Impl::xaml
  -> Loader(schema, providers, View LoadState)
  -> shared DocumentCache
  -> View mount transaction
```

This stage deliberately does not move resource-layer dictionaries or the
ObjectWriter effect lifetime into Gui. Those objects are presentation-instance
state and must not be shared between Views.
