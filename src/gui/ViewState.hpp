#pragma once

// Source-only View hub state. Not installed under include/Aero.
// ViewState is data plus short helpers; domain methods are defined out of line.

#include <Aero/View.hpp>
#include <Aero/Gui.hpp>
#include <AeroAudio/Audio.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Diagnostics/Rendering.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Base/Hash.hpp>
#include "gui/GuiData.hpp"
#include "gui/ViewRenderer.hpp"
#include <Aero/FrameworkElement.hpp>
#include "gui/media/ImageCache.hpp"
#include "gui/text/TextPipeline.hpp"
#include <AeroRender/RenderTarget.hpp>

#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"
#include "gui/controls/ControlBehavior.hpp"
#include "gui/meta/MetadataState.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/input/InputState.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/media/MediaState.hpp"

#include <Aero/Controls.hpp>
#include <Aero/Documents.hpp>
#include "gui/controls/Metadata.hpp"
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>

#include <Aero/InputInterop.hpp>
#include <Aero/Data/Binding.hpp>
#include "gui/media/AnimationModel.hpp"
#include <Aero/Media/Animation.hpp>
#include <Aero/Input.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/BuiltinThemes.generated.hpp>

#include "gui/templates/DataTemplateTriggerState.hpp"
#include <AeroRender/RenderDevice.hpp>
#include "render/RenderTree.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
    Base::Result<void> appended = text.Append(name);
    if (!appended) return appended.GetStatus();
    return Base::ResourceUri::Parse(text.View());
}

