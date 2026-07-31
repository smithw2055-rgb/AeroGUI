# Metadata and Product Target Model

## Metadata ownership

Built-in metadata is registered in dependency order: Core UI semantics,
App, Controls and Markup. Each product owns its own runtime types.
`Application` is registered only by the `Aero.App` metadata module; Window
remains in Controls because it is a WPF ContentControl.

UI runtime and Controls retain one translation unit each so their private
conversion and collection helpers are not duplicated. Their registration
bodies are split into named semantic units, called in a fixed order by the
module population function. Reordering units is a schema change.

## Product targets

The installed public targets are:

- `Aero::Gui`: retained WPF/XAML UI, controls, and markup.
- `Aero::App`: default Application/Window lifetime above Gui and Integration.
- `Aero::Integration`: runtime and native/backend integration facade.
- `Aero::ModuleSdk`: Gui plus typed metadata/module authoring.

Lower-level targets remain available for specialized embedders, but public
umbrella headers map directly to these product targets.

## Invariants

- One TypeId is registered by exactly one built-in metadata module.
- Application metadata does not reside in UI runtime or Controls.
- Runtime can consume App metadata through the internal AppModel target
  without linking the native App host back into Runtime.
- Build-tree and installed-package consumers see the same target names.
- Product target dependencies point from App/Integration/ModuleSdk toward
  Gui and lower layers, never from Gui toward application lifetime.
