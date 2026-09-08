# AeroGUI source map (src/gui)

Public headers are one-class-per-file under `include/Aero` (WPF naming).
Implementation files under `src/gui` aggregate by responsibility. This table
maps the non-obvious cases so contributors find code by its public name.

Filesystem stays flat: no subdirectories under `src/gui/controls`, no `host/`
dir. IDE-only virtual folders are defined via `source_group()` in
`cmake/AeroGuiTargets.cmake` (`gui/host`, `gui/controls/{text,scroll,items,chrome}`).

## Composition root (`src/gui/`)

| Public surface | Implementation |
| --- | --- |
| `Aero::Gui` | `Gui.cpp` + `GuiState.hpp` (provider handlers live in `Gui.cpp`) |
| `Aero::View` | `View.cpp` (construct/mount) + `ViewFrame.cpp` (clocks) + `ViewInput.cpp` (pointer/keyboard/text **+ focus queue**, merged from `ViewFocus.cpp`) + `ViewRender.cpp` (render sync) + `ViewDocuments.cpp` (§1 mount, §2 resources, §3 fragments) |
| `Aero::IRenderer` | `ViewRenderer.hpp` / `ViewRenderer.cpp` (only concrete renderer) |
| Hub state | `ViewState.hpp` (no `<Aero/Controls.hpp>` umbrella; Controls types via `internal/AeroGuiInternal.hpp`) |

## Controls (`src/gui/controls/`, flat)

| Public header(s) | Implementation |
| --- | --- |
| `Controls/Button.hpp`, `CheckBox.hpp`, `RadioButton.hpp`, `Primitives/*Button*` | `Buttons.cpp` (+ `Bars.cpp` for menu/status/tool bars) |
| `Controls/TextBox.hpp`, `TextBoxBase.hpp`, `PasswordBox.hpp` | `TextBox.cpp` + `TextBoxInteraction.cpp` (behavior + selection/caret, merged from `TextBoxBehavior.cpp` + `TextBoxSelection.cpp`) + `TextBoxCommon.hpp` |
| `Controls/ScrollViewer*.hpp`, `Primitives/ScrollBar.hpp` | `ScrollViewer.cpp` + `ScrollBar.cpp` + `ScrollBehavior.cpp` + `ScrollContentPresenter.cpp` (shared: `ScrollCommon.hpp`) |
| `Controls/ItemsControl.hpp`, `ListBox*.hpp`, `TreeView*.hpp`, `ComboBox*.hpp` | `Items.cpp` + `ItemContainerGenerator.cpp` + `Selection.cpp` + `Trees.cpp` + `ListView.cpp` + `Virtualization.cpp` (shared: `ItemsContainers.hpp`, renamed from `ItemsDetail.hpp`) |
| `Controls/*Panel*.hpp`, `Grid*.hpp`, `Canvas.hpp` | `Panels.cpp` |
| `Controls/Menu*.hpp`, `ContextMenu*.hpp`, `ToolBar.hpp` | `Menus.cpp` + `Bars.cpp` |
| `Controls/ContentControl.hpp`, `UserControl.hpp`, `Page.hpp`, `GroupBox.hpp` … | `ContentControls.cpp` |
| `Controls/Image.hpp` | `Images.cpp` |
| `VisualStateManager.hpp` | `VisualStateManager.cpp` |
| Metadata bootstrap | `ControlsMetadata.cpp` + `Metadata.hpp` + `metadata/Metadata.{Foundation,Widgets,Layout}.inl` (7 former `*.inl` merged in stable registration order) |

## Media / animation (`src/gui/media/`, flat)

