#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include <Aero/BuiltinThemes.generated.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

ViewState::ViewState(
        View& owner,
        Gui& guiOwner,
        Base::IAllocator& value,
        Base::Ref<Base::Object> guiState) noexcept
    : allocator(&value),
          guiOwner(&guiOwner),
          gui(std::move(guiState)),
          publicRenderer(owner, value),
          dispatcher(&static_cast<GuiState&>(*gui).dispatcher),
          xamlRuntime(&static_cast<GuiState&>(*gui).xaml),
          schemaBundle(&xamlRuntime->SchemaBundle()),
          documentCache(&xamlRuntime->Documents()),
          itemGenerators(&value),
          fragmentMounts(&value),
          componentMounts(&value) {}

Base::Result<void> ViewState::ApplyViewport(
        const ViewViewport& next) noexcept {
        if (renderer == nullptr) {
            return AeroNotInitialized(
                "View render tree is unavailable");
        }
        const ViewViewport previous = viewport;
        Base::Result<void> updated = renderer->SetViewport(
            next.logicalSize,
            next.pixelWidth,
            next.pixelHeight,
            next.dpiScale);
        if (!updated) return updated.GetStatus();
        if (HasAttachedRoot()) {
            updated = ResizeVisualRoot(next.logicalSize);
            if (!updated) {
                static_cast<void>(renderer->SetViewport(
                    previous.logicalSize,
                    previous.pixelWidth,
                    previous.pixelHeight,
                    previous.dpiScale));
                return updated.GetStatus();
            }
        }
        viewport = next;
        return {};
    }

void ViewState::Shutdown() noexcept {
        audio.Shutdown();
        BeginDestroyInteractions();
        DetachViewUi(*this);
        FinishDestroyInteractions();
        VisitTextElements(RootVisual(), nullptr);
        VisitPaths(RootVisual(), nullptr);
        if (HasAttachedRoot()) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
        }
        mounted = false;
        root.Reset();
        ClearLoadedDocument(*this);
        for (std::uint32_t index = componentMounts.Size();
             index > 0U; --index) {
            FreeObject(
                *allocator,
                Base::MemoryTag::Ui,
                componentMounts[index - 1U]);
        }
        componentMounts.Clear();
        if (effectLifetime) effectLifetime->Invalidate();
        if (values != nullptr) {
            values->Shutdown();
        }
        if (tree != nullptr) {
            tree->SetTextLayout(nullptr);
            tree->SetMeshResources(nullptr);
        }
        DestroyUiEngines();
        if (images != nullptr) {
            images->Shutdown(GetImageResources());
        }
        if (tree != nullptr) {
            tree->SetLifecycleHandler(nullptr);
        }
        if (animations != nullptr) {
            static_cast<void>(animations->RemoveAll());
        }
        if (storyboards != nullptr) {
            storyboards->storyboardSessions.Clear();
        }


        FreeObject(*allocator, Base::MemoryTag::Ui, interactivity);
        FreeObject(*allocator, Base::MemoryTag::Ui, storyboards);
        FreeObject(*allocator, Base::MemoryTag::Ui, input);
        FreeObject(*allocator, Base::MemoryTag::Ui, events);
        if (bindings != nullptr) bindings->Shutdown();
        FreeObject(*allocator, Base::MemoryTag::Ui, bindings);
        FreeObject(*allocator, Base::MemoryTag::Ui, renderer);
        FreeObject(*allocator, Base::MemoryTag::Ui, layout);
        FreeObject(*allocator, Base::MemoryTag::Ui, tree);
        FreeObject(*allocator, Base::MemoryTag::Ui, text);
        FreeObject(*allocator, Base::MemoryTag::Ui, images);
        FreeObject(*allocator, Base::MemoryTag::Ui, animations);
        FreeObject(*allocator, Base::MemoryTag::Ui, values);
        FreeObject(*allocator, Base::MemoryTag::Ui, objectFactory);
        schema = nullptr;
        metadata = nullptr;
        device.Reset();
        initialized = false;
    }

