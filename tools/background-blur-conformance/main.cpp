#include <Aero/Controls.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Gui.hpp>
#include <Aero/Input.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Triggers/BlendBehaviors.hpp>
#include <Aero/View.hpp>
#include <AeroApp/Window.hpp>

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace {

class FileStream final : public Aero::Base::Stream {
public:
    FileStream(std::FILE* file, std::uint64_t length) noexcept
        : file_(file), length_(length) {}
    ~FileStream() override {
        if (file_ != nullptr) std::fclose(file_);
    }
    bool CanRead() const noexcept override { return file_ != nullptr; }
    bool CanSeek() const noexcept override { return file_ != nullptr; }
    Aero::Base::Result<std::uint32_t> Read(
        Aero::Base::Span<std::uint8_t> destination) noexcept override {
        if (file_ == nullptr) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InvalidState,
                "BackgroundBlur file stream is closed");
        }
        const std::size_t read = std::fread(
            destination.Data(), 1U, destination.Size(), file_);
        if (read == 0U && std::ferror(file_) != 0) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InternalError,
                "BackgroundBlur file read failed");
        }
        return static_cast<std::uint32_t>(read);
    }
    Aero::Base::Result<std::uint64_t> Position() const noexcept override {
        const long value = file_ != nullptr ? std::ftell(file_) : -1L;
        return value >= 0
            ? Aero::Base::Result<std::uint64_t>(
                  static_cast<std::uint64_t>(value))
            : Aero::Base::Result<std::uint64_t>(
                  Aero::Base::Status::Failure(
                      Aero::Base::ErrorCode::InternalError,
                      "BackgroundBlur file position failed"));
    }
    Aero::Base::Result<std::uint64_t> Length() const noexcept override {
        return length_;
    }
    Aero::Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Aero::Base::SeekOrigin origin) noexcept override {
        const int nativeOrigin = origin == Aero::Base::SeekOrigin::Begin
            ? SEEK_SET
            : origin == Aero::Base::SeekOrigin::Current ? SEEK_CUR : SEEK_END;
        if (file_ == nullptr || offset < LONG_MIN || offset > LONG_MAX ||
            std::fseek(file_, static_cast<long>(offset), nativeOrigin) != 0) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InternalError,
                "BackgroundBlur file seek failed");
        }
        return Position();
    }
private:
    std::FILE* file_ = nullptr;
    std::uint64_t length_ = 0U;
};

std::string ToString(Aero::Base::StringView value) {
    return value.Data() == nullptr
        ? std::string{}
        : std::string(value.Data(), value.SizeBytes());
}

std::string ComponentPath(Aero::Base::StringView path) {
    std::string value = ToString(path);
    constexpr const char* marker = ";component/";
    const std::size_t found = value.find(marker);
    if (found != std::string::npos) {
        value.erase(0U, found + std::strlen(marker));
    }
    while (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }
    return value;
}

class BackgroundBlurXamlProvider final
    : public ::Aero::Markup::XamlProvider {
public:
    explicit BackgroundBlurXamlProvider(std::string root) noexcept
        : root_(std::move(root)) {}

    Aero::Base::Result<::Aero::Markup::StreamResourceInfo> Open(
        const Aero::Base::ResourceUri& uri) const noexcept override {
        if (!uri.Assembly().Empty() &&
            uri.Assembly() != Aero::Base::StringView("BackgroundBlur")) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::NotFound,
                "BackgroundBlur assembly route was not found");
        }
        std::string path = root_;
        if (!path.empty() && path.back() != '/') path.push_back('/');
        path += ComponentPath(uri.Path());
        std::FILE* file = std::fopen(path.c_str(), "rb");
        if (file == nullptr) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::NotFound,
                "BackgroundBlur resource was not found");
        }
        if (std::fseek(file, 0L, SEEK_END) != 0) {
            std::fclose(file);
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InternalError,
                "BackgroundBlur file length failed");
        }
        const long length = std::ftell(file);
        if (length < 0 || std::fseek(file, 0L, SEEK_SET) != 0) {
            std::fclose(file);
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InternalError,
                "BackgroundBlur file rewind failed");
        }
        Aero::Base::Result<Aero::Base::Ref<FileStream>> stream =
            Aero::Base::MakeRef<FileStream>(
                file, static_cast<std::uint64_t>(length));
        if (!stream) {
            std::fclose(file);
            return stream.GetStatus();
        }
        ::Aero::Markup::StreamResourceInfo info;
        info.uri = uri;
        info.stream = Aero::Base::Ref<Aero::Base::Stream>(
            std::move(stream).Value());
        info.revision = 1U;
        return info;
    }

    std::uint64_t CacheIdentity() const noexcept override {
        return UINT64_C(0x4241434B424C5552);
    }
