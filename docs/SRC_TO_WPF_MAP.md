# AeroGUI source map (WPF developer orientation)

This document maps the WPF-shaped public surface (`include/Aero`) to the
implementation files under `src/gui`, so you can find the code for a type by
its WPF namespace rather than by an implementation-mechanism folder.

## Namespace → source directory

| Public namespace | Installed headers | Implementation (src/gui) |
| --- | --- | --- |
| `Aero` (root) | `Gui.hpp`, `View.hpp`, `DependencyObject.hpp`, `UIElement.hpp`, `FrameworkElement.hpp`, `Visual.hpp`, `Freezable.hpp`, `ContentElement.hpp`, `RoutedEvent.hpp`, `Value.hpp`, `Resources.hpp`, `Style.hpp`, `Events.hpp`, `Layout.hpp`, `Shapes.hpp`, `Documents.hpp`, `Threading.hpp`, `Collections.hpp`, `Meta.hpp`, `Module.hpp` | `core/` (elements, property system, Freezable, events, Dispatcher, layout engine; Resources/Style lives in `styles/`), `data/` (Aero::Data Binding), `styles/`, `meta/` (Meta/Module), `triggers/` (behavior), `controls/` (controls + layout engine implementation), root `Gui.cpp`/`View.cpp` |
| `Aero::Controls` | `Controls/*.hpp` | `controls/` + `templates/` (ControlTemplate/DataTemplate) |
| `Aero::Data` | `Data/Binding.hpp`, `DataTemplate.hpp` | `data/` (Binding) + `templates/` (DataTemplate) |
| `Aero::Markup` | `Markup/*.hpp` | `markup/` |
| `Aero::Media` | `Media/*.hpp` | `media/` |
| `Aero::Text` (private) | — | `text/` |
| `Aero::Input` | `Input.hpp`, `InputInterop.hpp` | `input/` |
| `Aero` (style triggers) | `Triggers/*.hpp` | `triggers/` |
| `Aero::Diagnostics` | `Diagnostics.hpp`, `Diagnostics/*` | `diagnostics/` |
| `Aero::Events` | `Events/*.hpp` | `core/` (RoutedEvent/EventRouter) |

## Consolidated implementation files → public types

Several `.cpp` files implement many related public types (deliberate, per
`docs/SOURCE_ARCHITECTURE.md`; see "No 1:1 header mapping"). Each file lists
its types below.

- `controls/Buttons.cpp` — `ButtonBase`, `Button`, `RepeatButton`, `ToggleButton`, `CheckBox`, `RadioButton`
- `controls/ContentControls.cpp` — `ContentControl`, `UserControl`, `ContentPresenter`, `Popup`, `HeaderedContentControl`, `HeaderedItemsControl`
- `controls/Shapes.cpp` — `Rectangle`, `Ellipse`, `Path`, `Line`, `Polygon`, `Polyline`
- `controls/Items.cpp` — `ItemsControl`, `ItemCollection`, `ItemContainerGenerator`
- `controls/Selection.cpp` — `Selector`, `ListBox`
- `controls/Scroll.cpp` — `ScrollViewer`, `ScrollBar`
- `controls/TextBox.cpp` — `TextBox`, `TextBoxBase`
- `controls/Bars.cpp` — `ToolBar`, `StatusBar`
- `controls/Menus.cpp` — `Menu`, `TreeView` (menu parts)
- `controls/Trees.cpp` — `TreeView`, `ListView` (tree parts)
- `controls/Virtualization.cpp` — `VirtualizingStackPanel`
- `controls/Layout.cpp` — `LayoutEngine` (Measure/Arrange engine) + `UIElement`/`FrameworkElement` layout method bodies
- `controls/VisualStates.cpp` — `VisualStateManager` groups
- `controls/RichText.cpp` — rich-text token helpers (`TrimRichTextToken`, `FindRichTextToken`, `AppendRichTextValue/Binding`), `ApplyRichText`, `RichText::OnTextChanged` (the `Run`/`Span`/`Bold`/`Italic`/`Underline`/`LineBreak` bodies are inline in their public headers)
- `templates/Templates.cpp` — `ControlTemplate`, `DataTemplate` runtime programs
- `markup/XamlObjectWriter.cpp` — `XamlObjectWriter`
- `markup/XamlLoader.cpp` — XAML load + `StaticResource`/`DynamicResource`
- `markup/XamlSchemaContext.cpp` — `XamlSchemaContext`
- `markup/XamlParser.cpp` — XML tokenizer + `XamlNodeReader`
- `markup/TemplateProgram.cpp` — `ControlTemplate`/`DataTemplate` runtime programs
- `meta/Metadata.cpp` — `Registry`, type/value metadata
- `core/PropertySystem.cpp` — `DependencyProperty` effective-value engine
- `styles/Style.cpp` — `StyleEngine` (style application/seal; delegates trigger evaluation to `TriggerEngine`)
- `triggers/TriggerEngine.cpp` — `TriggerEngine` (style/control/template trigger evaluation, deferred trigger phase, `SetBindingTriggerState`)
- `triggers/BaseTrigger.cpp` — `TriggerBase` (+ shared `InvalidStyle` diagnostic)
- `triggers/Trigger.cpp` — `Trigger` (property trigger)
- `triggers/DataTrigger.cpp` — `DataTrigger`
- `triggers/Condition.cpp` — `Condition`
- `triggers/MultiTrigger.cpp` — `MultiTrigger`
- `triggers/MultiDataTrigger.cpp` — `MultiDataTrigger`
- `interactivity/BlendBehaviors.cpp` — `Interaction::*` Blend behaviors (moved from `controls/`)
- `interactivity/Interactivity.cpp` — `Interaction` attached properties

