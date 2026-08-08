# S14 Noesis-style public header closure

S14 removes implementation-category declaration ownership from the installed
controls SDK without adding a new runtime, service, access, contract, or CMake
product layer.

## Public declaration ownership

Type-named headers now own declarations. Related types remain grouped when they
form one WPF concept or one tight C++ dependency family. Examples:

- `Control.hpp`, `ContentControl.hpp`, `Panel.hpp`, `Decorator.hpp`;
- `ButtonBase.hpp` for the button primitive family;
- `RangeBase.hpp` for range/track/scrollbar primitives;
- `ItemsControl.hpp` for the item-generation foundation;
- `ListBox.hpp`, `ComboBox.hpp`, `ListView.hpp`, `TreeView.hpp`;
- `TextBoxBase.hpp`, `TextBox.hpp`, `PasswordBox.hpp`.

`Core.hpp`, `Common.hpp`, `Panels.hpp`, `Items.hpp`, `Primitives.hpp`, and
`Text.hpp` remain compatibility umbrellas only. They contain includes and no
public type declarations.

## WPF-facing static member spelling

The temporary `Members::Property`, `Members::ReadOnlyProperty`,
`Members::AttachedProperty`, and `Members::RoutedEvent` categories are retired.
Types declare static identifiers through the owner-aware aliases introduced in
S13: `DependencyProperty<T>`, `ReadOnlyDependencyProperty<T>`,
`AttachedProperty<T>`, and `RoutedEvent<TArgs>`.

## Product boundary

No new binary target or runtime layer is introduced. Existing source files may
continue to include compatibility umbrellas while translation units are
incrementally narrowed. The public header model is now WPF-discoverable while
source organization remains module-oriented rather than one `.cpp` per class.
