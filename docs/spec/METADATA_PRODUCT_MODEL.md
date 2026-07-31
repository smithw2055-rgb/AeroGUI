# Metadata and Product Target Model

## Metadata ownership

Built-in metadata is registered in dependency order: Core UI semantics,
Controls, App and Markup. Each product owns its own runtime types.
`Application` and `Window` metadata are registered by the App module; Controls
does not duplicate those XAML identities.

UI and Controls retain one metadata translation unit each so conversion and
collection helpers are not duplicated. Registration bodies may be split into
private semantic units, but their order is part of the schema contract.

## Product targets

The supported installed targets are:

- `Aero::Gui`: retained WPF/XAML UI, controls and markup.
- `Aero::App`: default Application/Window lifetime above Gui and Integration.
- `Aero::Integration`: View, endpoint and native/backend integration.
- `Aero::Meta`: Gui plus typed metadata/module authoring.

There is no second public runtime framework. Internal composition targets may
be exported only as transitive implementation dependencies for static linking.
They are not documented or consumed directly by application code.

## Invariants

- One XAML type identity is registered by exactly one built-in metadata module.
- Application and Window metadata do not reside in Controls.
- Integration may consume App metadata through the private AppModel target
  without linking the native App lifetime back into Gui.
- Build-tree and installed-package consumers see the same product target names.
- Dependencies point from App/Integration/Meta toward Gui and lower layers,
  never from Gui toward application lifetime or native backends.
