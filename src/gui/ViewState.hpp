#pragma once

// Source-only View hub state. Not installed under include/Aero.
// ViewState is frame/POD ownership; ElementTree is the service hub
// (Layout/Bindings/Styles/…). Domain work lives on the engines,
// OverlayHost, FocusHost, ResourceHost, and free functions in
// ViewDocuments.cpp / ViewFrame.cpp.

#include <Aero/View.hpp>
#include <Aero/Gui.hpp>
#include <AeroAudio/Audio.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Diagnostics/Rendering.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/Conditions.hpp>
#include "gui/GuiState.hpp"
#include "gui/ViewRenderer.hpp"
#include <Aero/FrameworkElement.hpp>
#include "gui/media/ImageCache.hpp"
#include "gui/text/TextPipeline.hpp"
#include <AeroRender/RenderTarget.hpp>

#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>
#include "gui/controls/ControlBehavior.hpp"
#include "gui/controls/TextBlockLayout.hpp"
#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputState.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/media/MediaHelpers.hpp"

// NOTE: <Aero/Controls.hpp> umbrella intentionally not included here.
// ContentControl/ItemsControl/ItemContainerGenerator are already available
// via gui/internal/AeroGuiInternal.hpp; source-only Controls types via
// gui/controls/{ControlBehavior,TextBlockLayout,Metadata}.hpp below.
#include "gui/controls/Metadata.hpp"

#include "gui/templates/DataTemplateTriggerState.hpp"
#include <AeroRender/RenderDevice.hpp>
#include "render/RenderTree.hpp"

#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

namespace MediaAnimation = ::Aero::Media::Animation;

inline Base::Status ViewInvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

inline Base::Status AeroNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

inline Base::Status ViewApiInvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

inline Base::Status ViewNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

inline Base::Result<void> ValidateViewport(
    const ViewViewport& viewport) noexcept {
    if (!IsValidLayoutSize(viewport.logicalSize) ||
        !std::isfinite(viewport.dpiScale) ||
        viewport.dpiScale <= 0.0 ||
        ((viewport.logicalSize.width == 0.0) !=
            (viewport.pixelWidth == 0U)) ||
        ((viewport.logicalSize.height == 0.0) !=
            (viewport.pixelHeight == 0U))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View viewport is invalid");
    }
    return {};
}

inline Base::Result<ViewViewport> MakeLogicalViewport(
    Size logicalSize,
    double dpiScale) noexcept {
    if (!IsValidLayoutSize(logicalSize) ||
        !std::isfinite(dpiScale) || dpiScale <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View viewport is invalid");
    }
    ViewViewport viewport;
    viewport.logicalSize = logicalSize;
    viewport.dpiScale = dpiScale;

    const double pixelWidth = logicalSize.width * dpiScale;
    const double pixelHeight = logicalSize.height * dpiScale;
    constexpr double PixelLimit =
        static_cast<double>((std::numeric_limits<std::uint32_t>::max)());
    if (!std::isfinite(pixelWidth) || !std::isfinite(pixelHeight) ||
        pixelWidth > PixelLimit || pixelHeight > PixelLimit) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "View viewport pixel dimensions are out of range");
    }
    viewport.pixelWidth = static_cast<std::uint32_t>(
        std::floor(pixelWidth + 0.5));
    viewport.pixelHeight = static_cast<std::uint32_t>(
        std::floor(pixelHeight + 0.5));
    Base::Result<void> valid = ValidateViewport(viewport);
    if (!valid) return valid.GetStatus();
    return viewport;
}

inline Base::Result<Base::ResourceUri> BuiltInThemeUri(
    Base::StringView name) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(
        Base::StringView(
            "pack://application:,,,/Aero.Themes;component/"));
    if (!assigned) return assigned.GetStatus();
    text.Append(name);
    return Base::ResourceUri::Parse(text.View());
}

template<class T, class... TArgs>
inline Base::Result<void> AllocateObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& output,
    TArgs&&... arguments) noexcept {
    if (output != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Object is already allocated");
    }
    void* memory = allocator.Allocate({
        sizeof(T), alignof(T), tag});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Object allocation failed");
    }
    output = new (memory) T(
        std::forward<TArgs>(arguments)...);
    return {};
}

template<class T>
inline void FreeObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& object) noexcept {
    if (object == nullptr) return;
    object->~T();
    allocator.Deallocate(
        object, sizeof(T), alignof(T), tag);
    object = nullptr;
}