## Private implementation access headers

Internal state/access headers use `*State.hpp` (formerly `*Runtime.hpp`):
`core/ElementState.hpp`, `core/PropertyState.hpp`, `core/MetadataState.hpp`,
`binding/BindingState.hpp`, `controls/ControlState.hpp`, `controls/ItemsState.hpp`,
`templates/TemplateState.hpp`, `input/InputState.hpp`, `layout/LayoutState.hpp`,
`markup/MarkupState.hpp`, `markup/XamlState.hpp`, `media/MediaState.hpp`,
`styles/StyleState.hpp`, `core/LayoutState.hpp`, `data/BindingState.hpp`, `triggers/TriggerEngine.hpp`, `triggers/TriggerPlan.hpp`,
`triggers/TriggerDiagnostics.hpp`, `triggers/TriggerValueCompare.hpp`,
`core/state/*.hpp` (per-domain split of the former single `State.hpp` into the facet/engine headers).

## WPF virtual override surface

Aero types expose WPF-shaped virtuals you can override when subclassing:

| WPF member | Aero declaration | Notes |
| --- | --- | --- |
| `Visual.GetVisualChildrenCount` | `Visual::GetVisualChildrenCount()` | renamed from `GetVisualChildrenCountCore` |
| `Visual.GetVisualChild` | `Visual::GetVisualChild(int)` | renamed from `GetVisualChildCore` |
| `FrameworkElement.GetLogicalChildrenCount` | `FrameworkElement::GetLogicalChildrenCount()` | renamed from `GetLogicalChildrenCountCore` |
| `FrameworkElement.GetLogicalChild` | `FrameworkElement::GetLogicalChild(int)` | renamed from `GetLogicalChildCore` |
| `DependencyObject.OnPropertyChanged` | `DependencyObject::OnPropertyChanged(const DependencyPropertyChangedEventArgs&)` | new WPF-bridge hook; fires with `PropertyMetadata::PropertyChangedCallback` |
| `Visual.OnVisualParentChanged` | `Visual::OnVisualParentChanged(Visual* oldParent)` | new WPF-bridge hook |

Runtime engines are reached through `Core::GetFacet<T>(element)`
(`src/gui/core/Facet.hpp`) instead of per-facade static methods.

## Platform code (two trees, by design)

- `src/app/platform/{win32,x11}/` — window, clipboard, IME; links into `AeroApp`.
- `src/render/platform/{win32,x11}/` — GL surface adapters; links into `AeroRenderOpenGL33`.