private:
    std::string root_;
};

void PrintDiagnostics(const Aero::Diagnostics::DiagnosticBag& diagnostics) {
    for (const Aero::Diagnostics::Diagnostic& item : diagnostics.Items()) {
        std::fprintf(
            stderr,
            "  0x%08x %u:%u %.*s\n",
            item.Code().value,
            item.Source().begin.line,
            item.Source().begin.column,
            static_cast<int>(item.Message().SizeBytes()),
            item.Message().Data());
    }
}

Aero::Base::Point RootCenter(
    const Aero::UIElement& element,
    const Aero::Media::Visual& root) noexcept {
    Aero::Base::Point point{
        element.GetRenderSize().width * 0.5,
        element.GetRenderSize().height * 0.5};
    const Aero::Media::Visual* current = &element;
    while (current != nullptr) {
        const Aero::FrameworkElement* framework =
            current->AsFrameworkElement();
        if (framework != nullptr) {
            point = Aero::Media::TransformPoint(
                framework->GetLocalVisualTransform(), point);
        }
        const Aero::UIElement* currentElement = current->AsUIElement();
        if (currentElement != nullptr) {
            const Aero::Rect slot = currentElement->GetLayoutSlot();
            point.x += slot.x;
            point.y += slot.y;
        }
        if (current == &root) break;
        current = current->GetVisualParent();
    }
    return point;
}

Aero::Controls::Button* FindButton(Aero::Media::Visual& root) noexcept {
    if (root.RuntimeType() == Aero::Controls::Button::StaticTypeId()) {
        return static_cast<Aero::Controls::Button*>(&root);
    }
    const std::uint32_t count =
        Aero::Media::VisualTreeHelper::GetChildrenCount(root);
    for (std::uint32_t index = 0U; index < count; ++index) {
        Aero::Media::Visual* child = Aero::Media::VisualTreeHelper::GetChild(root, index);
        if (child == nullptr) continue;
        Aero::Controls::Button* found = FindButton(*child);
        if (found != nullptr) return found;
    }
    return nullptr;
}

void CollectRepeatButtons(
    Aero::Media::Visual& root,
    Aero::Controls::Primitives::RepeatButton** output,
    std::uint32_t capacity,
    std::uint32_t& count) noexcept {
    if (count >= capacity) return;
    if (root.RuntimeType() == Aero::Controls::Primitives::RepeatButton::StaticTypeId()) {
        output[count++] = static_cast<Aero::Controls::Primitives::RepeatButton*>(&root);
        if (count >= capacity) return;
    }
    const std::uint32_t childCount =
        Aero::Media::VisualTreeHelper::GetChildrenCount(root);
    for (std::uint32_t index = 0U;
         index < childCount && count < capacity;
         ++index) {
        Aero::Media::Visual* child =
            Aero::Media::VisualTreeHelper::GetChild(root, index);
        if (child != nullptr) {
            CollectRepeatButtons(*child, output, capacity, count);
        }
    }
}

bool Near(double left, double right, double epsilon = 0.05) noexcept {
    return std::fabs(left - right) <= epsilon;
}

