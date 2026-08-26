# AeroGUI for WPF and NoesisGUI developers

AeroGUI keeps WPF/XAML names and semantics while using ordinary C++17 methods.
Code-first desktop applications call `Application::Run()`; generated XAML
bootstrap code may call `App::Run()` to load `App.xaml`. Engine and custom
native hosts link `Aero::Gui` plus one Render backend and use an explicit
RenderDevice/RenderTarget.

## Application and Window

```cpp
#include <AeroApp/App.hpp>

class App : public Aero::Application {
    AERO_DECLARE_TYPE_NAMED(App, Aero::Application, "urn:demo", "App")

public:
    App() noexcept {
        static_cast<void>(SetStartupUri("MainWindow.xaml"));
    }

protected:
    void OnStartup(Aero::StartupEventArgs& args) noexcept override {
        Aero::Application::OnStartup(args);
    }
};

int main() {
    App app;
    auto run = app.Run();
return run ? run.Value() : 1;
}
```

For a generated XAML application entry point, `Aero::App::Run()` still loads
`App.xaml`, reads its `StartupUri`, and runs the same private desktop host.
Backend, allocator, diagnostics and initial-window options are supplied through
`Aero::App::RunOptions`; no public launcher or host object is required.

## Properties

WPF CLR-property syntax maps directly to `GetXxx()`, `SetXxx()`, and `ClearXxx()`:

```cpp
button->SetWidth(120.0);
button->SetHeight(36.0);
button->SetIsEnabled(true);
button->ClearWidth();

double width = button->GetWidth();
bool enabled = button->GetIsEnabled();
```

Typed setters such as `SetWidth` are `void`, matching WPF. They do **not** return `Result`. Diagnostics use the `*Checked` dual rail on the DP engine (`SetValueChecked`, `ClearValueChecked`, `AddHandlerChecked`), not a `Result`-returning `SetWidth`.

Attached properties keep their WPF shape:

```cpp
Aero::Controls::Grid::SetRow(*button, 1);
Aero::Controls::Grid::SetColumn(*button, 2);
```

Dependency-property and routed-event identifiers are declared on one physical
line:

```cpp
inline static constexpr Aero::DependencyProperty<double> RatingProperty{"Rating"};
inline static constexpr Aero::RoutedEvent<Aero::RoutedEventArgs> RatingChangedEvent{"RatingChanged"};
```

## Events

```cpp
Aero::RoutedEventHandler onClick(
    [](Aero::Base::Object*, Aero::RoutedEventArgs& args) noexcept {
        args.SetHandled(true);
    });

button->Click() += onClick;
button->Click() -= onClick;
```

There is no `CLICK` macro. Subscribe with `Click() += handler`, the same `Event<T>` operator used for Preview/bubble routes.

Create instances with `Base::MakeRef`, not `new` or a CLR-style factory:

```cpp
auto button = Aero::Base::MakeRef<Aero::Controls::Button>().Value();
```

Downcasts use TypeId `TryCast`, not C++ `dynamic_cast`, WPF `as`, or Noesis `DynamicCast`:

```cpp
Aero::Controls::Button* asButton =
    Aero::TryCast<Aero::Controls::Button>(obj);
```

Preview and bubbling events use the same event arguments and one route:

```cpp
button->PreviewMouseDown() += previewHandler;
button->MouseDown() += bubbleHandler;
```

## Collections and content

```cpp
panel->GetChildren().Add(button);
itemsControl->GetItems().Add(viewModel);
textBlock->GetInlines().Add(run);
```

Tree ownership, logical/visual attachment, layout invalidation and rendering are
runtime responsibilities; application code does not call mount services.

## Binding

```cpp
auto binding = Aero::Base::MakeRef<Aero::Data::Binding>("Customer.Name").Value();
binding->SetMode(Aero::Data::BindingMode::OneWay);
binding->SetElementName("Editor");
```

Binding objects expose WPF concepts such as `Path`, `Mode`, `ElementName`,
`RelativeSource`, converter, fallback value and target-null value. Runtime
binding records and schedulers are private.

WPF `INotifyPropertyChanged` maps to `Aero::Data::NotifyPropertyChanged<T>`:

```cpp
#include <Aero/Data/NotifyPropertyChanged.hpp>
#include <Aero/Meta.hpp>

class Person : public Aero::Base::Object,
               public Aero::Data::NotifyPropertyChanged<Person> {
    AERO_DECLARE_TYPE(Person, Aero::Base::Object)
public:
    const Aero::Base::String& GetName() const noexcept { return name_; }
    void SetName(Aero::Base::String value) noexcept {
        name_ = std::move(value);
        RaisePropertyChanged("Name");
    }
private:
    Aero::Base::String name_;
};

Aero::Result<void> RegisterPerson(
    Aero::Meta::Registration& registration) noexcept {
    return Aero::Meta::Register<Person>(registration)
        .Property<Aero::Base::String, &Person::GetName, &Person::SetName>("Name")
        .PropertyChangeNotifications()
        .Factory()
        .Result();
}
```

