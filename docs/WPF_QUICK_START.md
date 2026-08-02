# AeroGUI for WPF and NoesisGUI developers

AeroGUI keeps WPF/XAML names and semantics while using ordinary C++17 methods.
Code-first desktop applications call `Application::Run()`; generated XAML
bootstrap code may call `App::Run()` to load `App.xaml`. Engine and custom
native hosts use `Aero::Integration` explicitly.

## Application and Window

```cpp
#include <Aero/App.hpp>

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
    return app.Run();
}
```

For a generated XAML application entry point, `Aero::App::Run()` still loads
`App.xaml`, reads its `StartupUri`, and runs the same private desktop host.
Backend, allocator, diagnostics and initial-window options are supplied through
`Aero::App::RunOptions`; no public launcher or host object is required.

## Properties

WPF CLR-property syntax maps directly to `GetXxx()` and `SetXxx()`:

```cpp
button->SetWidth(120.0);
button->SetHeight(36.0);
button->SetIsEnabled(true);

double width = button->GetWidth();
bool enabled = button->GetIsEnabled();
```

Attached properties keep their WPF shape:

```cpp
Aero::Controls::Grid::SetRow(*button, 1);
Aero::Controls::Grid::SetColumn(*button, 2);
```

Dependency-property and routed-event identifiers are declared on one physical
line:

```cpp
inline static constexpr Members::Property<double> RatingProperty{"Rating"};
inline static constexpr Members::RoutedEvent<Aero::RoutedEventArgs> RatingChangedEvent{"RatingChanged"};
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

Engine hosts load XAML explicitly and keep `View` focused on content, input and
frame updates:

```cpp
Aero::Gui environment;
environment.Initialize();

auto view = environment.CreateView(options).Value();
Aero::Markup::XamlReader reader(*view);
auto document = reader.Load("app:///MainView.xaml").Value();
view->SetContent(std::move(document), {1280.0, 720.0});
view->Update(elapsedMilliseconds);
```

`FrameworkElement::FindName()` performs WPF-style namescope lookup. XAML
providers, compiled-document loading and parsing remain on `XamlReader`; View
does not expose a second loader API.

## Custom controls

Custom controls normally need only `Aero/Gui.hpp` and `Aero/Meta.hpp`:

```cpp
class Rating : public Aero::Controls::Control {
    AERO_DECLARE_TYPE_NAMED(Rating, Aero::Controls::Control, "urn:demo", "Rating")

public:
    inline static constexpr Members::Property<double> ValueProperty{"Value"};

    double GetValue() const noexcept { return GetValueOr(ValueProperty, 0.0); }
    void SetValue(double value) noexcept { static_cast<void>(Aero::DependencyObject::SetValue(ValueProperty, value)); }
};
```

Metadata authoring uses the canonical `Meta::Registration` callback and
`Meta::Describe<T>()` fluent entry:

```cpp
Aero::Base::Result<void> RegisterDemoTypes(
    Aero::Meta::Registration& registration) noexcept {
    return Aero::Meta::Describe<Rating>(registration)
        .Property(
            Rating::ValueProperty,
            Aero::Meta::FrameworkPropertyMetadata(
                0.0,
                Aero::Meta::FrameworkPropertyMetadataOptions::AffectsRender))
        .Factory()
        .Result();
}
```

Effective-value providers, registry tables and render implementation details
remain private.