void AdvanceView(
    Aero::View& view,
    double& timeInSeconds,
    std::uint32_t elapsedMilliseconds) noexcept {
    timeInSeconds += static_cast<double>(elapsedMilliseconds) / 1000.0;
    view.Update(timeInSeconds);
}

bool VerifyBackgroundProjection(Aero::FrameworkElement& root) {
    using namespace Aero;
    auto* source = root.FindName<Controls::Border>("source");
    auto* top = root.FindName<Shapes::Path>("TopPanelBlur");
    auto* bottom = root.FindName<Shapes::Path>("BottomPanelBlur");
    auto* topSlider = root.FindName<Controls::Slider>("TopBlurRadius");
    auto* bottomSlider = root.FindName<Controls::Slider>("BottomBlurRadius");
    if (source == nullptr || top == nullptr || bottom == nullptr ||
        topSlider == nullptr || bottomSlider == nullptr) {
        std::fprintf(stderr, "BACKGROUND FAIL: required named elements are missing\n");
        return false;
    }
    Base::Ref<Media::Brush> sourceBrush = source->GetBackground();
    Base::Ref<Media::Brush> topFill = top->GetFill();
    Base::Ref<Media::Brush> bottomFill = bottom->GetFill();
    Base::Ref<Media::Effect> topEffect = top->GetEffect();
    Base::Ref<Media::Effect> bottomEffect = bottom->GetEffect();
    if (!sourceBrush || !topFill || !bottomFill ||
        sourceBrush->RuntimeType() != Media::ImageBrush::StaticTypeId() ||
        topFill->RuntimeType() != Media::ImageBrush::StaticTypeId() ||
        bottomFill->RuntimeType() != Media::ImageBrush::StaticTypeId() ||
        !topEffect || !bottomEffect ||
        topEffect->RuntimeType() != Media::BlurEffect::StaticTypeId() ||
        bottomEffect->RuntimeType() != Media::BlurEffect::StaticTypeId()) {
        std::fprintf(stderr, "BACKGROUND FAIL: projected brush/effect types are incorrect\n");
        return false;
    }
    const auto& topImage = static_cast<const Media::ImageBrush&>(*topFill);
    const auto& bottomImage = static_cast<const Media::ImageBrush&>(*bottomFill);
    const Rect topViewbox = topImage.GetViewbox();
    const Rect bottomViewbox = bottomImage.GetViewbox();
    if (topViewbox.width <= 0.0 || topViewbox.height <= 0.0 ||
        bottomViewbox.width <= 0.0 || bottomViewbox.height <= 0.0 ||
        (Near(topViewbox.x, bottomViewbox.x, 1.0e-5) &&
         Near(topViewbox.y, bottomViewbox.y, 1.0e-5) &&
         Near(topViewbox.width, bottomViewbox.width, 1.0e-5) &&
         Near(topViewbox.height, bottomViewbox.height, 1.0e-5))) {
        std::fprintf(stderr,
            "BACKGROUND FAIL: projected viewboxes are invalid top=(%.3f %.3f %.3f %.3f) bottom=(%.3f %.3f %.3f %.3f)\n",
            topViewbox.x, topViewbox.y, topViewbox.width, topViewbox.height,
            bottomViewbox.x, bottomViewbox.y,
            bottomViewbox.width, bottomViewbox.height);
        return false;
    }
    const auto& topBlur = static_cast<const Media::BlurEffect&>(*topEffect);
    const auto& bottomBlur = static_cast<const Media::BlurEffect&>(*bottomEffect);
    if (!Near(topBlur.GetRadius(), topSlider->GetValue()) ||
        !Near(bottomBlur.GetRadius(), bottomSlider->GetValue())) {
        std::fprintf(stderr,
            "BACKGROUND FAIL: blur bindings are incorrect top=%.3f/%.3f bottom=%.3f/%.3f\n",
            topBlur.GetRadius(), topSlider->GetValue(),
            bottomBlur.GetRadius(), bottomSlider->GetValue());
        return false;
    }
    return true;
}

