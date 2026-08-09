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
- `Aero::Render`: backend-neutral contracts implemented by Gui, with no
  additional DLL.
- `Aero::RenderD3D11` and `Aero::RenderOpenGL33`: native backend products.
- `Aero::App`: default Application/Window and presentation lifetime above Gui.
- `Aero::Audio`: optional audio product.

`Aero::Meta` is Gui's typed metadata/module authoring namespace, not an
independent linker target. There is no Integration product.

There is no second public runtime framework. Internal composition targets may
be exported only as transitive implementation dependencies for static linking.
They are not documented or consumed directly by application code.

## Invariants

- One XAML type identity is registered by exactly one built-in metadata module.
- Application and Window metadata do not reside in Controls.
- Build-tree and installed-package consumers see the same product target names.
- Dependencies point from App and Render backends toward Gui and lower layers,
  never from Gui toward application lifetime or native backends.
