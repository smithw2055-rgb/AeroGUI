#pragma once

// Source-only View hub state. Not installed under include/Aero.
// ViewFrame is frame/POD ownership; ElementTree is the service hub
// (Layout/Bindings/Styles/…). Domain work lives on the engines,
// OverlayHost, FocusHost, ResourceHost, and free functions in
// ViewDocuments.cpp / ViewFrame.cpp.

#include <Aero/View.hpp>
#include <Aero/Gui.hpp>
#include <AeroAudio/Audio.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include "gui/markup/XamlSchema.hpp"
#include "gui/ViewRenderer.hpp"
#include <Aero/FrameworkElement.hpp>
#include "gui/media/ImageCache.hpp"
#include <AeroRender/RenderTarget.hpp>

#include <Aero/VisualStateManager.hpp>
#include "gui/controls/TextBlockLayout.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/controls/ItemContainerGeneratorFactory.hpp"
// NOTE: <Aero/Controls.hpp> umbrella intentionally not included here.
// Source-only text layout is gui/controls/TextBlockLayout.hpp above.

#include <AeroRender/RenderDevice.hpp>

#include <cmath>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Threading {
class Dispatcher;
}

namespace Aero::Markup {
class ReloadCoordinator;
}

namespace Aero {

namespace MediaAnimation = ::Aero::Media::Animation;

inline Base::Status ViewInvalidState(const char* message) noexcept {
    return Base::Status::InvalidState(message);
}

inline Base::Status ViewNotInitialized(const char* message) noexcept {
    return Base::Status::NotInitialized(message);
}

inline bool ValidateViewport(
    const ViewViewport& viewport) noexcept {
    if (!IsValidLayoutSize(viewport.logicalSize) ||
        !std::isfinite(viewport.dpiScale) ||
        viewport.dpiScale <= 0.0 ||
        ((viewport.logicalSize.width == 0.0) !=
            (viewport.pixelWidth == 0U)) ||
        ((viewport.logicalSize.height == 0.0) !=
            (viewport.pixelHeight == 0U))) {
        return false;
    }
    return true;
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
    if (!ValidateViewport(viewport)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View viewport is invalid");
    }
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

using Base::AllocateObject;
using Base::FreeObject;

class InteractivityEngine;
class StoryboardHost;
class OverlayHost;
class FocusHost;
class ResourceHost;
struct ViewEngines;
namespace Text { class TextPipeline; }
namespace Render { class RenderTree; }

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

struct ViewFrame {
    static const ::Aero::Render::RenderFrame* CurrentFrame(
        const View& view) noexcept;

    struct FragmentMount {

        Controls::ContentControl* host = nullptr;
        Markup::LoaderResult document;
        Aero::ElementAttachment rootEdge;
    };

    ViewFrame(
        View& owner,
        Gui& guiOwner,
        Base::IAllocator& value,
        Base::Ref<GuiRuntime> guiState) noexcept;

    // Composition roots and Gui-owned services.
    Base::IAllocator* allocator = nullptr;
    Gui* guiOwner = nullptr;
    Base::Ref<GuiRuntime> gui;
    ViewRenderer publicRenderer;
    RenderingEventHandler renderingHandlers;
    Audio::Engine audio;
    ::Aero::Threading::Dispatcher* dispatcher = nullptr;
    GuiSchema* schemaBundle = nullptr;
    Markup::DocumentCache* documentCache = nullptr;
    Markup::XamlProviderRegistry* xamlProviders = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    ViewOptions options;
    // Frame/device state. These are direct values; ViewFrame remains the sole
    // owner and no forwarding object is introduced.
    Base::Status updateStatus;
    Base::Status rendererStatus;
    Base::Ref<RenderDevice> device;
    std::uint64_t deviceGeneration = 0U;
    ViewViewport viewport;

    // Frame ownership + ElementTree hub.
    // Domain callers reach engines through direct inline accessors that forward
    // to engines. ViewFrame keeps ownership of engines without redundant mirrors.
    ViewEngines* engines = nullptr;

    Meta::EffectiveValueEngine* Values() const noexcept;
    Aero::ElementTree* Tree() const noexcept;
    Aero::Media::ImageCache* Images() const noexcept;
    Aero::Text::TextPipeline* Text() const noexcept;
    InteractivityEngine* Interactivity() const noexcept;
    StoryboardHost* Storyboards() const noexcept;
    OverlayHost* Overlays() const noexcept;
    FocusHost* Focus() const noexcept;
    ResourceHost* Resources() const noexcept;
    Aero::LayoutEngine* Layout() const noexcept;
    ::Aero::Render::RenderTree* RenderTree() const noexcept;
    Aero::BindingEngine* Bindings() const noexcept;
    Aero::StyleEngine* Styles() const noexcept;
    Aero::EventRouter* Events() const noexcept;
    Aero::InputManager* Input() const noexcept;
    Aero::AnimationEngine* Animations() const noexcept;
    VisualStateManager* VisualStates() const noexcept;
    Aero::Controls::TemplateEngine* Templates() const noexcept;

    // Mount, provider-generation, and resource-layer state.
    Markup::Schema* schema = nullptr;
    Aero::RootAttachment rootAttachment;
    Aero::Media::Visual* attachedRootVisual = nullptr;
    Aero::UIElement* attachedRootLayout = nullptr;
    Aero::FrameworkElement* attachedRootRender = nullptr;
    std::uint64_t seenTextureProviderChange = 0U;
    std::uint64_t seenFontProviderChange = 0U;



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
    Base::Vector<Controls::ItemsControl*>
        trackedItemsControls;
    Base::Vector<Aero::VisualHandle>
        pendingGeneratedVisuals;
    bool deferGeneratedActivation = false;

    Markup::LoaderResult loadedDocument;
    Markup::ReloadCoordinator* reloadCoordinator = nullptr;
    Base::Vector<FragmentMount*> fragmentMounts;
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

    Aero::Render::MeshResources*
    GetMeshResources() noexcept;

    Aero::Render::ImageResources*
    GetImageResources() noexcept;

    Aero::Media::Visual* RootVisual() noexcept;

    Aero::Media::Visual* ResolveVisual(
        Base::Object& object, Meta::TypeId type) noexcept;

    Aero::UIElement* ResolveUIElement(
        Base::Object& object, Meta::TypeId type) noexcept;

    Aero::FrameworkElement* ResolveFrameworkElement(
        Base::Object& object, Meta::TypeId type) noexcept;

    static Base::Object* FindNameForElement(
        void* context,
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId,
        const DependencyObject* requester = nullptr) noexcept;

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

    Base::Result<void> VisitAndAttach(
        Aero::Media::Visual& rootVisual) noexcept;



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
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/LogicalTreeHelper.hpp>

namespace Aero {

template <typename TFn>
inline void ForEachVisualAndLogicalChild(Aero::Media::Visual& node, TFn&& fn) {
    for (Aero::Media::Visual* child : node.RenderChildren()) {
        if (child != nullptr) {
            fn(child);
        }
    }
    const std::uint32_t logicalCount = Aero::LogicalTreeHelper::GetChildrenCount(node);
    for (std::uint32_t i = 0U; i < logicalCount; ++i) {
        if (DependencyObject* child = Aero::LogicalTreeHelper::GetChild(node, i)) {
            if (auto* childVis = ::Aero::TryCast<Aero::Media::Visual>(child)) {
                if (childVis->GetVisualParent() != &node) {
                    fn(childVis);
                }
            }
        }
    }
}

Base::Result<void> ApplyViewUi(
    ViewFrame& state,
    Aero::Media::Visual& root) noexcept;
void DetachViewUi(
    ViewFrame& state,
    Aero::Media::Visual* root,
    Base::Span<Aero::Media::Visual* const> declarationNodes) noexcept;
inline void DetachViewUi(ViewFrame& state) noexcept {
    DetachViewUi(
        state,
        state.RootVisual(),
        {state.loadedDocument.visualContent.nodes.Data(),
         state.loadedDocument.visualContent.nodes.Size()});
}
Base::Result<std::uint32_t> ExecuteViewFrame(
    ViewFrame& state,
    View& view) noexcept;

Base::Result<void> LoadViewResources(
    ViewFrame& state,
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode = ResourceLoadMode::Replace,
    Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
Base::Result<void> LoadViewCompiledResources(
    ViewFrame& state,
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept;
void SetViewResourceDictionary(
    ViewFrame& state,
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept;
Base::Result<void> LoadViewBuiltInTheme(
    ViewFrame& state,
    BuiltInTheme theme) noexcept;

void ClearLoadedDocument(ViewFrame& state) noexcept;
Base::Result<void> BeginDocumentLoad(ViewFrame& state) noexcept;
Base::Result<Markup::XamlReaderSettings> XamlSettings(
    ViewFrame& state,
    bool deferredEffects = false,
    const Markup::XamlReaderSettings* override = nullptr) noexcept;
Base::Result<void> ValidateDocumentRoot(
    ViewFrame& state,
    const Base::Ref<Base::Object>& requestedRoot) noexcept;
Base::Result<void> MountRoot(
    ViewFrame& state,
    Base::Ref<Base::Object> requestedRoot,
    Aero::Size availableSize) noexcept;
Base::Result<void> DetachFragment(
    ViewFrame& state,
    ViewFrame::FragmentMount& fragment) noexcept;
Base::Result<void> UnmountFragmentAt(
    ViewFrame& state,
    std::uint32_t index) noexcept;
Base::Result<void> UnmountAllFragments(ViewFrame& state) noexcept;
Base::Result<void> DetachMountedRoot(
    ViewFrame& state,
    bool clearDocument) noexcept;
Base::Result<void> UnmountRoot(ViewFrame& state) noexcept;

Base::Result<void> MountViewContent(
    ViewFrame& state,
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept;
Base::Result<void> MountViewDocument(
    ViewFrame& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept;
Base::Result<void> ReplaceViewDocument(
    ViewFrame& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept;
Base::Result<std::uint32_t> AdvanceViewClocks(
    ViewFrame& state,
    std::uint64_t elapsedMicroseconds) noexcept;
Base::Result<void> MountViewFragment(
    ViewFrame& state,
    Controls::ContentControl& host,
    Markup::XamlDocument&& document) noexcept;
Base::Result<void> UnmountViewFragment(
    ViewFrame& state,
    Controls::ContentControl& host) noexcept;
// Binds deferred LoadComponent effects into a View that already contains the
// component root (UserControl.InitializeComponent after the host Window is
// mounted). LoadComponent itself is View-independent; this adopts the pending
// document's bindings, visual edges, and Loaded storyboards.
Base::Result<void> AdoptLoadedComponent(
    ViewFrame& state,
    Markup::LoaderResult&& document) noexcept;

} // namespace Aero

#include "gui/templates/TemplateInstance.hpp"
#include "gui/text/TextPipeline.hpp"
#include "render/RenderTree.hpp"

namespace Aero {

struct ViewEngines {
    Meta::ObjectFactoryScope objectFactory;
    Meta::EffectiveValueEngine values;
    Aero::AnimationEngine animations;
    Aero::ElementTree tree;
    Aero::LayoutEngine layout;
    ::Aero::Render::RenderTree renderTree;
    Aero::Media::ImageCache images;
    Aero::Text::TextPipeline text;
    Aero::BindingEngine bindings;
    Aero::EventRouter events;
    Aero::InputManager input;
    InteractivityEngine interactivity;
    StoryboardHost storyboards;
    ResourceHost resources;
    Aero::StyleEngine styles;
    Aero::Controls::TemplateEngine templates;
    OverlayHost overlays;
    FocusHost focus;
    Base::Ref<VisualStateManager> visualStates;

    ViewEngines(
        ViewFrame& frame,
        Base::IAllocator* allocator,
        ::Aero::Threading::Dispatcher& dispatcher,
        ::Aero::Meta::Registry& metadata) noexcept;
    ~ViewEngines() noexcept;
};

inline Meta::EffectiveValueEngine* ViewFrame::Values() const noexcept {
    return engines != nullptr ? &engines->values : nullptr;
}
inline Aero::ElementTree* ViewFrame::Tree() const noexcept {
    return engines != nullptr ? &engines->tree : nullptr;
}
inline Aero::Media::ImageCache* ViewFrame::Images() const noexcept {
    return engines != nullptr ? &engines->images : nullptr;
}
inline Aero::Text::TextPipeline* ViewFrame::Text() const noexcept {
    return engines != nullptr ? &engines->text : nullptr;
}
inline InteractivityEngine* ViewFrame::Interactivity() const noexcept {
    return engines != nullptr ? &engines->interactivity : nullptr;
}
inline StoryboardHost* ViewFrame::Storyboards() const noexcept {
    return engines != nullptr ? &engines->storyboards : nullptr;
}
inline OverlayHost* ViewFrame::Overlays() const noexcept {
    return engines != nullptr ? &engines->overlays : nullptr;
}
inline FocusHost* ViewFrame::Focus() const noexcept {
    return engines != nullptr ? &engines->focus : nullptr;
}
inline ResourceHost* ViewFrame::Resources() const noexcept {
    return engines != nullptr ? &engines->resources : nullptr;
}
inline Aero::LayoutEngine* ViewFrame::Layout() const noexcept {
    return engines != nullptr ? &engines->layout : nullptr;
}
inline ::Aero::Render::RenderTree* ViewFrame::RenderTree() const noexcept {
    return engines != nullptr ? &engines->renderTree : nullptr;
}
inline Aero::BindingEngine* ViewFrame::Bindings() const noexcept {
    return engines != nullptr ? &engines->bindings : nullptr;
}
inline Aero::StyleEngine* ViewFrame::Styles() const noexcept {
    return engines != nullptr ? &engines->styles : nullptr;
}
inline Aero::EventRouter* ViewFrame::Events() const noexcept {
    return engines != nullptr ? &engines->events : nullptr;
}
inline Aero::InputManager* ViewFrame::Input() const noexcept {
    return engines != nullptr ? &engines->input : nullptr;
}
inline Aero::AnimationEngine* ViewFrame::Animations() const noexcept {
    return engines != nullptr ? &engines->animations : nullptr;
}
inline VisualStateManager* ViewFrame::VisualStates() const noexcept {
    return engines != nullptr ? engines->visualStates.Get() : nullptr;
}
inline Aero::Controls::TemplateEngine* ViewFrame::Templates() const noexcept {
    return engines != nullptr ? &engines->templates : nullptr;
}

} // namespace Aero