bool VerifySliderBinding(
    Aero::View& view,
    double& timeInSeconds,
    Aero::FrameworkElement& root) {
    using namespace Aero;
    auto* top = root.FindName<Shapes::Path>("TopPanelBlur");
    auto* slider = root.FindName<Controls::Slider>("TopBlurRadius");
    if (top == nullptr || slider == nullptr) return false;
    slider->SetValue(37.0);
    AdvanceView(view, timeInSeconds, 16U);
    Base::Ref<Media::Effect> effect = top->GetEffect();
    if (!effect ||
        effect->RuntimeType() != Media::BlurEffect::StaticTypeId() ||
        !Near(static_cast<Media::BlurEffect&>(*effect).GetRadius(), 37.0)) {
        std::fprintf(stderr, "BINDING FAIL: Slider did not update BlurEffect.Radius\n");
        return false;
    }
    return true;
}

bool VerifySliderCommands(
    Aero::View& view,
    double& timeInSeconds,
    Aero::FrameworkElement& root) {
    using namespace Aero;
    auto* slider = root.FindName<Controls::Slider>("TopBlurRadius");
    if (slider == nullptr) {
        std::fprintf(stderr, "COMMAND FAIL: TopBlurRadius is missing\n");
        return false;
    }
    Controls::Primitives::RepeatButton* buttons[2]{};
    std::uint32_t count = 0U;
    CollectRepeatButtons(*slider, buttons, 2U, count);
    if (count != 2U || buttons[0] == nullptr || buttons[1] == nullptr) {
        std::fprintf(stderr, "COMMAND FAIL: Slider repeat buttons were not realized\n");
        return false;
    }
    const double initialValue =
        (slider->GetMinimum() + slider->GetMaximum()) * 0.5;
    slider->SetValue(initialValue);
    AdvanceView(view, timeInSeconds, 16U);
    const double initial = slider->GetValue();
    const Value parameter = Value::NullObject(Meta::TypeOf<Base::Object>());
    auto execute = [&](Controls::Primitives::RepeatButton& button) noexcept {
        Input::ICommand* command = button.GetCommand();
        if (command == nullptr || command->RuntimeType() !=
                Input::RoutedCommand::StaticTypeId()) {
            return false;
        }
        Base::Result<bool> canExecute = command->CanExecute(parameter, slider);
        if (!canExecute || !canExecute.Value()) return false;
        command->Execute(parameter, slider);
        AdvanceView(view, timeInSeconds, 16U);
        return true;
    };
    if (!execute(*buttons[0])) {
        std::fprintf(stderr, "COMMAND FAIL: first Slider routed command failed\n");
        return false;
    }
    const double first = slider->GetValue();
    slider->SetValue(initial);
    AdvanceView(view, timeInSeconds, 16U);
    if (!execute(*buttons[1])) {
        std::fprintf(stderr, "COMMAND FAIL: second Slider routed command failed\n");
        return false;
    }
    const double second = slider->GetValue();
    const bool opposite =
        (first < initial && second > initial) ||
        (first > initial && second < initial);
    const double expected = slider->GetLargeChange();
    if (!opposite || !Near(std::fabs(first - initial), expected, 0.1) ||
        !Near(std::fabs(second - initial), expected, 0.1)) {
        std::fprintf(stderr,
            "COMMAND FAIL: Slider routed commands did not apply LargeChange "
            "initial=%.2f first=%.2f second=%.2f large=%.2f\n",
            initial, first, second, expected);
        return false;
    }
    slider->SetValue(37.0);
    AdvanceView(view, timeInSeconds, 16U);
    return true;
}

