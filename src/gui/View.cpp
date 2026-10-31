#include "gui/ViewFrame.hpp"
#include "gui/GuiRuntime.hpp"
#include "gui/text/TextPipeline.hpp"
#include "render/RenderTree.hpp"
#include "gui/input/InputDevicesState.hpp"
#include <Aero/BuiltinThemes.generated.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Decorators.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/XamlReader.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

ViewFrame::ViewFrame(
        View& owner,
        Gui& guiOwner,
        Base::IAllocator& value,
        Base::Ref<GuiRuntime> guiState) noexcept
    : allocator(&value),
      guiOwner(&guiOwner),
      gui(std::move(guiState)),
      publicRenderer(owner, value),
      dispatcher(&gui->dispatcher),
      schemaBundle(&gui->schema),
      documentCache(&gui->documents),
      xamlProviders(&gui->xamlProviders),
      itemGenerators(&value),
      trackedItemsControls(&value),
      fragmentMounts(&value),
      componentMounts(&value) {}

Base::Result<void> ViewFrame::ApplyViewport(
        const ViewViewport& next) noexcept {
        ::Aero::Render::RenderTree* renderTree = RenderTree();
        if (renderTree == nullptr) {
            return ViewNotInitialized(
                "View render tree is unavailable");
        }
        const ViewViewport previous = viewport;
        Base::Result<void> updated = renderTree->SetViewport(
            next.logicalSize,
            next.pixelWidth,
            next.pixelHeight,
            next.dpiScale);
        if (!updated) return updated.GetStatus();
        if (HasAttachedRoot()) {
            updated = ResizeVisualRoot(next.logicalSize);
            if (!updated) {
                static_cast<void>(renderTree->SetViewport(
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

void ViewFrame::Shutdown() noexcept {
    audio.Shutdown();
    if (Storyboards() != nullptr) {
        Storyboards()->storyboardSessions.Clear();
    }
    if (Animations() != nullptr) {
        Animations()->Shutdown();
    }
    if (Bindings() != nullptr) {
        Bindings()->Shutdown();
    }
    BeginDestroyInteractions();
    DetachViewUi(*this);
    FinishDestroyInteractions();
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
    for (std::uint32_t index = fragmentMounts.Size();
         index > 0U; --index) {
        FreeObject(
            *allocator,
            Base::MemoryTag::Ui,
            fragmentMounts[index - 1U]);
    }
    fragmentMounts.Clear();
    if (Tree() != nullptr) {
        Tree()->FlushLifecycle();
    }
    if (effectLifetime) effectLifetime->Invalidate();
    if (Values() != nullptr) {
        Values()->Shutdown();
    }
    if (Tree() != nullptr) {
        Tree()->SetTextLayout(nullptr);
        Tree()->SetMeshResources(nullptr);
        Tree()->SetLifecycleHandler(nullptr);
    }

    if (Images() != nullptr) {
        Images()->Shutdown(GetImageResources());
    }

    if (engines != nullptr) {
        FreeObject(*allocator, Base::MemoryTag::Ui, engines);
        engines = nullptr;
    }
    if (reloadCoordinator != nullptr) {
        reloadCoordinator->~ReloadCoordinator();
        allocator->Deallocate(
            reloadCoordinator,
            sizeof(Markup::ReloadCoordinator),
            alignof(Markup::ReloadCoordinator),
            Base::MemoryTag::Markup);
        reloadCoordinator = nullptr;
    }
    schema = nullptr;
    metadata = nullptr;
    device.Reset();
    initialized = false;
}

Base::Result<void> ViewFrame::Initialize(
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
        const GuiRuntime& guiState = *gui;
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

        Base::Ref<Markup::EffectLifetime> lifetime =
            Base::MakeRefWithAllocator<Markup::EffectLifetime>(
                *allocator);
        status = lifetime
            ? Base::Result<void>()
            : Base::Result<void>(Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM"));
        if (status) effectLifetime = std::move(lifetime);

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
            status = AllocateObject(
                *allocator,
                Base::MemoryTag::Ui,
                engines,
                *this,
                allocator,
                *dispatcher,
                *metadata);
        }
        if (!status) {
            Shutdown();
            terminal = true;
            return status.GetStatus();
        }

        status = engines->values.Initialize();
        if (status) status = engines->animations.Initialize();
        if (status) {
            engines->animations.SetAutomaticTickingEnabled(
                options.automaticAnimationClock);
        }
        if (status) status = engines->tree.Initialize();
        if (status) {
            engines->tree.SetAnimations(&engines->animations);
        }
        if (status) status = engines->layout.Initialize();
        if (status) {
            engines->tree.SetLayout(&engines->layout);
        }
        if (status) status = engines->renderTree.Initialize();
        if (status) {
            engines->tree.SetRenderTree(&engines->renderTree);
        }
        if (status) {
            status = engines->text.Initialize(
                *device, nullptr, options.text);
        }
        if (status) status = engines->bindings.Initialize();
        if (status) {
            engines->tree.SetBindings(&engines->bindings);
            engines->tree.SetEvents(&engines->events);
            engines->tree.SetInput(&engines->input);
        }

        if (status) {
            engines->tree.SetTemplates(&engines->templates);
            engines->visualStates =
                ::Aero::Controls::FrameworkTemplateState::CreateVisualStateManager(
                    engines->values,
                    engines->templates,
                    engines->animations,
                    (*metadata).DependencyProperties());
            if (!engines->visualStates) {
                status = Base::Status::Failure(
                    Base::ErrorCode::OutOfMemory,
                    "VisualStateManager allocation failed");
            } else {
                engines->tree.SetVisualStates(engines->visualStates.Get());
            }
        }
        if (status) {
            engines->styles.SetTriggerActionHandler(
                &InteractivityEngine::ExecuteStyleTriggerActions, &engines->interactivity);
            engines->tree.SetStyles(&engines->styles);
            engines->tree.SetTextLayout(engines->text.Layout());
            engines->tree.SetMeshResources(GetMeshResources());
            engines->tree.SetClipboard(options.clipboard);
            engines->tree.AttachResourceEnvironment(engines->resources.Environment());
            engines->tree.SetNameScope(this, &ViewFrame::FindNameForElement);
            engines->tree.SetViewState(this);
        }
        if (status) {
            engines->interactivity.Bind();
            engines->storyboards.Bind();
            engines->overlays.Bind();
            engines->focus.Bind();
            engines->resources.Bind();
            status = engines->resources.RebuildDynamicEnvironment();
        }
        if (status) {
            engines->tree.AttachPresentation(Layout(), RenderTree());
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
        sizeof(ViewFrame), alignof(ViewFrame), Base::MemoryTag::Markup});
    if (stateMemory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(ViewFrame), alignof(ViewFrame), Base::MemoryTag::Markup);
    }
    state_ = new (stateMemory) ViewFrame(
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
    state_->~ViewFrame();
    allocator->Deallocate(
        state_, sizeof(ViewFrame), alignof(ViewFrame), Base::MemoryTag::Markup);
    state_ = nullptr;
}

Base::Result<void> View::Initialize(
    const ViewOptions& options) noexcept {
    if (state_ == nullptr || !state_->gui) {
        return ViewInvalidState("View has no Gui state");
    }
    const GuiRuntime& guiState =
        static_cast<const GuiRuntime&>(*state_->gui);
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
    ViewFrame& state,
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (state.Resources() == nullptr) {
        return ViewNotInitialized(
            "View resource host is unavailable");
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state.Resources()->ResolveLayer(layer);
    if (!target) return target.GetStatus();
    return state.Resources()->LoadLayer(
        uri,
        *target.Value(),
        diagnostics,
        mode == ResourceLoadMode::Merge);
}

Base::Result<void>
LoadViewCompiledResources(
    ViewFrame& state,
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    if (state.Resources() == nullptr) {
        return ViewNotInitialized(
            "View resource host is unavailable");
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state.Resources()->ResolveLayer(layer);
    if (!target) return target.GetStatus();
    return state.Resources()->LoadCompiledLayer(
        bytes,
        originUri,
        *target.Value(),
        mode == ResourceLoadMode::Merge);
}

void SetViewResourceDictionary(
    ViewFrame& state,
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    if (state.Resources() == nullptr || !state.initialized) {
        return;
    }
    if (state.mounted || state.root ||
        state.loadedDocument.root) {
        return;
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state.Resources()->ResolveLayer(layer);
    if (!target) return;
    Aero::ResourceDictionary shared =
        dictionary.Share();
    if (!shared) return;
    if (mode == ResourceLoadMode::Merge) {
        target->AddMerged(shared);
        (void)state.Resources()->RebuildDynamicEnvironment();
        return;
    }

    Aero::ResourceDictionary previous =
        std::move(*target.Value());
    *target.Value() = std::move(shared);
    Base::Result<void> rebuilt =
        state.Resources()->RebuildDynamicEnvironment();
    if (rebuilt) return;
    *target.Value() = std::move(previous);
    Base::Result<void> restored =
        state.Resources()->RebuildDynamicEnvironment();
    (void)restored;
}

Base::Result<void> LoadViewBuiltInTheme(
    ViewFrame& state,
    BuiltInTheme theme) noexcept {
    if (state.Resources() == nullptr) {
        return ViewNotInitialized(
            "View resource host is unavailable");
    }
    if (theme != BuiltInTheme::Light &&
        theme != BuiltInTheme::Dark) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Built-in theme value is invalid");
    }
    if (state.gui != nullptr) {
        const bool hasCached = theme == BuiltInTheme::Light
            ? state.gui->hasCachedLightTheme
            : state.gui->hasCachedDarkTheme;
        if (hasCached) {
            const ResourceDictionary& cached = theme == BuiltInTheme::Light
                ? state.gui->cachedLightTheme
                : state.gui->cachedDarkTheme;
            Base::Result<ResourceDictionary> shared = cached.Share();
            if (shared) {
                state.Resources()->themeResources = std::move(shared.Value());
                return state.Resources()->RebuildDynamicEnvironment();
            }
        }
    }
    const Base::StringView themeUri =
        theme == BuiltInTheme::Light
        ? Base::StringView(
              "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.LightBlue.xaml")
        : Base::StringView(
              "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.DarkBlue.xaml");

    Aero::ResourceDictionary previous =
        std::move(state.Resources()->themeResources);
    Base::Result<void> loaded = LoadViewResources(
        state, ResourceLayer::Theme, themeUri);
    if (loaded) {
        if (state.gui != nullptr) {
            ResourceDictionary prototype =
                state.Resources()->themeResources.Share();
            if (prototype) {
                prototype.Seal();
                if (theme == BuiltInTheme::Light) {
                    state.gui->cachedLightTheme = std::move(prototype);
                    state.gui->hasCachedLightTheme = true;
                } else {
                    state.gui->cachedDarkTheme = std::move(prototype);
                    state.gui->hasCachedDarkTheme = true;
                }
            }
        }
        return {};
    }

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
        state.Resources()->themeResources = std::move(previous);
        return paletteUri.GetStatus();
    }
    Base::Result<Base::ResourceUri> genericUri =
        ::Aero::BuiltInThemeUri(
            Base::StringView("Generic.xaml"));
    if (!genericUri) {
        state.Resources()->themeResources = std::move(previous);
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
        state.Resources()->themeResources =
            std::move(previous);
        Base::Result<void> restored =
            state.Resources()->RebuildDynamicEnvironment();
        return restored
            ? Base::Result<void>(loaded.GetStatus())
            : Base::Result<void>(restored.GetStatus());
    }
    if (state.gui != nullptr) {
        ResourceDictionary prototype =
            state.Resources()->themeResources.Share();
        if (prototype) {
            prototype.Seal();
            if (theme == BuiltInTheme::Light) {
                state.gui->cachedLightTheme = std::move(prototype);
                state.gui->hasCachedLightTheme = true;
            } else {
                state.gui->cachedDarkTheme = std::move(prototype);
                state.gui->hasCachedDarkTheme = true;
            }
        }
    }
    return {};
}

Base::Result<Base::Ref<Base::Object>> View::LoadXamlRoot(
    Base::StringView uri, Base::MetaTypeId expectedRoot) noexcept {
    if (state_ == nullptr || !state_->initialized || !state_->gui) {
        return ViewNotInitialized("View must be initialized to load XAML");
    }
    const Aero::Size availableSize = state_->viewport.logicalSize;
    Markup::XamlReader reader(GetGui());
    Base::Result<Markup::XamlDocument> doc = reader.Load(uri);
    if (!doc) return doc.GetStatus();

    Base::Ref<Base::Object> root = doc.Value().Root();
    if (!root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, "Loaded XAML document has no root");
    }
    if (expectedRoot != Base::InvalidMetaTypeId) {
        if (state_->metadata != nullptr &&
            !state_->metadata->Types().IsDerivedFrom(root->RuntimeType(), expectedRoot)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument, "Loaded root does not match expected type");
        }
    }
    if (!SetContent(std::move(doc).Value(), availableSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState, "SetContent failed for loaded XAML document");
    }
    if (state_->reloadCoordinator == nullptr) {
        Base::IAllocator* alloc = state_->allocator;
        void* mem = alloc->Allocate({sizeof(Markup::ReloadCoordinator),
                                     alignof(Markup::ReloadCoordinator),
                                     Base::MemoryTag::Markup});
        if (mem != nullptr) {
            state_->reloadCoordinator = new (mem) Markup::ReloadCoordinator(*this, alloc);
        }
    }
    if (state_->reloadCoordinator != nullptr) {
        static_cast<void>(state_->reloadCoordinator->Start(uri, availableSize));
    }
    return root;
}

bool View::SetContent(
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return false;
    }
    Base::Result<void> res = state_->mounted
        ? ReplaceViewDocument(
              *state_, std::move(document), availableSize)
        : MountViewDocument(
              *state_, std::move(document), availableSize);
    return res.HasValue();
}

bool View::SetContent(
    Base::Ref<FrameworkElement> root,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return false;
    }
    if (root) {
        Markup::XamlDocument document;
        if (GetGui().TakeLoadedDocument(*root, document)) {
            return SetContent(std::move(document), availableSize);
        }
    }
    if (state_->mounted) {
        Base::Result<void> unmounted = UnmountRoot(*state_);
        if (!unmounted) return false;
    }
    Base::Result<void> res = MountViewContent(
        *state_,
        Base::Ref<Base::Object>(std::move(root)),
        availableSize);
    return res.HasValue();
}

bool View::SetContent(
    Base::Ref<FrameworkElement> root) noexcept {
    const Aero::Size availableSize = state_ != nullptr
        ? state_->viewport.logicalSize
        : Aero::Size{};
    return SetContent(std::move(root), availableSize);
}

bool View::SetContent(
    Base::Ref<FrameworkElement> root,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return false;
    }
    if (!root || !document.IsValid()) {
        return false;
    }
    if (state_->mounted) {
        Base::Result<void> unmounted = UnmountRoot(*state_);
        if (!unmounted) return false;
    }

    Markup::LoaderResult next = Aero::Markup::TakeXamlDocument(document);
    if (!next.root) {
        next.Clear();
        return false;
    }
    Base::Result<Aero::UIElement*> documentRoot =
        state_->ResolveUIElement(
            *next.root, next.root->RuntimeType());
    if (!documentRoot) {
        next.Clear();
        return false;
    }
    Base::Result<Aero::UIElement*> hostRoot =
        state_->ResolveUIElement(*root, root->RuntimeType());
    if (!hostRoot) {
        next.Clear();
        return false;
    }

    // Attach the loaded document root (for example a UserControl) as a visual
    // child of the host root (for example the wrapping Window). The host is
    // kept as the view root so the app-facing Window remains the mounted root.
    if (auto* contentHost = ::Aero::TryCast<Controls::ContentControl>(hostRoot.Value())) {
        Aero::Markup::VisualEdge edge;
        edge.parent = hostRoot.Value();
        edge.child = documentRoot.Value();
        next.visualContent.mountEdges.PushBack(std::move(edge));

        contentHost->SetOwnedContent(
            next.root,
            *documentRoot.Value());
    } else if (auto* panelHost = ::Aero::TryCast<Controls::Panel>(hostRoot.Value())) {
        Aero::Markup::VisualEdge edge;
        edge.parent = hostRoot.Value();
        edge.child = documentRoot.Value();
        next.visualContent.mountEdges.PushBack(std::move(edge));

        if (auto* uiElement = ::Aero::TryCast<Aero::UIElement>(documentRoot.Value())) {
            if (auto childElement = Base::Ref<Aero::UIElement>::TryFromBorrowed(*uiElement)) {
                panelHost->GetChildren().Add(std::move(childElement));
            }
        }
    } else if (auto* decoratorHost = ::Aero::TryCast<Controls::Decorator>(hostRoot.Value())) {
        Aero::Markup::VisualEdge edge;
        edge.parent = hostRoot.Value();
        edge.child = documentRoot.Value();
        next.visualContent.mountEdges.PushBack(std::move(edge));

        decoratorHost->SetOwnedChild(
            next.root,
            *documentRoot.Value());
    } else {
        next.Clear();
        return false;
    }

    next.root = std::move(root);
    state_->loadedDocument = std::move(next);
    Base::Result<void> res = MountRoot(
        *state_,
        state_->loadedDocument.root, availableSize);
    return res.HasValue();
}

