# AeroGUI for WPF and NoesisGUI developers

AeroGUI keeps WPF/XAML names and semantics while using ordinary C++17 methods.
The default desktop application entry point is `Aero::App::Run()`; engine and
custom native hosts use `Aero::Integration` explicitly.

## Application and Window

```cpp
#include <Aero/App.hpp>

class App final : public Aero::Application {
    AERO_DECLARE_TYPE_NAMED(App, Aero::Application, "urn:demo", "App")

protected:
    void OnStartup(Aero::StartupEventArgs& args) noexcept override {
        Aero::Application::OnStartup(args);
    }
};

int main() {
    return Aero::App::Run();
}
```

`App.xaml` and `StartupUri` select the application and main-window types. Include
`<Aero/App/Launcher.hpp>` only when backend selection, allocator injection or
module registration must be controlled by the native host.

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
        args.handled = true;
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

## Custom controls

Custom controls normally need only `Aero/Gui.hpp` and `Aero/Meta.hpp`:

```cpp
class Rating final : public Aero::Controls::Control {
    AERO_DECLARE_TYPE_NAMED(Rating, Aero::Controls::Control, "urn:demo", "Rating")

public:
    inline static constexpr Members::Property<double> ValueProperty{"Value"};

    double GetValue() const noexcept { return GetValueOr(ValueProperty, 0.0); }
    void SetValue(double value) noexcept { static_cast<void>(Aero::DependencyObject::SetValue(ValueProperty, value)); }
};
```

Metadata authoring uses `FrameworkPropertyMetadata` and keeps effective-value,
provider and render implementation details out of the control declaration.
