#include "GalleryRuntime.hpp"
#include "StatusBadge.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/RuntimeEnvironment.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Platform/Ime.hpp>
#include <Aero/Presentation/Binding.hpp>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Aero::Samples::ControlGallery {
namespace {

using namespace Base;
using namespace Controls;
using namespace Core;
using namespace Markup;
using namespace Presentation;

Status Failure(const char* message) noexcept {
    return Status::Failure(
        ErrorCode::InvalidState, message);
}

Base::Result<void> ApplyTextLayoutServiceToSubtree(
    Presentation::Visual& node,
    Controls::ITextBlockLayoutService& service,
    bool refreshExisting) noexcept {
    if (auto* element = node.AsUIElement()) {
        if (element->RuntimeType() == Controls::TextBlock::StaticTypeId()) {
            Controls::TextBlock* text =
                static_cast<Controls::TextBlock*>(element);
            if (refreshExisting) {
                Result<void> cleared = text->SetLayoutService(nullptr);
                if (!cleared) return cleared.GetStatus();
            }
            if (refreshExisting || text->LayoutService() != &service) {
                Result<void> assigned = text->SetLayoutService(&service);
                if (!assigned) return assigned.GetStatus();
            }
        } else if (element->RuntimeType() == Controls::TextBox::StaticTypeId()) {
            Controls::TextBox* text =
                static_cast<Controls::TextBox*>(element);
            if (refreshExisting) {
                Result<void> cleared = text->SetLayoutService(nullptr);
                if (!cleared) return cleared.GetStatus();
            }
            if (refreshExisting || text->LayoutService() != &service) {
                Result<void> assigned = text->SetLayoutService(&service);
                if (!assigned) return assigned.GetStatus();
            }
        }
    }
    for (Presentation::Visual* child : node.VisualChildren()) {
        if (child != nullptr) {
            Result<void> assigned = ApplyTextLayoutServiceToSubtree(
                *child, service, refreshExisting);
            if (!assigned) return assigned.GetStatus();
        }
    }
    return {};
}

bool ReadFile(
    const std::string& path,
    std::vector<std::uint8_t>& output) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length < 0 ||
        static_cast<std::uint64_t>(length) > UINT32_MAX) {
        return false;
    }
    stream.seekg(0, std::ios::beg);
    output.resize(static_cast<std::size_t>(length));
    return output.empty() || static_cast<bool>(
        stream.read(
            reinterpret_cast<char*>(output.data()),
            static_cast<std::streamsize>(length)));
}

class GalleryItem final : public Object {
    AERO_TYPED_META_NAMED(
        GalleryItem,
        Object,
        "urn:aero-control-gallery",
        "GalleryItem")
public:
    GalleryItem() noexcept = default;
};

class GalleryItemsSource final : public IItemsSource {
public:
    Result<void> Initialize() noexcept {
        Result<Ref<GalleryItem>> made =
            MakeRef<GalleryItem>();
        if (!made) return made.GetStatus();
        item_ = Ref<Object>(std::move(made).Value());
        return {};
    }

    std::uint32_t Count() const noexcept override {
        return 10000U;
    }

    Ref<Object> ItemAt(
        std::uint32_t index) const noexcept override {
        return index < Count()
            ? item_ : Ref<Object>();
    }

    Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.TryAdd(handler);
    }

    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }

private:
    Ref<Object> item_;
    ItemsChangedHandler changed_;
};

Result<Ref<Object>> MakeVirtualizedItem(
    const Ref<Object>&,
    void*) noexcept {
    Result<Ref<TextBlock>> made = MakeRef<TextBlock>();
    if (!made) return made.GetStatus();
    Ref<TextBlock> text = std::move(made).Value();
    Result<void> configured =
        text->SetText("Virtualized gallery item");
    if (configured) configured = text->SetHeight(24.0);
    if (configured) configured = text->SetWidth(340.0);
    if (!configured) return configured.GetStatus();
    return Ref<Object>(std::move(text));
}

double EventScale(
    const Platform::WindowEvent& event) noexcept {
    return std::isfinite(event.dpiScale) &&
        event.dpiScale > 0.0
        ? event.dpiScale
        : 1.0;
}