When the source type registers property-change notifications, `{Binding}`
evaluates only on `RaisePropertyChanged`. Types without the mixin keep the
per-frame metadata-path poll and emit a diagnostic hint.

`ItemsSource` accepts any `Object` that `Implements<IItemsSource>()` at
registration. Subclasses of `ObservableCollection<T>` and user collections
are no longer compared by a hard-coded TypeId. The untyped XAML name
`ObservableCollection` is `ObservableObjectCollection` in C++
(`ObservableCollection<Object>` underneath).

## Embedded View and XAML

Engine hosts use the `Gui` façade and keep `View` focused on content, input and
frame updates:

```cpp
Aero::Gui environment;
environment.Initialize();

auto root = environment
    .LoadXaml<Aero::FrameworkElement>("app:///MainView.xaml")
    .Value();
auto view = environment.CreateView(root, options).Value();
view->SetSize({1280.0, 720.0});
view->Update(totalTimeSeconds);

// Populate an existing code-behind / custom-control instance:
environment.LoadComponent(*userControl, "app:///UserControl.xaml");
```

`FrameworkElement::FindName()` performs WPF-style namescope lookup. Gui keeps
the complete XAML document alive until the returned root is mounted, so the
façade does not discard resources or deferred effects. Direct parsing,
compiled-document loading, hot reload, and schema/tooling work remain on the
advanced `Markup::XamlReader` surface.

## Custom controls

Custom-control **authors** include their WPF base type plus `Aero/Meta.hpp` (for `AERO_DECLARE_TYPE` metadata and `Meta::Register`). Application code that only uses `Button`, `Grid`, and `LoadXaml` should **not** include `Meta.hpp`.

```cpp
#include <Aero/Controls/Control.hpp>
#include <Aero/Meta.hpp>

class Rating : public Aero::Controls::Control {
    AERO_DECLARE_TYPE_NAMED(Rating, Aero::Controls::Control, "urn:demo", "Rating")

public:
    inline static constexpr Aero::DependencyProperty<double> ValueProperty{"Value"};

    double GetValue() const noexcept { return GetValueOr(ValueProperty, 0.0); }
    void SetValue(double value) noexcept { static_cast<void>(Aero::DependencyObject::SetValue(ValueProperty, value)); }
};
```

Metadata authoring uses the canonical `Meta::Registration` callback and
`Meta::Register<T>()` fluent entry:

```cpp
Aero::Result<void> RegisterDemoTypes(
    Aero::Meta::Registration& registration) noexcept {
    return Aero::Meta::Register<Rating>(registration)
        .Property(
            Rating::ValueProperty,
            Aero::Meta::FrameworkPropertyMetadata(
                0.0,
                Aero::Meta::AffectsRender))
        .Factory()
        .Result();
}
```

Effective-value providers, registry tables and render implementation details
remain private.

## Animation types

`<Aero/Media/Animation.hpp>` is an umbrella. Prefer the type header when you
only need one class, for example `<Aero/Media/Animation/Storyboard.hpp>` or
`<Aero/Media/Animation/DoubleAnimation.hpp>`. Do not include interactivity
headers from the animation umbrella; `EventTrigger` and `BeginStoryboard` live
in their own `Media/Animation/` headers. `Storyboard` derives from
`ParallelTimeline` / `TimelineGroup`. `EasingFunctionBase` is a `Freezable`.
Key frames share `KeyTime`, `EasingFunction`, and `KeySpline` on `KeyFrameBase`.

## One type per header

Kitchen-sink media/input/data headers are umbrellas or type owners:

- `<Aero/Media/Brushes.hpp>`, `<Aero/Media/Transforms.hpp>`,
  `<Aero/Media/Effects.hpp>` include `Media/<Type>.hpp`.
- `<Aero/Media/Geometry.hpp>` owns `Geometry`; path types live in
  `PathGeometry.hpp` / `StreamGeometry.hpp` / `PathFigure.hpp`. Segment and
  primitive geometry types (`BezierSegment`, `ArcSegment`, `LineGeometry`,
  `GeometryGroup`, …) each have their own `Media/<Type>.hpp`. Rendering
  `Flatten`s; `ToStreamData` is serialize/debug only.
- `<Aero/Input.hpp>` is input values (`Key`, `PointerInput`, `InputScope`).
  Commands are `<Aero/ICommand.hpp>` and `<Aero/RoutedCommand.hpp>`.
- `<Aero/Data/Binding.hpp>` owns `Binding`; `MultiBinding` and converters
  have sibling headers under `Data/`.