template<class T>
inline Base::Result<const T*> ResolveUiValue(
    Aero::FrameworkElement& element,
    Meta::DependencyPropertyHandle property,
    const Aero::ResourceEnvironment& resources,
    const char* incompatibleMessage) noexcept {
    Base::Result<Meta::Value> explicitValue = element.GetValue(property);
    if (!explicitValue) return explicitValue.GetStatus();
    if (explicitValue.Value().Kind() == Meta::ValueKind::Object &&
        !explicitValue.Value().IsNullObject() &&
        explicitValue.Value().AsObject()) {
        Base::Object* object = explicitValue.Value().AsObject().Get();
        if (object->RuntimeType() != T::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument, incompatibleMessage);
        }
        return static_cast<const T*>(object);
    }

    Base::Result<Meta::Value> implicit = Aero::ResourceResolver::Lookup(
        &element, element.RuntimeType(), nullptr, resources);
    if (!implicit) {
        return implicit.GetStatus().code == Base::ErrorCode::NotFound
            ? Base::Result<const T*>(static_cast<const T*>(nullptr))
            : Base::Result<const T*>(implicit.GetStatus());
    }
    if (implicit.Value().Kind() != Meta::ValueKind::Object ||
        implicit.Value().IsNullObject() || !implicit.Value().AsObject() ||
        implicit.Value().AsObject()->RuntimeType() != T::StaticTypeId()) {
        return static_cast<const T*>(nullptr);
    }
    return static_cast<const T*>(implicit.Value().AsObject().Get());
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
    Base::Result<std::uint32_t> ExecuteFrame(View& view) noexcept;

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
    Markup::XamlRuntime* xamlRuntime = nullptr;
    GuiSchema* schemaBundle = nullptr;
    Markup::DocumentCache* documentCache = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    ViewOptions options;
    // Frame/device state. These are direct values; ViewState remains the sole
    // owner and no forwarding object is introduced.
    Base::Status updateStatus;
    Base::Status rendererStatus;
    Base::Ref<RenderDevice> device;
    std::uint64_t deviceGeneration = 0U;
    ViewViewport viewport;

    // Business-domain engines allocated and destroyed by this ViewState.
    Meta::ObjectFactoryScope* objectFactory = nullptr;
    Meta::EffectiveValueEngine* values = nullptr;
    Aero::AnimationEngine* animations = nullptr;
    Aero::ElementTree* tree = nullptr;
    Aero::LayoutEngine* layout = nullptr;
    ::Aero::Render::RenderTree* renderer = nullptr;
    Aero::Media::ImageCache* images = nullptr;
    Aero::Text::TextPipeline* text = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    Aero::EventRouter* events = nullptr;
    Aero::InputRouter* input = nullptr;

    Aero::Controls::TemplateEngine* templates = nullptr;
    VisualStateManager* visualStates = nullptr;
    Aero::StyleEngine* styles = nullptr;
    InteractivityEngine* interactivity = nullptr;
    StoryboardHost* storyboards = nullptr;

    // Mount, provider-generation, and resource-layer state.
    Markup::Schema* schema = nullptr;
    Aero::RootAttachment rootAttachment;
    Aero::Media::Visual* attachedRootVisual = nullptr;
    Aero::UIElement* attachedRootLayout = nullptr;
    Aero::FrameworkElement* attachedRootRender = nullptr;
    std::uint64_t seenTextureProviderChange = 0U;
    std::uint64_t seenFontProviderChange = 0U;
    Aero::ResourceDictionary applicationResources;
    Aero::ResourceDictionary themeResources;
    Aero::ResourceDictionary systemResources;
    Aero::ResourceDictionary dynamicResourceEnvironment;

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

    Base::Vector<Base::WeakRef<Aero::UIElement>>
        pendingFocusTargets;
    Base::Result<void> QueueFocus(Aero::UIElement& target) noexcept;
    Base::Result<std::uint32_t> ProcessPendingFocus() noexcept;
    Base::Vector<Controls::ItemContainerGenerator*>
        itemGenerators;
    Base::Vector<Aero::VisualHandle>
        pendingGeneratedVisuals;
    bool deferGeneratedActivation = false;
    Base::Vector<Aero::FrameworkElement*>
        renderOverlays;
    Base::Vector<Aero::UIElement*>
        inputOverlays;
    Base::Vector<Aero::Base::Transform2D>
        overlayTransforms;
    Base::Ref<Controls::ToolTip>
        pendingToolTip;
    Base::Ref<Controls::ToolTip>
        activeToolTip;
    Base::Ref<Aero::UIElement>
        toolTipTarget;
    Base::Ref<Aero::UIElement>
        overlayFocusReturn;
    std::uint32_t toolTipElapsed = 0U;
    std::uint32_t toolTipVisibleElapsed = 0U;

    Markup::LoaderResult loadedDocument;
    Base::Vector<FragmentMount> fragmentMounts;
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

    Base::Result<void> BeginDocumentLoad() noexcept;

    Base::Result<Markup::XamlReaderSettings> XamlSettings(
        bool deferredEffects = false,
        const Markup::XamlReaderSettings* override = nullptr) noexcept;

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

    void ClearLoadedDocument() noexcept;

    Aero::ResourceEnvironment ResourceEnvironment() const noexcept;

    Base::Result<Aero::ResourceDictionary*>
    ResolveResourceLayer(
        ResourceLayer layer) noexcept;

    Base::Result<void> RebuildDynamicResourceEnvironment() noexcept;

    void DetachUi() noexcept;

    Aero::Media::Visual* RootVisual() noexcept;

    Base::Result<void> SynchronizeOverlays() noexcept;

    void ClearOverlays() noexcept;

    void CloseAllOverlays() noexcept;

    static bool IsVisualDescendantOrSelf(
        const Aero::Media::Visual& root,
        const Aero::Media::Visual& target)
        noexcept;

    Base::Result<void> RestoreOverlayFocus()
        noexcept;

    Base::Result<void> DismissOverlaysForPointer(
        const Input::PointerInput& input,
        Aero::UIElement* target)
        noexcept;

    Base::Result<bool> DismissTopOverlayForEscape()
        noexcept;

    Base::Result<void> OpenContextMenuForPointer(
        const Input::PointerInput& input,
        Aero::UIElement* hitTarget)
        noexcept;

    Base::Result<void> UpdateToolTipForPointer(
        const Input::PointerInput& input,
        Aero::UIElement* hitTarget)
        noexcept;

    Base::Result<std::uint32_t>
    AdvanceToolTipTime(
        std::uint32_t elapsedMilliseconds)
        noexcept;

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

    Base::Result<void> ApplyUi(Aero::Media::Visual& root) noexcept;

    void DetachUi(
        Aero::Media::Visual* root,
        Base::Span<Aero::Media::Visual* const> declarationNodes) noexcept;

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

    Base::Result<void> CommitResourceLayer(
        Markup::XamlDocument document,
        Aero::ResourceDictionary& target,
        bool merge) noexcept;

    Base::Result<void> LoadResourceLayer(
        Base::StringView uri,
        Aero::ResourceDictionary& target,
        Diagnostics::IDiagnosticSink* diagnostics,
        bool merge = false) noexcept;

    Base::Result<void> LoadCompiledResourceLayer(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        Aero::ResourceDictionary& target,
        bool merge = false) noexcept;

    Base::Result<void> ValidateDocumentRoot(
        const Base::Ref<Base::Object>& requestedRoot) noexcept;

    Base::Result<void> MountRoot(
        Base::Ref<Base::Object> requestedRoot,
        Aero::Size availableSize) noexcept;

    Base::Result<void> DetachFragment(
        FragmentMount& fragment) noexcept;

    Base::Result<void> UnmountFragmentAt(
        std::uint32_t index) noexcept;

    Base::Result<void> UnmountAllFragments() noexcept;

    Base::Result<void> DetachMountedRoot(
        bool clearDocument) noexcept;

    Base::Result<void> UnmountRoot() noexcept;
};

} // namespace Aero

#include "gui/interactivity/InteractivityEngine.hpp"
#include "gui/media/StoryboardHost.hpp"

namespace Aero {

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

} // namespace Aero