Base::Result<void> ViewState::Initialize(
        const ViewOptions& requested) noexcept {
        if (initialized) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "View is already initialized");
        }
        if (terminal) {
            return ViewInvalidState(
                "View cannot be restarted after shutdown or failed startup");
        }
        options = requested;
        if (!gui) {
            return ViewInvalidState("View has no Gui provider state");
        }
        const GuiState& guiState =
            static_cast<const GuiState&>(*gui);
        seenTextureProviderChange = guiState.textureChangeGeneration;
        seenFontProviderChange = guiState.fontChangeGeneration;

        Base::Result<void> status;

        Base::Result<Base::Ref<RenderDevice>>
            headless =
                ::Aero::Render::CreateHeadlessRenderDevice(
                    allocator);
        if (!headless) {
            terminal = true;
            return headless.GetStatus();
        }
        device = std::move(headless).Value();
        deviceGeneration = device->Generation();

        Base::Result<Base::Ref<Markup::EffectLifetime>> lifetime =
            Base::MakeRefWithAllocator<Markup::EffectLifetime>(
                *allocator);
        status = lifetime
            ? Base::Result<void>()
            : Base::Result<void>(lifetime.GetStatus());
        if (status) effectLifetime = std::move(lifetime).Value();

        if (!status || schemaBundle == nullptr ||
            !schemaBundle->IsFrozen()) {
            terminal = true;
            return status
                ? ViewInvalidState(
                      "Gui schema is not initialized")
                : status.GetStatus();
        }
        metadata = &schemaBundle->Metadata();
        schema = &schemaBundle->Schema();

        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, objectFactory, *dispatcher,
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata),
                *metadata);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, values, *dispatcher,
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata));
        }
        if (status) status = values->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, animations, *dispatcher, *values, allocator);
        }
        if (status) status = animations->Initialize();
        if (status) {
            animations->SetAutomaticTickingEnabled(
                options.automaticAnimationClock);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, tree, *dispatcher, *values);
        }
        if (status) status = tree->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, layout, *dispatcher);
        }
        if (status) status = layout->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, renderer, *dispatcher);
        }
        if (status) status = renderer->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, images, allocator);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, text, allocator);
        }
        if (status) {
            status = text->Initialize(
                *device, nullptr, options.text);
        }
        if (status) {
            tree->SetLifecycleHandler(
                &TextLifecycleHook, this);
        }
        if (status) {
            status = AllocateObject(
                *allocator,
                Base::MemoryTag::Ui,
                bindings,
                *dispatcher,
                metadata);
        }
        if (status) status = bindings->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, events,
                ::Aero::MetadataPrivate::
                    RoutedEventState(*metadata));
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, input, *tree, *events);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, interactivity, *this);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, storyboards, *this);
        }
        if (status) status = CreateUiEngines();
        if (status && interactivity != nullptr) {
            interactivity->Bind();
        }
        if (status && storyboards != nullptr) {
            storyboards->Bind();
        }
        if (status && overlays != nullptr) {
            overlays->Bind();
        }
        if (status && focus != nullptr) {
            focus->Bind();
        }
        if (status && resources != nullptr) {
            resources->Bind();
            status = resources->RebuildDynamicEnvironment();
        }
        if (status) {
            tree->AttachPresentation(layout, renderer);
        }
        if (!status) {
            Shutdown();
            terminal = true;
            return status.GetStatus();
        }
        initialized = true;
        return {};
    }

View::View(
    ConstructionToken,
    Gui& gui,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator* selected = allocator != nullptr
        ? allocator
        : &Base::GetDefaultAllocator();
    void* stateMemory = selected->Allocate({
        sizeof(ViewState), alignof(ViewState), Base::MemoryTag::Markup});
    if (stateMemory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(ViewState), alignof(ViewState), Base::MemoryTag::Markup);
    }
    state_ = new (stateMemory) ViewState(
        *this, gui, *selected, gui.state_);
}

View::~View() noexcept {
    if (state_ == nullptr) return;
    if (!state_->terminal) {
        state_->Shutdown();
        state_->terminal = true;
    }
    state_->publicRenderer.Shutdown();
    Base::IAllocator* allocator = state_->allocator;
    state_->~ViewState();
    allocator->Deallocate(
        state_, sizeof(ViewState), alignof(ViewState), Base::MemoryTag::Markup);
    state_ = nullptr;
}