bool View::SetViewport(
    const ViewViewport& viewport) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return false;
    }
    if (!ValidateViewport(viewport)) return false;
    Base::Result<void> applied = state_->ApplyViewport(viewport);
    return applied.HasValue();
}

void View::SetSize(
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) return;
    Base::Result<ViewViewport> viewport =
        Aero::MakeLogicalViewport(
            availableSize,
            state_->viewport.dpiScale);
    if (!viewport) return;
    SetViewport(viewport.Value());
}

void View::SetSize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    SetSize(Aero::Size{
        static_cast<double>(width),
        static_cast<double>(height)});
}

Aero::Size View::GetSize() const noexcept {
    return state_ != nullptr ? state_->viewport.logicalSize : Aero::Size{};
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
    if (state_->reloadCoordinator != nullptr && state_->reloadCoordinator->IsActive()) {
        static_cast<void>(state_->reloadCoordinator->Poll());
    }
    const ::Aero::Render::RenderFrame* before =
        ViewFrame::CurrentFrame(*this);
    const std::uint64_t beforeVersion =
        before != nullptr ? before->Version() : 0U;
    double elapsedSeconds = 0.0;
    if (hasUpdateTime_) {
        elapsedSeconds = timeInSeconds - updateTimeSeconds_;
    } else {
        hasUpdateTime_ = true;
    }
    updateTimeSeconds_ = timeInSeconds;
    const double elapsedMicrosecondsD = std::min(
        elapsedSeconds * 1000000.0,
        static_cast<double>(UINT64_MAX));
    const std::uint64_t elapsed =
        static_cast<std::uint64_t>(std::max(0.0, elapsedMicrosecondsD));
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
        ViewFrame::CurrentFrame(*this);
    return after != nullptr && after->Version() != 0U &&
        after->Version() != beforeVersion;
}

void View::Activate() noexcept {
    active_ = true;
    hasUpdateTime_ = false;
    if (state_ != nullptr && state_->Input() != nullptr) {
        Input::DeviceState::SetActiveRouter(state_->Input());
    }
}

void View::Deactivate() noexcept {
    active_ = false;
    hasUpdateTime_ = false;
    if (state_ != nullptr && state_->Input() != nullptr &&
        Input::DeviceState::ActiveRouter() == state_->Input()) {
        Input::DeviceState::SetActiveRouter(nullptr);
    }
}

IRenderer& View::GetRenderer() noexcept {
    return state_->publicRenderer;
}

const IRenderer& View::GetRenderer() const noexcept {
    return state_->publicRenderer;
}

CommittedFrameInfo View::GetCommittedFrameInfo() const noexcept {
    CommittedFrameInfo info;
    if (state_ == nullptr || state_->RenderTree() == nullptr) return info;
    const ::Aero::Render::RenderFrame& frame =
        state_->RenderTree()->CurrentFrame();
    if (frame.Version() == 0U) return info;
    const ::Aero::Render::RenderDiagnostics diagnostics =
        state_->RenderTree()->Diagnostics();
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