MouseButton MapButton(
    Platform::WindowPointerButton button) noexcept {
    switch (button) {
    case Platform::WindowPointerButton::Right:
        return MouseButton::Right;
    case Platform::WindowPointerButton::Middle:
        return MouseButton::Middle;
    case Platform::WindowPointerButton::XButton1:
        return MouseButton::XButton1;
    case Platform::WindowPointerButton::XButton2:
        return MouseButton::XButton2;
    case Platform::WindowPointerButton::Unknown:
    case Platform::WindowPointerButton::Left:
    default:
        return MouseButton::Left;
    }
}

} // namespace

struct GalleryRuntime::Impl final {
    NullRenderBackend nullBackend;
#if defined(_WIN32)
    Platform::Win32Clipboard clipboard;
    Platform::Win32ImeAdapter inputMethod;
#else
    Platform::MemoryClipboard clipboard;
#endif
    RuntimeEnvironment environment;
    Ref<RuntimeView> view;
    Ref<Object> root;
    GalleryItemsSource items;
    DataTemplate itemTemplate{
        &MakeVirtualizedItem, nullptr};
    std::unique_ptr<ItemContainerGenerator> generator;
    ListBox* listBox = nullptr;
    VirtualizingStackPanel* virtualizingPanel = nullptr;
    GallerySnapshot snapshot;
    Controls::ITextBlockLayoutService* textLayoutService = nullptr;

    ~Impl() { Cleanup(); }

    RuntimeHost& Runtime() const noexcept {
        return view->Host();
    }

    MetadataDomain& Metadata() noexcept {
        return *Runtime().Metadata();
    }

    Result<void> LoadDocument(
        const std::string& assetDirectory,
        GalleryLoadMode mode) noexcept {
        std::vector<std::uint8_t> source;
        if (mode == GalleryLoadMode::Runtime) {
            if (!ReadFile(
                    assetDirectory +
                    "/ControlGallery.xaml",
                    source)) {
                return Failure(
                    "ControlGallery runtime XAML is unavailable");
            }
            DiagnosticBag diagnostics;
            Result<Ref<Object>> loaded =
                Runtime().ParseXaml({
                    reinterpret_cast<const char*>(
                        source.data()),
                    static_cast<std::uint32_t>(
                        source.size())},
                    {},
                    &diagnostics);
            if (!loaded) return loaded.GetStatus();
            root = std::move(loaded).Value();
            return {};
        }

        if (!ReadFile(
                assetDirectory +
                "/ControlGallery.axir",
                source)) {
            return Failure(
                "ControlGallery compiled XAML is unavailable");
        }
        Result<Ref<Object>> loaded =
            Runtime().LoadCompiledXaml({
                source.data(),
                static_cast<std::uint32_t>(
                    source.size())});
        if (!loaded) return loaded.GetStatus();
        root = std::move(loaded).Value();
        return {};
    }

    Result<void> LoadTheme(
        const std::string&,
        GalleryTheme requested) noexcept {
        return Runtime().LoadBuiltInTheme(
            requested == GalleryTheme::Light
            ? BuiltInTheme::Light
            : BuiltInTheme::Dark);
    }

    Result<void> ConfigureBinding() noexcept {
        TextBox* input =
            Runtime().FindNamed<TextBox>("Input");
        TextBlock* mirror =
            Runtime().FindNamed<TextBlock>(
                "BindingMirror");
        if (input == nullptr || mirror == nullptr) {
            return Failure(
                "ControlGallery binding endpoints are missing");
        }
        BindingDescriptor descriptor;
        descriptor.source = input;
        descriptor.sourceProperty =
            TextBox::TextProperty;
        descriptor.target = mirror;
        descriptor.targetProperty =
            TextBlock::TextProperty;
        descriptor.mode = BindingMode::OneWay;
        Result<BindingHandle> attached =
            Runtime().Bindings()->Attach(descriptor);
        if (!attached) return attached.GetStatus();
        Result<void> changed =
            input->SetText(
                "Binding validation passed");
        if (!changed) return changed.GetStatus();
        Result<std::uint32_t> flushed =
            Runtime().Bindings()->Flush();
        if (!flushed) return flushed.GetStatus();
        if (mirror->Text() != input->Text()) {
            return Failure(
                "ControlGallery binding validation failed");
        }
        return {};
    }