Base::Result<void> View::Initialize(
    const ViewOptions& options) noexcept {
    if (state_ == nullptr || !state_->gui) {
        return ViewApiInvalidState("View has no Gui state");
    }
    const GuiState& guiState =
        static_cast<const GuiState&>(*state_->gui);
    if (!guiState.initialized) {
        return ViewNotInitialized(
            "Gui must be initialized before creating a View");
    }
    Base::Result<void> initialized = state_->Initialize(options);
    if (!initialized) return initialized.GetStatus();
    if (options.applicationResources != nullptr) {
        SetViewResourceDictionary(
            *state_,
            ResourceLayer::Application,
            *options.applicationResources,
            ResourceLoadMode::Replace);
    }
    return options.loadBuiltInTheme
        ? LoadViewBuiltInTheme(*state_, options.builtInTheme)
        : Base::Result<void>();
}

Base::Result<void> LoadViewResources(
    ViewState& state,
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (state.resources == nullptr) {
        return AeroNotInitialized(
            "View resource host is unavailable");
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state.resources->ResolveLayer(layer);
    if (!target) return target.GetStatus();
    return state.resources->LoadLayer(
        uri,
        *target.Value(),
        diagnostics,
        mode == ResourceLoadMode::Merge);
}

Base::Result<void>
LoadViewCompiledResources(
    ViewState& state,
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    if (state.resources == nullptr) {
        return AeroNotInitialized(
            "View resource host is unavailable");
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state.resources->ResolveLayer(layer);
    if (!target) return target.GetStatus();
    return state.resources->LoadCompiledLayer(
        bytes,
        originUri,
        *target.Value(),
        mode == ResourceLoadMode::Merge);
}

void SetViewResourceDictionary(
    ViewState& state,
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    if (state.resources == nullptr || !state.initialized) {
        return;
    }
    if (state.mounted || state.root ||
        state.loadedDocument.root) {
        return;
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state.resources->ResolveLayer(layer);
    if (!target) return;
    Base::Result<Aero::ResourceDictionary> shared =
        dictionary.Share();
    if (!shared) return;
    if (mode == ResourceLoadMode::Merge) {
        Base::Result<void> merged =
            target.Value()->AddMerged(shared.Value());
        if (!merged) return;
        (void)state.resources->RebuildDynamicEnvironment();
        return;
    }

    Aero::ResourceDictionary previous =
        std::move(*target.Value());
    *target.Value() = std::move(shared).Value();
    Base::Result<void> rebuilt =
        state.resources->RebuildDynamicEnvironment();
    if (rebuilt) return;
    *target.Value() = std::move(previous);
    Base::Result<void> restored =
        state.resources->RebuildDynamicEnvironment();
    (void)restored;
}

Base::Result<void> LoadViewBuiltInTheme(
    ViewState& state,
    BuiltInTheme theme) noexcept {
    if (state.resources == nullptr) {
        return AeroNotInitialized(
            "View resource host is unavailable");
    }
    if (theme != BuiltInTheme::Light &&
        theme != BuiltInTheme::Dark) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Built-in theme value is invalid");
    }
    const Base::StringView themeUri =
        theme == BuiltInTheme::Light
        ? Base::StringView(
              "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.LightBlue.xaml")
        : Base::StringView(
              "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.DarkBlue.xaml");

    Aero::ResourceDictionary previous =
        std::move(state.resources->themeResources);
    Base::Result<void> loaded = LoadViewResources(
        state, ResourceLayer::Theme, themeUri);
    if (loaded) return {};

    const std::uint8_t* paletteBytes =
        theme == BuiltInTheme::Light
        ? Aero::AeroThemeLightCompiled
        : Aero::AeroThemeDarkCompiled;
    const std::uint32_t paletteSize =
        theme == BuiltInTheme::Light
        ? Aero::AeroThemeLightCompiledSize
        : Aero::AeroThemeDarkCompiledSize;
    Base::Result<Base::ResourceUri> paletteUri =
        ::Aero::BuiltInThemeUri(
            theme == BuiltInTheme::Light
            ? Base::StringView("Light.xaml")
            : Base::StringView("Dark.xaml"));
    if (!paletteUri) {
        state.resources->themeResources = std::move(previous);
        return paletteUri.GetStatus();
    }
    Base::Result<Base::ResourceUri> genericUri =
        ::Aero::BuiltInThemeUri(
            Base::StringView("Generic.xaml"));
    if (!genericUri) {
        state.resources->themeResources = std::move(previous);
        return genericUri.GetStatus();
    }

    loaded = paletteSize != 0U
        ? LoadViewCompiledResources(state,
              ResourceLayer::Theme,
              {paletteBytes, paletteSize},
              paletteUri.Value())
        : LoadViewResources(state,
              ResourceLayer::Theme,
              paletteUri.Value().Canonical());
    if (loaded) {
        loaded = Aero::AeroThemeGenericCompiledSize != 0U
            ? LoadViewCompiledResources(state,
                  ResourceLayer::Theme,
                  {Aero::AeroThemeGenericCompiled,
                   Aero::AeroThemeGenericCompiledSize},
                  genericUri.Value(),
                  ResourceLoadMode::Merge)
            : LoadViewResources(state,
                  ResourceLayer::Theme,
                  genericUri.Value().Canonical(),
                  ResourceLoadMode::Merge);
    }
    if (!loaded) {
        state.resources->themeResources =
            std::move(previous);
        Base::Result<void> restored =
            state.resources->RebuildDynamicEnvironment();
        return restored
            ? Base::Result<void>(loaded.GetStatus())
            : Base::Result<void>(restored.GetStatus());
    }
    return {};
}

Base::Result<void> View::SetContent(
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return ViewNotInitialized(
            "View must be initialized before SetContent");
    }
    return state_->mounted
        ? ReplaceViewDocument(
              *state_, std::move(document), availableSize)
        : MountViewDocument(
              *state_, std::move(document), availableSize);
}

Base::Result<void> View::SetContent(
    Base::Ref<FrameworkElement> root,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View must be initialized before SetContent");
    }
    if (root) {
        Markup::XamlDocument document;
        Base::Result<bool> pending =
            GetGui().TakeLoadedDocument(*root, document);
        if (!pending) return pending.GetStatus();
        if (pending.Value()) {
            return SetContent(std::move(document), availableSize);
        }
    }
    if (state_->mounted) {
        Base::Result<void> unmounted = UnmountRoot(*state_);
        if (!unmounted) return unmounted.GetStatus();
    }
    return MountViewContent(
        *state_,
        Base::Ref<Base::Object>(std::move(root)),
        availableSize);
}