bool VerifyDrag(
    Aero::View& view,
    double& timeInSeconds,
    Aero::FrameworkElement& root) {
    using namespace Aero;
    auto* panel = root.FindName<Controls::Viewbox>("Panel");
    if (panel == nullptr) {
        std::fprintf(stderr, "DRAG FAIL: Panel is missing\n");
        return false;
    }
    const Base::Transform2D before = panel->GetLocalVisualTransform();
    const Base::Point start = RootCenter(*panel, root);
    const int startX = static_cast<int>(start.x);
    const int startY = static_cast<int>(start.y);
    const int endX = static_cast<int>(start.x + 45.0);
    const int endY = static_cast<int>(start.y + 28.0);
    bool dispatched = view.MouseButtonDown(
        startX, startY, Input::MouseButton::Left);
    if (dispatched) dispatched = view.MouseMove(endX, endY);
    if (dispatched) dispatched = view.MouseButtonUp(
        endX, endY, Input::MouseButton::Left);
    AdvanceView(view, timeInSeconds, 16U);
    const Base::Transform2D after = panel->GetLocalVisualTransform();
    if (!dispatched ||
        (Near(before.dx, after.dx, 0.5) && Near(before.dy, after.dy, 0.5))) {
        std::fprintf(stderr,
            "DRAG FAIL: panel transform did not move before=(%.2f %.2f) after=(%.2f %.2f)\n",
            before.dx, before.dy, after.dx, after.dy);
        return false;
    }
    const UIElement* parent = panel->GetVisualParent() != nullptr
        ? panel->GetVisualParent()->AsUIElement()
        : nullptr;
    if (parent != nullptr) {
        const Rect panelBounds = Media::TransformBounds(
            panel->GetLocalVisualTransform(),
            {panel->GetLayoutSlot().x, panel->GetLayoutSlot().y,
             panel->GetRenderSize().width, panel->GetRenderSize().height});
        const Size parentSize = parent->GetRenderSize();
        if (panelBounds.x < -1.0 || panelBounds.y < -1.0 ||
            panelBounds.x + panelBounds.width > parentSize.width + 1.0 ||
            panelBounds.y + panelBounds.height > parentSize.height + 1.0) {
            std::fprintf(stderr, "DRAG FAIL: parent-bound constraint was violated\n");
            return false;
        }
    }
    return true;
}

struct ClickProbe {
    std::uint32_t count = 0U;
    void OnClick(Aero::Base::Object*, Aero::RoutedEventArgs&) noexcept {
        ++count;
    }
};

bool VerifyStoryboard(
    Aero::View& view,
    double& timeInSeconds,
    Aero::FrameworkElement& root) {
    using namespace Aero;
    auto* buttonsGrid = root.FindName<Controls::Grid>("ButtonsGrid");
    auto* panelGrid = root.FindName<Controls::Grid>("PanelGrid");
    if (buttonsGrid == nullptr || panelGrid == nullptr) {
        std::fprintf(stderr, "STORYBOARD FAIL: required named elements are missing\n");
        return false;
    }
    Controls::Button* button = FindButton(*buttonsGrid);
    if (button == nullptr) {
        std::fprintf(stderr, "STORYBOARD FAIL: button was not found\n");
        return false;
    }
    ClickProbe probe;
    RoutedEventHandler clickHandler(&probe, &ClickProbe::OnClick);
    button->Click().Add(clickHandler);
    const Base::Point point = RootCenter(*button, root);
    const int x = static_cast<int>(point.x);
    const int y = static_cast<int>(point.y);
    bool dispatched = view.MouseButtonDown(
        x, y, Input::MouseButton::Left);
    if (dispatched) dispatched = view.MouseButtonUp(
        x, y, Input::MouseButton::Left);
    AdvanceView(view, timeInSeconds, 350U);
    Base::Ref<Media::Transform> transform = panelGrid->GetRenderTransform();
    if (!dispatched || probe.count != 1U || !transform ||
        transform->RuntimeType() != Media::ScaleTransform::StaticTypeId()) {
        std::fprintf(stderr, "STORYBOARD FAIL: click did not start AnimClose\n");
        return false;
    }
    auto& scale = static_cast<Media::ScaleTransform&>(*transform);
    if (scale.GetScaleX() > 0.15 || scale.GetScaleY() > 0.15) {
        std::fprintf(stderr,
            "STORYBOARD FAIL: close animation did not reach collapsed sample (%.3f %.3f) click=%u\n",
            scale.GetScaleX(), scale.GetScaleY(), probe.count);
        return false;
    }
    AdvanceView(view, timeInSeconds, 1100U);
    if (!Near(scale.GetScaleX(), 1.0, 0.1) ||
        !Near(scale.GetScaleY(), 1.0, 0.1)) {
        std::fprintf(stderr,
            "STORYBOARD FAIL: close animation did not return to one (%.3f %.3f)\n",
            scale.GetScaleX(), scale.GetScaleY());
        return false;
    }
    return true;
}