    Result<void> ConfigureVirtualization() noexcept {
        listBox = Runtime().FindNamed<ListBox>("BigList");
        virtualizingPanel =
            Runtime().FindNamed<VirtualizingStackPanel>(
                "BigListPanel");
        if (listBox == nullptr ||
            virtualizingPanel == nullptr) {
            return Failure(
                "ControlGallery virtualization endpoints are missing");
        }
        Result<void> initialized = items.Initialize();
        if (!initialized) return initialized.GetStatus();
        listBox->SetItemTemplate(&itemTemplate);
        Result<void> source =
            listBox->SetItemsSource(&items);
        if (!source) return source.GetStatus();
        Result<void> extent =
            virtualizingPanel->SetEstimatedItemExtent(
                24.0);
        if (extent) {
            extent = virtualizingPanel->SetOverscanCount(
                2U);
        }
        if (!extent) return extent.GetStatus();
        Result<bool> viewport =
            virtualizingPanel->SetViewport(
                {360.0, 120.0});
        if (!viewport) return viewport.GetStatus();
        generator =
            std::make_unique<ItemContainerGenerator>(
                *Runtime().Tree(),
                *Runtime().Layout(),
                *Runtime().EffectiveValues(),
                nullptr,
                Runtime().Renderer());
        Result<void> attached =
            generator->AttachVirtualized(
                *listBox, *virtualizingPanel);
        if (!attached) return attached.GetStatus();
        Result<bool> scrolled =
            virtualizingPanel->SetVerticalOffset(
                4800.0);
        if (!scrolled) return scrolled.GetStatus();
        return {};
    }

    void UpdatePlanSnapshot() noexcept {
        const RenderPlan& plan =
            Runtime().Renderer()->CurrentPlan();
        snapshot.planHash = plan.StableHash();
        snapshot.nodeCount = plan.Nodes().Size();
        snapshot.commandCount =
            plan.Commands().Size();
        snapshot.textCommandCount = 0U;
        for (const RenderCommand& command : plan.Commands()) {
            if (command.kind == RenderCommandKind::DrawGlyphRun) {
                ++snapshot.textCommandCount;
            }
        }
    }

    Result<void> RunFrame() noexcept {
        if (textLayoutService == nullptr) {
            Result<RuntimeFrameResult> frame =
                Runtime().RunFrame();
            if (!frame) return frame.GetStatus();
            UpdatePlanSnapshot();
            return {};
        }

        TextBlockLayoutServiceScope textScope(
            *textLayoutService);
        Result<RuntimeFrameResult> frame =
            Runtime().RunFrame();
        if (!frame) return frame.GetStatus();
        UpdatePlanSnapshot();
        return {};
    }

    Result<void> SetTextLayoutService(
        Controls::ITextBlockLayoutService& service,
        bool refreshExisting) noexcept {
        textLayoutService = &service;
        Presentation::ObjectTree* tree = Runtime().Tree();
        Presentation::Visual* root =
            tree != nullptr ? tree->Root() : nullptr;
        return root != nullptr
            ? ApplyTextLayoutServiceToSubtree(
                  *root, service, refreshExisting)
            : Result<void>{};
    }
    Result<void> EnsureNativeTextServices() noexcept {
#if defined(_WIN32)
        Result<bool> attached =
            inputMethod.AttachActiveWindow();
        if (!attached) return attached.GetStatus();
        if (inputMethod.IsAttached()) {
            clipboard.SetOwnerWindow(
                inputMethod.AttachedWindow());
        }
#endif
        return {};
    }