Base::Result<void> View::SetContent(
    Base::Ref<FrameworkElement> root) noexcept {
    const Aero::Size availableSize = state_ != nullptr
        ? state_->viewport.logicalSize
        : Aero::Size{};
    return SetContent(std::move(root), availableSize);
}

Base::Result<void> View::SetContent(
    Base::Ref<FrameworkElement> root,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View must be initialized before SetContent");
    }
    if (!root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View host root must not be null");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot mount an empty UI document");
    }
    if (state_->mounted) {
        Base::Result<void> unmounted = UnmountRoot(*state_);
        if (!unmounted) return unmounted.GetStatus();
    }

    Markup::LoaderResult next = Aero::Markup::TakeXamlDocument(document);
    if (!next.root) {
        next.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View host document has no root object");
    }
    Base::Result<Aero::UIElement*> documentRoot =
        state_->ResolveUIElement(
            *next.root, next.root->RuntimeType());
    if (!documentRoot) {
        next.Clear();
        return Base::Result<void>(documentRoot.GetStatus());
    }
    Base::Result<Aero::UIElement*> hostRoot =
        state_->ResolveUIElement(*root, root->RuntimeType());
    if (!hostRoot) {
        next.Clear();
        return Base::Result<void>(hostRoot.GetStatus());
    }

    // Attach the loaded document root (for example a UserControl) as a visual
    // child of the host root (for example the wrapping Window). The host is
    // kept as the view root so the app-facing Window remains the mounted root.
    Aero::Markup::VisualEdge edge;
    edge.parent = hostRoot.Value();
    edge.child = documentRoot.Value();
    Base::Result<void> pushed =
        next.visualContent.mountEdges.PushBack(std::move(edge));
    if (!pushed) {
        next.Clear();
        return pushed.GetStatus();
    }

    Base::Result<void> assigned =
        AeroGuiInternal::SetOwnedContent(
            *static_cast<Controls::ContentControl*>(hostRoot.Value()),
            next.root,
            *documentRoot.Value());
    if (!assigned) {
        next.Clear();
        return assigned.GetStatus();
    }

    next.root = std::move(root);
    state_->loadedDocument = std::move(next);
    return MountRoot(
        *state_,
        state_->loadedDocument.root, availableSize);
}

Base::Result<void> View::SetViewport(
    const ViewViewport& viewport) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return ViewNotInitialized(
            "View must be initialized before setting its viewport");
    }
    Base::Result<void> valid = ValidateViewport(viewport);
    if (!valid) return valid.GetStatus();
    return state_->ApplyViewport(viewport);
}

