# AeroGUI logical and visual tree model

AeroGUI follows WPF terminology: there is a logical tree and a visual tree.
“Object tree” is useful as a description of an XAML object graph, but it is not
an AeroGUI runtime class or public API.

## Public element spine

```text
DispatcherObject
└─ DependencyObject
   ├─ Visual
   │  └─ UIElement
   │     └─ FrameworkElement
   └─ ContentElement
      └─ FrameworkContentElement
         └─ TextElement
            └─ Inline
```

`ContentElement` is nonvisual. It can own resources, style, DataContext and
routed-event handlers through `FrameworkContentElement`, but it is rendered by a
content host such as `TextBlock`.

## Visual tree

The visual tree contains only `Visual` objects and is inspected with
`VisualTreeHelper`.

- Panel children are visual children.
- Decorator.Child is a visual child.
- A Control template root is a visual child of the Control.
- Generated item containers are visual children of the items presenter/panel.
- Inline objects are not visual children of TextBlock.

Adding or removing a visual child connects or disconnects layout, hit testing,
render invalidation and View ownership through the private `GuiContext`.

## Logical tree

The logical tree represents authored content and is inspected with
`LogicalTreeHelper`.

- Panel.Children are logical children.
- Decorator.Child is a logical child.
- ContentControl.Content is logical content even when it is not a Visual.
- ItemsControl.Items are logical content; generated containers belong to the
  visual realization.
- TextBlock.Inlines and Span.Inlines are logical content.

Logical parentage supplies inherited properties such as DataContext and the
fallback parent for resources and routed events.

## Routed events

The event route contains `DependencyObject` nodes rather than Visual-only nodes.
The parent rule is centralized:

1. a UIElement follows its visual parent;
2. when no visual parent exists, framework logical parentage is used;
3. a ContentElement follows its content host/logical parent;
4. Window and popup presentation boundaries terminate or redirect the route.

Preview and bubble traversal share one snapshot and one event-args instance.
RoutedCommand uses the same route for CanExecute and Executed.

## Ownership and lifetime

Controls own authored child objects through `Base::Ref`. `GuiContext` tracks the
attached View, lifecycle queue, non-owning relationship indexes and runtime
identity, but does not expose a second tree API. The current pre-1.0
implementation retains generation-safe visual handles for queued layout, input
and control state; these handles are internal identity tokens, not child
ownership. Detach clears input focus/capture, template state and inherited
property participation before the owning reference is released.

## Removed concepts

The following names are retired and must not return:

```text
ObjectTree
MountService
VisualTreeMount
MountEdgeState
UiMountState
MountRootState
PresentationRuntime
RuntimeFwd
```

New code should use the element's Content/Children/Items contract,
VisualTreeHelper/LogicalTreeHelper for traversal, and GuiContext only inside the
View implementation.
