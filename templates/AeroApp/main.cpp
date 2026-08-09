#include <AeroApp/App.hpp>
#include <Aero/Controls/Button.hpp>
#include <Aero/Meta.hpp>

namespace Sample {

class MainWindow final : public Aero::Window {
    AERO_DECLARE_TYPE_NAMED(
        MainWindow,
        Aero::Window,
        "clr-namespace:Sample",
        "MainWindow")

public:
    MainWindow() noexcept
        : Aero::Window(StaticTypeId()) {
        InitializeComponent();
    }

    static void DescribeComponent(
        Aero::Meta::TypeBuilder<MainWindow>& type) noexcept {
        type.EventHandler<
            Aero::RoutedEventArgs,
            &MainWindow::OnHelloClick>("OnHelloClick");
    }

private:
    void OnHelloClick(
        Aero::Base::Object*,
        const Aero::RoutedEventArgs&) noexcept {
        if (auto* button = FindName<Aero::Controls::Button>("HelloButton")) {
            auto text = Aero::Meta::Value::TryFromString(
                Aero::Meta::TypeOf<Aero::String>(),
                "Hello from AeroGUI");
            if (text) {
                button->SetContent(std::move(text).Value());
            }
        }
    }
};

class App final : public Aero::Application {
    AERO_DECLARE_TYPE_NAMED(
        App,
        Aero::Application,
        "clr-namespace:Sample",
        "App")

public:
    App() noexcept
        : Aero::Application(StaticTypeId()) {}
};

} // namespace Sample

int main() {
    const Aero::ModuleRegistration components =
        Aero::DefineComponentModule<Sample::App, Sample::MainWindow>(
            "Sample.Components");
    const Aero::ModuleRegistration modules[] = {components};
    Aero::App::RunOptions options;
    options.modules = modules;
    Sample::App app;
    Aero::Result<int> result = app.Run(options);
    return result ? result.Value() : -1;
}