    Result<bool> HandleWindowEvent(
        const Platform::WindowEvent& event) noexcept {
        if (event.type !=
                Platform::WindowEventType::CloseRequested &&
            event.type !=
                Platform::WindowEventType::Closed) {
            Result<void> native =
                EnsureNativeTextServices();
            if (!native) return native.GetStatus();
        }

        const double scale = EventScale(event);
        switch (event.type) {
        case Platform::WindowEventType::CloseRequested:
        case Platform::WindowEventType::Closed:
            return false;
        case Platform::WindowEventType::Exposed: {
            Result<void> frame = RunFrame();
            return frame
                ? Result<bool>(true)
                : Result<bool>(frame.GetStatus());
        }
        case Platform::WindowEventType::Resized:
        case Platform::WindowEventType::ScaleChanged: {
            if (event.width == 0U ||
                event.height == 0U) {
                return false;
            }
            Result<void> resized = Runtime().Resize({
                static_cast<double>(event.width) / scale,
                static_cast<double>(event.height) / scale});
            if (!resized) return resized.GetStatus();
            Result<void> frame = RunFrame();
            return frame
                ? Result<bool>(true)
                : Result<bool>(frame.GetStatus());
        }
        case Platform::WindowEventType::PointerMove:
        case Platform::WindowEventType::PointerDown:
        case Platform::WindowEventType::PointerUp:
        case Platform::WindowEventType::PointerWheel: {
            PointerInput input;
            input.pointerId = 1U;
            input.position = {
                event.x / scale,
                event.y / scale};
            input.changedButton =
                MapButton(event.button);
            input.wheelDeltaX =
                event.wheelDeltaX / 120.0;
            input.wheelDeltaY =
                event.wheelDeltaY / 120.0;
            switch (event.type) {
            case Platform::WindowEventType::PointerDown:
                input.action = PointerAction::Down;
                break;
            case Platform::WindowEventType::PointerUp:
                input.action = PointerAction::Up;
                break;
            case Platform::WindowEventType::PointerWheel:
                input.action = PointerAction::Wheel;
                break;
            case Platform::WindowEventType::PointerMove:
            default:
                input.action = PointerAction::Move;
                break;
            }
            Result<PointerDispatchResult> dispatched =
                Runtime().DispatchPointer(input);
            if (!dispatched) {
                return dispatched.GetStatus();
            }
            Result<void> frame = RunFrame();
            return frame
                ? Result<bool>(true)
                : Result<bool>(frame.GetStatus());
        }
        case Platform::WindowEventType::KeyDown:
        case Platform::WindowEventType::KeyUp: {
            if (event.key == 0U) return false;
            KeyboardInput input;
            input.action =
                event.type ==
                    Platform::WindowEventType::KeyDown
                ? KeyboardAction::Down
                : KeyboardAction::Up;
            input.key = event.key;
            input.modifiers = event.modifiers;
            input.isRepeat = event.repeat;
            Result<KeyboardDispatchResult> dispatched =
                Runtime().DispatchKeyboard(input);
            if (!dispatched) {
                return dispatched.GetStatus();
            }
            Result<void> frame = RunFrame();
            return frame
                ? Result<bool>(true)
                : Result<bool>(frame.GetStatus());
        }
        case Platform::WindowEventType::TextInput: {
            if (event.textSize == 0U) return false;
            Result<TextInputDispatchResult> dispatched =
                Runtime().DispatchText({event.Text()});
            if (!dispatched) {
                return dispatched.GetStatus();
            }
            Result<void> frame = RunFrame();
            return frame
                ? Result<bool>(true)
                : Result<bool>(frame.GetStatus());
        }
        case Platform::WindowEventType::Invalid:
        default:
            return false;
        }
    }

    void Cleanup() noexcept {
        if (generator) {
            static_cast<void>(generator->Detach());
            generator.reset();
        }
        if (listBox != nullptr) {
            static_cast<void>(
                listBox->SetItemsSource(nullptr));
            listBox->SetItemTemplate(nullptr);
        }
        listBox = nullptr;
        virtualizingPanel = nullptr;
        if (view && Runtime().IsMounted()) {
            static_cast<void>(Runtime().Unmount());
        }
        root.Reset();
        if (view) {
            view->Shutdown();
            view.Reset();
        }
#if defined(_WIN32)
        static_cast<void>(inputMethod.Detach());
        clipboard.SetOwnerWindow(nullptr);
#endif
    }
};

GalleryRuntime::GalleryRuntime() noexcept = default;

GalleryRuntime::~GalleryRuntime() {
    Shutdown();
}