class InteractivityEngine;
class StoryboardHost;
class OverlayHost;
class FocusHost;
class ResourceHost;

struct ViewFrameResult {
    struct Layout {
        std::uint64_t passVersion = 0U;
        std::uint32_t measuredCount = 0U;
        std::uint32_t arrangedCount = 0U;
        std::uint32_t pendingMeasureCount = 0U;
        std::uint32_t pendingArrangeCount = 0U;
    };
    struct Render {
        std::uint64_t snapshotVersion = 0U;
        std::uint32_t nodeCount = 0U;
        std::uint32_t commandCount = 0U;
        std::uint32_t glyphCommandCount = 0U;
        std::uint32_t dirtyCount = 0U;
        std::uint64_t snapshotHash = 0U;
        std::uint32_t drawPacketCount = 0U;
        std::uint32_t batchCount = 0U;
        std::uint32_t drawCallCount = 0U;
        std::uint32_t mergedPacketCount = 0U;
        std::uint32_t barrierCount = 0U;
        std::uint32_t instanceCount = 0U;
        std::uint32_t stateBindingCount = 0U;
        bool batchingEnabled = true;
    };

    std::uint64_t frameNumber = 0U;
    std::uint32_t callbackCount = 0U;
    Layout layout;
    Render render;
};

struct ViewState {
    static const ::Aero::Render::RenderFrame* CurrentFrame(
        const View& view) noexcept;

    struct FragmentMount {

        Controls::ContentControl* host = nullptr;
        Markup::LoaderResult document;
        Aero::ElementAttachment rootEdge;
    };

    ViewState(
        View& owner,
        Gui& guiOwner,
        Base::IAllocator& value,
        Base::Ref<Base::Object> guiState) noexcept;

    // Composition roots and Gui-owned services.
    Base::IAllocator* allocator = nullptr;
    Gui* guiOwner = nullptr;
    Base::Ref<Base::Object> gui;
    ViewRenderer publicRenderer;
    RenderingEventHandler renderingHandlers;
    Audio::Engine audio;
    ::Aero::Threading::Dispatcher* dispatcher = nullptr;
    GuiSchema* schemaBundle = nullptr;
    Markup::DocumentCache* documentCache = nullptr;
    Markup::XamlProviderRegistry* xamlProviders = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    ViewOptions options;
    // Frame/device state. These are direct values; ViewState remains the sole
    // owner and no forwarding object is introduced.
    Base::Status updateStatus;
    Base::Status rendererStatus;
    Base::Ref<RenderDevice> device;
    std::uint64_t deviceGeneration = 0U;
    ViewViewport viewport;

    // Frame ownership + ElementTree hub.
    // Domain callers reach Layout/Bindings/Styles/Animations/… through
    // ElementTree (VisualTree()); ViewState keeps ownership for hosts that are
    // not yet on the tree, and thin accessors that forward to tree once wired.
    Meta::ObjectFactoryScope* objectFactory = nullptr;
    Meta::EffectiveValueEngine* values = nullptr;
    Aero::ElementTree* tree = nullptr;
    Aero::Media::ImageCache* images = nullptr;
    Aero::Text::TextPipeline* text = nullptr;

    InteractivityEngine* interactivity = nullptr;
    StoryboardHost* storyboards = nullptr;
    OverlayHost* overlays = nullptr;
    FocusHost* focus = nullptr;
    ResourceHost* resources = nullptr;

    Aero::LayoutEngine* Layout() const noexcept {
        return tree != nullptr ? tree->Layout() : nullptr;
    }
    ::Aero::Render::RenderTree* RenderTree() const noexcept {
        return tree != nullptr ? tree->RenderTree() : nullptr;
    }
    Aero::BindingEngine* Bindings() const noexcept {
        return tree != nullptr ? tree->Bindings() : nullptr;
    }
    Aero::StyleEngine* Styles() const noexcept {
        return tree != nullptr ? tree->Styles() : nullptr;
    }
    Aero::EventRouter* Events() const noexcept {
        return tree != nullptr ? tree->Events() : nullptr;
    }
    Aero::InputRouter* Input() const noexcept {
        return tree != nullptr ? tree->Input() : nullptr;
    }
    Aero::AnimationEngine* Animations() const noexcept {
        return tree != nullptr ? tree->Animations() : nullptr;
    }
    VisualStateManager* VisualStates() const noexcept {
        return tree != nullptr ? tree->VisualStates() : nullptr;
    }
    Aero::Controls::TemplateEngine* Templates() const noexcept {
        return tree != nullptr ? tree->Templates() : nullptr;
    }