| Public surface | Implementation |
| --- | --- |
| `Media/Animation/*` timelines | `AnimationEngine.cpp` + `AnimationEngine.Apply.cpp` + `Animation.cpp` (+ `AnimationModel.hpp`, `AnimationEngine.hpp`) |
| `Media/Animation/Storyboard*`, `EventTrigger` | `StoryboardHost.cpp` + `.Timelines/.Properties/.Actions/.Events/.Completions.cpp` (keyframe helper in `StoryboardHost.hpp`, merged from `StoryboardHostCommon.hpp`) |
| `Media/Brush*.hpp`, `Media/Effect*.hpp` | `Brushes.cpp` + `Effects.cpp` + `Pen.cpp` |
| `Media/Geometry*.hpp`, `PathGeometry`, `StreamGeometry` | `Geometry.cpp` + `GeometryFlatten.cpp` + `StrokeTessellate.cpp` + `StreamGeometry.cpp` |
| `Media/Image*.hpp` | `Images.cpp` + `ImageCache.cpp/.hpp` |

## Other domains

| Public surface | Implementation |
| --- | --- |
| `Data/Binding*.hpp` | `data/Binding.cpp`, `BindingPath.cpp`, `BindingEvaluation.cpp`, `BindingExpression.cpp`, `BindingOperations.cpp` (shared: `BindingCommon.hpp`, `BindingEngine.hpp`), `CollectionView.cpp` |
| `Markup/Xaml*.hpp` | `markup/XamlParser.cpp`, `XamlObjectWriter*.cpp`, `XamlObjectLoader.cpp`, `XamlCompiled{Schema,Document}.cpp`, `XamlSchema*.cpp`, `XamlDocumentCache.cpp`, `GuiSchema.cpp`, `TemplateCompiler.cpp` |
| `Meta.hpp` / `Module.hpp` | `meta/Metadata.cpp`, `Module.cpp`, `BuiltinMetadata.cpp`, `BuiltinModules.cpp`, `EnumMetadata.cpp`, `Value.cpp` + `*.inl` tables + `TypeBuilderCore.hpp` (renamed from `TypeBuilderDetail.hpp`) |
| `Triggers/*`, `Interactivity/*` | `triggers/Trigger*.cpp` + `interactivity/InteractivityEngine*.cpp` + `BlendBehaviors.cpp` |
| Text stack | `text/TextPipeline.cpp`, `TextLayout.cpp`, `GlyphAtlas.cpp`, `FontManager.cpp`, `EditableText.cpp` + `freetype/` + `harfbuzz/` adapters |
| Input | `input/Input.cpp` (routing), `Commands.cpp`, `OverlayHost.cpp`, `Clipboard.cpp`, `DragDrop.cpp`, `Cursor(s).cpp`, `Mouse.cpp`, `Keyboard.cpp`, `DataObject.cpp`; focus queue in `ViewInput.cpp`, declaration in `input/InputState.hpp` (merged from `FocusHost.hpp`) |
| Core kernel | `core/ElementTree.cpp`, `PropertySystem.cpp`, `DependencyObject.cpp`, `LayoutEngine.cpp`, `Visual.cpp`, `UIElement.cpp`, `FrameworkElement.cpp`, `Dispatcher.cpp`, `RoutedEvents.cpp` (+ `core/state/*Engine.hpp`, `internal/AeroGuiInternal*.hpp`, `internal/PropertyStore.hpp`); single-TU helpers live in their `.cpp` (e.g. `Invariants.cpp`, merged from `Invariants.hpp`) |
| Styles / templates | `styles/Resources.cpp`, `Style.cpp` (+ `StyleEngine.hpp`, `StyleState.hpp`, `ResourceHost.hpp`); `templates/Templates.cpp` |
| Documents / shapes / diagnostics | `documents/Documents.cpp`, `Adorners.cpp`; `shapes/Shapes.cpp`, `Path.cpp`; `diagnostics/Diagnostics.cpp`, `Inspector.cpp` |

## Retired names (do not reintroduce)