void View::SetSize(
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) return;
    Base::Result<ViewViewport> viewport =
        Aero::MakeLogicalViewport(
            availableSize,
            state_->viewport.dpiScale);
    if (!viewport) return;
    static_cast<void>(SetViewport(viewport.Value()));
}

void View::SetSize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    SetSize(Aero::Size{
        static_cast<double>(width),
        static_cast<double>(height)});
}

void View::SetScale(double scale) noexcept {
    if (state_ == nullptr || !state_->initialized ||
        !std::isfinite(scale) || scale <= 0.0) {
        return;
    }
    Base::Result<ViewViewport> viewport =
        Aero::MakeLogicalViewport(
            state_->viewport.logicalSize, scale);
    if (!viewport) return;
    static_cast<void>(SetViewport(viewport.Value()));
}

bool View::Update(double timeInSeconds) noexcept {
    if (!active_ || state_ == nullptr) return false;
    if (!std::isfinite(timeInSeconds) || timeInSeconds < 0.0 ||
        (hasUpdateTime_ && timeInSeconds < updateTimeSeconds_)) {
        state_->ReportUpdateFailure(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View update time must be finite, nonnegative and monotonic"));
        return false;
    }
    const ::Aero::Render::RenderFrame* before =
        ViewState::CurrentFrame(*this);
    const std::uint64_t beforeVersion =
        before != nullptr ? before->Version() : 0U;
    double elapsedSeconds = 0.0;
    if (hasUpdateTime_) {
        elapsedSeconds = timeInSeconds - updateTimeSeconds_;
    }
    updateTimeSeconds_ = timeInSeconds;
    hasUpdateTime_ = true;
    const double elapsedMilliseconds = std::min(
        elapsedSeconds * 1000.0,
        static_cast<double>(UINT32_MAX));
    const std::uint32_t elapsed =
        static_cast<std::uint32_t>(elapsedMilliseconds);
    if (elapsed != 0U) {
        Base::Result<std::uint32_t> advanced =
            AdvanceViewClocks(*state_, elapsed);
        if (!advanced) {
            state_->ReportUpdateFailure(advanced.GetStatus());
            return false;
        }
    }
    Base::Result<std::uint32_t> frame = ExecuteViewFrame(*state_, *this);
    if (!frame) {
        state_->ReportUpdateFailure(frame.GetStatus());
        return false;
    }
    state_->ClearUpdateFailure();
    const ::Aero::Render::RenderFrame* after =
        ViewState::CurrentFrame(*this);
    return after != nullptr && after->Version() != 0U &&
        after->Version() != beforeVersion;
}

void View::Activate() noexcept {
    active_ = true;
    hasUpdateTime_ = false;
}

void View::Deactivate() noexcept {
    active_ = false;
    hasUpdateTime_ = false;
}

IRenderer& View::GetRenderer() noexcept {
    return state_->publicRenderer;
}

const IRenderer& View::GetRenderer() const noexcept {
    return state_->publicRenderer;
}

CommittedFrameInfo View::GetCommittedFrameInfo() const noexcept {
    CommittedFrameInfo info;
    if (state_ == nullptr || state_->renderer == nullptr) return info;
    const ::Aero::Render::RenderFrame& frame =
        state_->renderer->CurrentFrame();
    if (frame.Version() == 0U) return info;
    const ::Aero::Render::RenderDiagnostics diagnostics =
        state_->renderer->Diagnostics();
    info.version = frame.Version();
    info.nodeCount = diagnostics.nodeCount;
    info.commandCount = diagnostics.commandCount;
    info.contentHash = diagnostics.frameHash;
    return info;
}

FrameworkElement* View::GetContent() noexcept {
    return state_ != nullptr && state_->RootVisual() != nullptr
        ? ::Aero::TryCast<::Aero::FrameworkElement>(state_->RootVisual())
        : nullptr;
}

const FrameworkElement* View::GetContent() const noexcept {
    return state_ != nullptr && state_->RootVisual() != nullptr
        ? ::Aero::TryCast<::Aero::FrameworkElement>(state_->RootVisual())
        : nullptr;
}

Gui& View::GetGui() noexcept {
    return *state_->guiOwner;
}

const Gui& View::GetGui() const noexcept {
    return *state_->guiOwner;
}


} // namespace Aero