- `ComboBoxItem`, `MenuItem`, `ContextMenu`, `ListViewItem`, and GridView
  types have their own `Controls/` headers.

Custom markup extensions inherit `<Aero/Markup/MarkupExtension.hpp>` and
override `ProvideValue()`. Register the type with `Factory()` (and
`TypeFlags::MarkupExtension` when you want the XAML flag). TreeView hierarchy
uses `<Aero/HierarchicalDataTemplate.hpp>` rather than aliasing to
`DataTemplate`. `Line` / `Polygon` / `Polyline` live next to `Path` under
`Shapes/`. `FreezableCollection<T>` replaces handwritten `Vector<Ref<...>>`
on path figures, transform groups, and timeline groups.

## Transform3D (2.5D projective perspective)

`<Aero/Media/Transform3D.hpp>` is the Freezable base (no Animatable). Concrete
types: `CompositeTransform3D`, `PerspectiveTransform3D`, `MatrixTransform3D`.
Set `UIElement.Transform3D`. Storyboard paths such as
`(UIElement.Transform3D).(CompositeTransform3D.RotationY)` walk the existing
Freezable + DP path.

This is CPU-side 3×3 collapse onto the local Z=0 plane, not true 3D:

- No depth sort — overlapping siblings use `Panel.ZIndex`.
- No perspective-correct UV; texture mapping stays affine in 2D after collapse.
- Hit testing unprojects onto the local Z=0 plane (not a 3D ray).
- Shared vanishing point only under a `PerspectiveTransform3D` ancestor **or**
  the implicit view-root default `Depth=1000` (intentional UWP deviation: a
  lone `CompositeTransform3D RotationY=45` already shows perspective). Strict
  UWP without a perspective ancestor would be orthographic squash; AeroGUI
  does not do that.
- No `PlaneProjection` / `UIElement.Projection`.
- `GetLocalVisualTransform()` is local-only `ProjectiveTransform2D` (O(1);
  no ancestor walk). 3D accumulation lives on the render/hit tree walk.
- 2D-only trees keep the affine fast path (`LeavesZ0PlaneUnchanged`).

## Implicit DataTemplate

`ItemsControl::ResolveItemTemplate(item, index)` follows WPF order:

1. `ItemTemplateSelector.SelectTemplate`
2. `ItemTemplate`
3. implicit `DataTemplate` keyed by `ResourceKey::FromType(item.RuntimeType())`
   walking `BaseType` (`<DataTemplate DataType="local:Foo">`)
4. fallback: `DisplayMemberPath`, boxed value, or `item` as `UIElement`

Hierarchical `ItemsSource` / `ItemTemplate` live on
`HierarchicalDataTemplate`, not on `DataTemplate`.

## Duration, KeyTime, RepeatBehavior

`Timeline` timing properties are dependency properties with strong value
types in `<Aero/Media/Animation/Duration.hpp>`, `KeyTime.hpp`, and
`RepeatBehavior.hpp`. Theme XAML still parses `Duration="0:0:2"`,
`RepeatBehavior="2x"`, `KeyTime="50%"` / `Uniform` / `Paced`. Clock text
accepts `1.5`, `2s`, `500ms`, `M:S`, and `H:M:S`. `"2"` is a repeat count,
not two seconds.

## CollectionView vs SelectedItem

`ItemsControl` wraps a non-view `ItemsSource` in a cached
`CollectionViewSource::GetDefaultView`. Sort/filter project index space
through `GetCount` / `GetItem`; filtered-out items return `UINT32_MAX` from
`Selector::GetIndexOfItem`. `Selector.SelectedItem` remains selection
authority. `CollectionView.CurrentItem` is view currency (`MoveCurrentTo*`).
`Selector.IsSynchronizedWithCurrentItem` (default `false`) is the WPF opt-in
to keep those two in sync. Grouping is not implemented.

## Path stroke, FillRule, and geometry Flatten

`Path` honors `StrokeLineJoin` / `StrokeStartLineCap` / `StrokeEndLineCap`
and `FillRule` during tessellation. Object-model geometry
(`PathGeometry`, `LineGeometry`, `BezierSegment`, `ArcSegment`, …) renders
by `Geometry::Flatten` / `PathSegment::Flatten`, not by stringifying through
`PathGeometry::ToStreamData` (that API remains for serialize/debug and still
rejects non-`LineSegment` content). Mini-language `A` and `ArcSegment` share
one arc-to-bezier implementation. `Geometry.Transform` is applied while
flattening.

## Follow-ups

Not in this pass: `CollectionView` grouping, `UIElement.Clip` geometry
stencil, libtess2 (boolean `CombinedGeometry` / robust fill), and
Matrix/Point3D animation families.