Result<void> GalleryRuntime::Initialize(
    StringView assetDirectory,
    GalleryLoadMode loadMode,
    GalleryTheme requestedTheme) noexcept {
    if (impl_) {
        return Failure(
            "ControlGallery runtime is already initialized");
    }
    if (assetDirectory.Empty()) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "ControlGallery asset directory is empty");
    }

    std::unique_ptr<Impl> state =
        std::make_unique<Impl>();
    Result<void> status =
        state->environment.AddModule(
            MakeStatusBadgeModuleManifest());
    RuntimeHostOptions options;
    options.renderBackend = &state->nullBackend;
    options.clipboard = &state->clipboard;
#if defined(_WIN32)
    options.textInputMethodHost =
        &state->inputMethod;
#endif
    if (status) status = state->environment.Initialize();
    if (status) {
        Result<Ref<RuntimeView>> created =
            state->environment.CreateView(options);
        if (!created) return created.GetStatus();
        state->view = std::move(created).Value();
    }
    if (!status) return status.GetStatus();

    const std::string assetPath(
        assetDirectory.Data(),
        assetDirectory.SizeBytes());
    status = state->LoadTheme(
        assetPath, requestedTheme);
    if (status) {
        status = state->LoadDocument(
            assetPath, loadMode);
    }
    if (!status) return status.GetStatus();
    if (!state->root ||
        state->root->RuntimeType() !=
            Border::StaticTypeId()) {
        return Failure(
            "ControlGallery root is not a Border");
    }
    status = state->Runtime().Mount({900.0, 640.0});
    if (status) status = state->ConfigureBinding();
    if (status) status = state->ConfigureVirtualization();
    if (status) status = state->RunFrame();
    if (!status) return status.GetStatus();

    state->snapshot.namedObjectCount =
        state->Runtime().NamedObjectCount();
    state->snapshot.itemCount = state->items.Count();
    state->snapshot.realizedItemCount =
        state->generator->GeneratedCount();
    state->snapshot.createdContainerCount =
        state->generator->CreatedContainerCount();
    state->snapshot.loadMode = loadMode;
    state->snapshot.theme = requestedTheme;

    if (state->snapshot.nodeCount == 0U ||
        state->snapshot.commandCount == 0U ||
        state->snapshot.namedObjectCount < 10U ||
        state->snapshot.itemCount != 10000U ||
        state->snapshot.realizedItemCount == 0U ||
        state->snapshot.realizedItemCount >=
            state->snapshot.itemCount ||
        state->snapshot.createdContainerCount > 16U) {
        std::fprintf(
            stderr,
            "ControlGallery metrics: nodes=%u commands=%u names=%u "
            "items=%u realized=%u created=%u\n",
            state->snapshot.nodeCount,
            state->snapshot.commandCount,
            state->snapshot.namedObjectCount,
            state->snapshot.itemCount,
            state->snapshot.realizedItemCount,
            state->snapshot.createdContainerCount);
        return Failure(
            "ControlGallery acceptance metrics are invalid");
    }

    impl_ = std::move(state);
    return {};
}

Base::Result<void> GalleryRuntime::SetTextLayoutService(
    Controls::ITextBlockLayoutService& service,
    bool refreshExisting) noexcept {
    if (!impl_) {
        return Status::Failure(
            ErrorCode::NotInitialized,
            "ControlGallery runtime is not initialized");
    }
    return impl_->SetTextLayoutService(service, refreshExisting);
}
Result<bool> GalleryRuntime::HandleWindowEvent(
    const Platform::WindowEvent& event) noexcept {
    if (!impl_) {
        return Status::Failure(
            ErrorCode::NotInitialized,
            "ControlGallery runtime is not initialized");
    }
    return impl_->HandleWindowEvent(event);
}

void GalleryRuntime::Shutdown() noexcept {
    impl_.reset();
}

const GallerySnapshot&
GalleryRuntime::Snapshot() const noexcept {
    static const GallerySnapshot empty;
    return impl_ ? impl_->snapshot : empty;
}

const RenderPlan&
GalleryRuntime::Plan() const noexcept {
    static const RenderPlan empty;
    return impl_ &&
        impl_->Runtime().Renderer() != nullptr
        ? impl_->Runtime().Renderer()->CurrentPlan()
        : empty;
}

} // namespace Aero::Samples::ControlGallery




