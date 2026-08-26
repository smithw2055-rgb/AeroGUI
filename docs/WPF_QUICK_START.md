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