`ViewFocus.cpp` → `ViewInput.cpp` §focus · `ItemsDetail.hpp` → `ItemsContainers.hpp` ·
`TypeBuilderDetail.hpp` → `TypeBuilderCore.hpp` · `StoryboardHostCommon.hpp` → `StoryboardHost.hpp` ·
`FocusHost.hpp` → `input/InputState.hpp` · `TextBoxBehavior.cpp` + `TextBoxSelection.cpp` → `TextBoxInteraction.cpp` ·
`Invariants.hpp` → `Invariants.cpp` · `metadata/{Support,Values,Templates,Primitives,Items,Panels,TextMedia}.inl` → `metadata/Metadata.{Foundation,Widgets,Layout}.inl`

## Noesis-parity round (2nd pass)

Clean-room note: the Noesis SDK under `C:\Projects\NoesisGUI\references`
was used for observable API comparison only; no implementation was copied.

| Area | Change |
| --- | --- |
| DP public surface | `DependencyObject.hpp` keeps Noesis-parity API (Get/Set/Clear/Coerce/expressions/notifications); `ChangeHandlerRecord`/`DependencyObjectRare`/`DependencyMutationScope` (ex-`MutationScope`) live in `internal/PropertyStore.hpp`. `ChangeKind` stays public (used by `Resources.hpp`). Friend: `DependencyMutationScope`. |
| Meta colocate pilot | `meta/Elements.inl`: one `PopulateUiElements` → 5 per-class `Fill*Metadata` + dispatcher, same order/linkage. Full colocate (Fill next to impl) waits on untangling `Support.inl` helpers shared in the `BuiltinMetadata.cpp` anonymous namespace. |
| Meta gap (verified) | `TemplatePart`/`DependsOn` have no Aero equivalent (Noesis: `TypeMetaData` subclasses). Consumption exists (`Control::GetTemplateChild(name)`, `PART_*` convention). Recording needs a new facet kind, but `FacetDraft::facets[11]` is single-index-per-kind while PARTs are one-to-many → requires facet-model redesign (range encoding or side-table) + template-tooling consumption. Tracked as feature design, not done here. |
| View content API | Canonical: `SetContent(doc, size)` + `SetContent(root, size)`. `SetContent(root)` is `[[deprecated]]` (no in-tree callers; `Gui::CreateView(content)` migrated to explicit empty size, behavior-identical). `SetContent(root, doc, size)` kept (used by `DesktopHost`); new fragment mounts prefer `XamlReader::MountFragment`. See `XamlReader.hpp` entry-point guide. |
| PCH tiers | `AeroPCH.hpp` gains a host-integration tier (`Input.hpp`, `TextureProvider.hpp`, `FontProvider.hpp`, `XamlReader.hpp`), mirroring the NoesisPCH Providers banner. Type-header-direct users unaffected. |

## Follow-up rounds (1/2/3)

| Area | Change |
| --- | --- |
| Meta colocate (full) | `meta/Elements.inl` is now an order-fixing dispatcher; the five `Fill*Metadata` bodies live in `core/{Visual,ContentElement,FrameworkContentElement,UIElement,FrameworkElement}.cpp` (declared in `meta/ElementsFill.hpp`). Element-only helpers moved alongside; the five render-state callbacks shared with Media fills have a single home in `UIElement.cpp`, declared in `meta/RenderStateCallbacks.hpp`. `Support.inl` keeps the remaining cross-domain converters/callbacks. |
| TemplatePart (ADR-0006) | `TemplatePartDescriptor` mirrors `EventHandlerDescriptor` in `TypeInfo` (`RegisterTemplatePart`/`FindTemplatePart` with base-type walk, `Registry.inl` composition merge, `MetadataAuthoringSession` + `TypeBuilder::TemplatePart`, `MetaConsumer` chain). Builtin PARTs declared: ScrollBar→PART_Track, ScrollViewer→PART_*ScrollBar, PasswordBox→PART_ContentHost, ComboBox→PART_EditableTextBox/PART_Popup. `DependsOn` deferred (needs invalidation-engine semantics). |
| XAML writer split | `XamlObjectWriterBuilderCore.cpp` (3005 lines) → `Builder{Load,Nodes,Values,Write}.cpp` (368/721/935/1064) by writer phase; method-only move, no shared file-locals. |