    // Mount, provider-generation, and resource-layer state.
    Markup::Schema* schema = nullptr;
    Aero::RootAttachment rootAttachment;
    Aero::Media::Visual* attachedRootVisual = nullptr;
    Aero::UIElement* attachedRootLayout = nullptr;
    Aero::FrameworkElement* attachedRootRender = nullptr;
    std::uint64_t seenTextureProviderChange = 0U;
    std::uint64_t seenFontProviderChange = 0U;

    // Interaction attachment state.
    ::Aero::Controls::ControlBehavior* controlBehaviors = nullptr;

    void ReportFrameFailure(
        Base::Status& slot,
        Base::Status status,
        std::uint16_t diagnosticNumber) noexcept;

    void ReportUpdateFailure(Base::Status status) noexcept;

    void ReportRendererFailure(Base::Status status) noexcept;

    void ClearUpdateFailure() noexcept;
    void ClearRendererFailure() noexcept;
    void RaiseFrameRendering(View& view) noexcept;

    Base::Vector<Controls::ItemContainerGenerator*>
        itemGenerators;
    Base::Vector<Aero::VisualHandle>
        pendingGeneratedVisuals;
    bool deferGeneratedActivation = false;

    Markup::LoaderResult loadedDocument;
    Base::Vector<FragmentMount> fragmentMounts;
    Base::Vector<FragmentMount*> componentMounts;
    const Aero::NameScope* activeFragmentNames = nullptr;

    bool HasAttachedRoot() const noexcept;

    Base::Result<void> AttachVisualGraph(
        ::Aero::Media::Visual& rootVisual,
        UIElement& rootLayout,
        FrameworkElement* rootRender,
        Base::Span<Aero::Markup::VisualEdge> edges,
        Size availableSize) noexcept;