bool Run(Aero::Gui& gui) {
    Aero::Markup::XamlReader reader(gui);
    Aero::Diagnostics::DiagnosticBag diagnostics;
    Aero::Base::Result<Aero::Markup::XamlDocument> resourceDocument =
        reader.Load(
            "pack://application:,,,/BackgroundBlur;component/Resources.xaml",
            {},
            &diagnostics);
    if (!resourceDocument) {
        std::fprintf(stderr, "RESOURCES FAIL: %s\n",
            resourceDocument.GetStatus().message);
        PrintDiagnostics(diagnostics);
        return false;
    }
    Aero::ResourceDictionary* resources =
        resourceDocument.Value().Root<Aero::ResourceDictionary>();
    if (resources == nullptr) {
        std::fprintf(stderr,
            "RESOURCES FAIL: resource root is not ResourceDictionary\n");
        return false;
    }
    ::Aero::ViewOptions options;
    options.automaticAnimationClock = false;
    options.applicationResources = resources;
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> made =
        gui.CreateView(options);
    if (!made) {
        std::fprintf(stderr, "VIEW FAIL: %s\n", made.GetStatus().message);
        return false;
    }
    Aero::Base::Ref<Aero::View> view = std::move(made).Value();
    diagnostics.Clear();
    Aero::Base::Result<Aero::Markup::XamlDocument> loaded = reader.Load(
        "pack://application:,,,/BackgroundBlur;component/MainWindow.xaml",
        {}, &diagnostics);
    if (!loaded) {
        std::fprintf(stderr, "LOAD FAIL: %s\n", loaded.GetStatus().message);
        PrintDiagnostics(diagnostics);
        return false;
    }
    Aero::FrameworkElement* root =
        loaded.Value().Root<Aero::FrameworkElement>();
    if (root == nullptr) {
        std::fprintf(stderr, "LOAD FAIL: MainWindow root is not FrameworkElement\n");
        return false;
    }
    Aero::Base::Result<void> mounted = view->SetContent(
        std::move(loaded).Value(), {1280.0, 720.0});
    if (!mounted) {
        std::fprintf(stderr, "MOUNT FAIL: %s\n", mounted.GetStatus().message);
        return false;
    }
    double timeInSeconds = 0.0;
    view->Update(timeInSeconds);
    AdvanceView(*view, timeInSeconds, 16U);
    if (!VerifyBackgroundProjection(*root) ||
        !VerifySliderBinding(*view, timeInSeconds, *root) ||
        !VerifySliderCommands(*view, timeInSeconds, *root) ||
        !VerifyStoryboard(*view, timeInSeconds, *root) ||
        !VerifyDrag(*view, timeInSeconds, *root)) {
        return false;
    }
    std::printf("LOAD OK MainWindow.xaml\n");
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr,
            "usage: aero-background-blur-conformance <background-blur-root>\n");
        return 2;
    }
    BackgroundBlurXamlProvider provider(argv[1]);
    Aero::Gui gui;
    Aero::Base::Result<void> initialized =
        gui.AddXamlProvider(provider, "pack", "BackgroundBlur");
    if (initialized) initialized = gui.Initialize();
    if (!initialized) {
        std::fprintf(stderr, "INITIALIZATION FAIL: %s\n",
            initialized.GetStatus().message);
        return 1;
    }
    return Run(gui) ? 0 : 1;
}