    Base::Result<void> CompleteVisualEdges(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept;

    Base::Result<void> ResizeVisualRoot(Size availableSize) noexcept;

    Base::Result<void> ApplyViewport(
        const ViewViewport& next) noexcept;

    Base::Result<void> DetachVisualGraph(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept;
    Markup::LoadState loadContext;
    Base::Ref<Markup::EffectLifetime> effectLifetime;
    Base::Ref<Base::Object> root;
    std::uint64_t frameNumber = 0U;
    bool initialized = false;
    bool mounted = false;
    bool terminal = false;

    void AttachTextLayout(
        Aero::Media::Visual& node,
        ::Aero::Controls::TextBlockLayout* service,
        bool invalidate = false) noexcept;

    Aero::Render::MeshResources*
    GetMeshResources() noexcept;

    Aero::Render::ImageResources*
    GetImageResources() noexcept;

    void AttachPathResources(
        Aero::Media::Visual& node,
        Aero::Render::MeshResources* service,
        bool invalidate = false) noexcept;

    void VisitTextElements(
        Aero::Media::Visual* rootVisual,
        ::Aero::Controls::TextBlockLayout* service,
        bool invalidate = false,
        bool ancestorsVisible = true) noexcept;

    void VisitPaths(
        Aero::Media::Visual* rootVisual,
        Aero::Render::MeshResources* service,
        bool invalidate = false,
        bool ancestorsVisible = true) noexcept;

    static void TextLifecycleHook(
        const Aero::ElementTreeLifecycleEvent& event,
        void* context) noexcept;

    Aero::Media::Visual* RootVisual() noexcept;

    Base::Result<Aero::Media::Visual*> ResolveVisual(
        Base::Object& object, Meta::TypeId type) noexcept;

    Base::Result<Aero::UIElement*> ResolveUIElement(
        Base::Object& object, Meta::TypeId type) noexcept;

    Aero::FrameworkElement* ResolveFrameworkElement(
        Base::Object& object, Meta::TypeId type) noexcept;

    static Base::Object* FindNameForElement(
        void* context,
        Base::StringView name,
        Meta::TypeId expectedType) noexcept;

    Base::Result<void> CreateUiEngines() noexcept;

    static Base::Result<void> GeneratedItemSubtreeChanged(
        Aero::Media::Visual& root,
        Controls::ItemSubtreeChange change,
        void* context) noexcept;

    Base::Result<void>
    FlushGeneratedVisuals() noexcept;

    Base::Result<void> AttachItemGenerator(
        Controls::ItemsControl& itemsControl) noexcept;

    Base::Result<void> AttachPendingItemGenerators(
        Aero::Media::Visual& rootVisual) noexcept;

    void DestroyUiEngines() noexcept;

    Base::Result<void> VisitAndAttach(
        Aero::Media::Visual& rootVisual) noexcept;

    void ClearTextInputHosts(
        Aero::Media::Visual* node) noexcept;

    void ClearElementEvents(
        Aero::Media::Visual* node) noexcept;

    void BeginDestroyInteractions() noexcept;

    void FinishDestroyInteractions() noexcept;

    void DestroyInteractions() noexcept;

    Base::Result<void> CreateInteractions() noexcept;

    void Shutdown() noexcept;

    Base::Result<void> Initialize(
        const ViewOptions& requested) noexcept;
};

} // namespace Aero

#include "gui/interactivity/InteractivityEngine.hpp"
#include "gui/media/StoryboardHost.hpp"
#include "gui/input/OverlayHost.hpp"
#include "gui/styles/ResourceHost.hpp"

namespace Aero {

Base::Result<void> ApplyViewUi(
    ViewState& state,
    Aero::Media::Visual& root) noexcept;
void DetachViewUi(
    ViewState& state,
    Aero::Media::Visual* root,
    Base::Span<Aero::Media::Visual* const> declarationNodes) noexcept;
inline void DetachViewUi(ViewState& state) noexcept {
    DetachViewUi(
        state,
        state.RootVisual(),
        {state.loadedDocument.visualContent.nodes.Data(),
         state.loadedDocument.visualContent.nodes.Size()});
}
Base::Result<std::uint32_t> ExecuteViewFrame(
    ViewState& state,
    View& view) noexcept;

Base::Result<void> LoadViewResources(
    ViewState& state,
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode = ResourceLoadMode::Replace,
    Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
Base::Result<void> LoadViewCompiledResources(
    ViewState& state,
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept;
void SetViewResourceDictionary(
    ViewState& state,
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept;
Base::Result<void> LoadViewBuiltInTheme(
    ViewState& state,
    BuiltInTheme theme) noexcept;

void ClearLoadedDocument(ViewState& state) noexcept;
Base::Result<void> BeginDocumentLoad(ViewState& state) noexcept;
Base::Result<Markup::XamlReaderSettings> XamlSettings(
    ViewState& state,
    bool deferredEffects = false,
    const Markup::XamlReaderSettings* override = nullptr) noexcept;
Base::Result<void> ValidateDocumentRoot(
    ViewState& state,
    const Base::Ref<Base::Object>& requestedRoot) noexcept;
Base::Result<void> MountRoot(
    ViewState& state,
    Base::Ref<Base::Object> requestedRoot,
    Aero::Size availableSize) noexcept;
Base::Result<void> DetachFragment(
    ViewState& state,
    ViewState::FragmentMount& fragment) noexcept;
Base::Result<void> UnmountFragmentAt(
    ViewState& state,
    std::uint32_t index) noexcept;
Base::Result<void> UnmountAllFragments(ViewState& state) noexcept;
Base::Result<void> DetachMountedRoot(
    ViewState& state,
    bool clearDocument) noexcept;
Base::Result<void> UnmountRoot(ViewState& state) noexcept;

Base::Result<void> MountViewContent(
    ViewState& state,
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept;
Base::Result<void> MountViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept;
Base::Result<void> ReplaceViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept;
Base::Result<std::uint32_t> AdvanceViewClocks(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept;
Base::Result<void> MountViewFragment(
    ViewState& state,
    Controls::ContentControl& host,
    Markup::XamlDocument&& document) noexcept;
Base::Result<void> UnmountViewFragment(
    ViewState& state,
    Controls::ContentControl& host) noexcept;
// Binds deferred LoadComponent effects into a View that already contains the
// component root (UserControl.InitializeComponent after the host Window is
// mounted). LoadComponent itself is View-independent; this adopts the pending
// document's bindings, visual edges, and Loaded storyboards.
Base::Result<void> AdoptLoadedComponent(
    ViewState& state,
    Markup::LoaderResult&& document) noexcept;

} // namespace Aero
