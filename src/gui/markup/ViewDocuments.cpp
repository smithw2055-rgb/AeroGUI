#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

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

Base::Result<void> ViewState::BeginDocumentLoad() noexcept {
        if (!initialized) {
            return AeroNotInitialized(
                "View must be initialized before XAML loading");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View already owns a loaded document");
        }
        return {};
    }

Base::Result<Markup::XamlReaderSettings> ViewState::XamlSettings(
        bool deferredEffects,
        const Markup::XamlReaderSettings* override) noexcept {
        Markup::XamlReaderSettings result;
        if (override != nullptr) {
            result = *override;
        }
        loadContext.resources = &dynamicResourceEnvironment;
        loadContext.effectiveValues = values;
        loadContext.bindings = bindings;
        loadContext.fallbackResources =
            &dynamicResourceEnvironment;
        loadContext.documentCache = documentCache;
        loadContext.dispatcher = dispatcher;
        loadContext.dependencyProperties =
            &::Aero::MetadataPrivate::
                DependencyProperties(*metadata);
        loadContext.effectLifetime = effectLifetime;
        loadContext.effectCommitMode = deferredEffects
            ? Markup::EffectCommitMode::Deferred
            : Markup::EffectCommitMode::Immediate;
        return result;
    }

void ViewState::ClearLoadedDocument() noexcept {
        loadedDocument.Clear();
    }

Base::Object* ViewState::FindNameForElement(
        void* context,
        Base::StringView name,
        Meta::TypeId expectedType) noexcept {
        auto* runtime = static_cast<ViewState*>(context);
        if (runtime == nullptr || name.Empty()) return nullptr;
        Base::Object* object = runtime->activeFragmentNames != nullptr
            ? runtime->activeFragmentNames->Find(name)
            : nullptr;
        if (object == nullptr) {
            object = runtime->loadedDocument.names.Find(name);
        }
        if (object == nullptr) {
            for (ViewState::FragmentMount& fragment :
                 runtime->fragmentMounts) {
                object = fragment.document.names.Find(name);
                if (object != nullptr) break;
            }
        }
        if (object == nullptr || expectedType == Meta::InvalidTypeId) {
            return object;
        }
        return runtime->metadata != nullptr &&
            runtime->metadata->Types().IsAssignableFrom(
                expectedType, object->RuntimeType())
            ? object
            : nullptr;
    }

Base::Result<void> ViewState::CommitResourceLayer(
        Markup::XamlDocument document,
        Aero::ResourceDictionary& target,
        bool merge) noexcept {
        const Base::Ref<Base::Object>& rootObject =
            document.Root();
        if (!rootObject ||
            rootObject->RuntimeType() !=
                Aero::ResourceDictionary::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View resource document root must be ResourceDictionary");
        }
        auto& dictionary =
            static_cast<Aero::ResourceDictionary&>(
                *rootObject);
        if (merge) {
            Base::Result<void> merged =
                target.AddMerged(dictionary);
            if (!merged) return merged.GetStatus();
            Base::Result<void> rebuilt =
                RebuildDynamicResourceEnvironment();
            if (rebuilt) return {};
            Base::Result<bool> removed =
                target.RemoveMerged(dictionary);
            Base::Result<void> restored =
                removed && removed.Value()
                ? RebuildDynamicResourceEnvironment()
                : Base::Result<void>(
                      removed
                      ? Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "View resource merge rollback lost its dictionary")
                      : removed.GetStatus());
            return restored
                ? Base::Result<void>(rebuilt.GetStatus())
                : restored;
        }

        Aero::ResourceDictionary previous =
            std::move(target);
        target = std::move(dictionary);
        Base::Result<void> rebuilt =
            RebuildDynamicResourceEnvironment();
        if (rebuilt) return {};
        target = std::move(previous);
        Base::Result<void> restored =
            RebuildDynamicResourceEnvironment();
        return restored
            ? Base::Result<void>(rebuilt.GetStatus())
            : restored;
    }

Base::Result<void> ViewState::LoadResourceLayer(
        Base::StringView uri,
        Aero::ResourceDictionary& target,
        Diagnostics::IDiagnosticSink* diagnostics,
        bool merge) noexcept {
        if (!initialized) {
            return AeroNotInitialized(
                "View must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
        }
        Base::Result<Markup::XamlReaderSettings> loadOptions =
            XamlSettings();
        if (!loadOptions) {
            return loadOptions.GetStatus();
        }
        if (xamlRuntime == nullptr) {
            return AeroNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            xamlRuntime->Load(
            xamlRuntime->Providers(),
            &loadContext,
            allocator,
            uri, loadOptions.Value(), diagnostics);
        if (!loaded) {
            return loaded.GetStatus();
        }
        return CommitResourceLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

Base::Result<void> ViewState::LoadCompiledResourceLayer(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        Aero::ResourceDictionary& target,
        bool merge) noexcept {
        if (!initialized) {
            return AeroNotInitialized(
                "View must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
        }
        Base::Result<Markup::XamlReaderSettings> loadOptions =
            XamlSettings();
        if (!loadOptions) return loadOptions.GetStatus();
        if (xamlRuntime == nullptr) {
            return AeroNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            xamlRuntime->LoadCompiled(
                xamlRuntime->Providers(), &loadContext, allocator,
                bytes, originUri, loadOptions.Value());
        if (!loaded) return loaded.GetStatus();

        return CommitResourceLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

Base::Result<void> ViewState::ValidateDocumentRoot(
        const Base::Ref<Base::Object>& requestedRoot) noexcept {
        if (!requestedRoot) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root must not be null");
        }
        if (!metadata->Types().IsDerivedFrom(
                requestedRoot->RuntimeType(),
                Aero::Media::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root must derive from Visual");
        }
        Base::Result<Aero::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        return rootLayout
            ? Base::Result<void>()
            : Base::Result<void>(rootLayout.GetStatus());
    }

Base::Result<void> ViewState::MountRoot(
        Base::Ref<Base::Object> requestedRoot,
        Aero::Size availableSize) noexcept {
        if (!initialized) {
            return AeroNotInitialized(
                "View must be initialized before mounting");
        }
        if (mounted || root) {
            return ViewInvalidState(
                "View already has a mounted root");
        }
        const bool needsViewport =
            viewport.logicalSize.width != availableSize.width ||
            viewport.logicalSize.height != availableSize.height ||
            (availableSize.width > 0.0 && viewport.pixelWidth == 0U) ||
            (availableSize.height > 0.0 && viewport.pixelHeight == 0U);
        if (needsViewport) {
            Base::Result<ViewViewport> nextViewport =
                MakeLogicalViewport(availableSize, viewport.dpiScale);
            if (!nextViewport) return nextViewport.GetStatus();
            Base::Result<void> viewportApplied =
                ApplyViewport(nextViewport.Value());
            if (!viewportApplied) return viewportApplied.GetStatus();
        }
        Base::Result<void> validRoot = ValidateDocumentRoot(requestedRoot);
        if (!validRoot) return validRoot.GetStatus();
        if (loadedDocument.root &&
            loadedDocument.root.Get() != requestedRoot.Get()) {
            return ViewInvalidState(
                "Mounted root does not match the staged XAML document");
        }
        Base::Result<Aero::Media::Visual*> rootVisual =
            ResolveVisual(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        Base::Result<Aero::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootLayout) return rootLayout.GetStatus();
        Base::Result<void> rootTracked =
            loadedDocument.visualContent.AddNode(*rootVisual.Value());
        if (!rootTracked) return rootTracked.GetStatus();
        Base::Result<void> mountedResult = AttachVisualGraph(
            *rootVisual.Value(),
            *rootLayout.Value(),
            ResolveFrameworkElement(*requestedRoot, requestedRoot->RuntimeType()),
            {loadedDocument.visualContent.mountEdges.Data(),
             loadedDocument.visualContent.mountEdges.Size()},
            availableSize);
        if (!mountedResult) return mountedResult.GetStatus();
        root = std::move(requestedRoot);
        mounted = true;
        Markup::EffectRuntimeServices runtimeServices;
        runtimeServices.effectiveValues = values;
        runtimeServices.bindings = bindings;
        runtimeServices.fallbackResources = &dynamicResourceEnvironment;
        runtimeServices.lifetime = effectLifetime;
        Base::Result<void> bound = loadedDocument.effects.Bind(runtimeServices);
        if (!bound) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return bound.GetStatus();
        }
        Base::Result<void> effects = loadedDocument.effects.Commit();
        if (!effects) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return effects.GetStatus();
        }
        Base::Result<std::uint32_t> initialBindings =
            bindings->Flush();
        if (!initialBindings) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return initialBindings.GetStatus();
        }
        deferGeneratedActivation = true;
        Base::Result<void> uiApplied =
            ApplyViewUi(*this, *rootVisual.Value());
        if (!uiApplied) {
            deferGeneratedActivation = false;
            DetachViewUi(*this);
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return uiApplied.GetStatus();
        }
        Base::Result<void> interactions =
            CreateInteractions();
        if (!interactions) {
            deferGeneratedActivation = false;
            BeginDestroyInteractions();
            DetachViewUi(*this);
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return interactions.GetStatus();
        }
        Base::Result<void> completed =
            CompleteVisualEdges({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()});
        if (!completed) {
            deferGeneratedActivation = false;
            BeginDestroyInteractions();
            DetachViewUi(*this);
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return completed.GetStatus();
        }
        uiApplied =
            ApplyViewUi(
                *this,
                *rootVisual.Value());
        if (!uiApplied) {
            deferGeneratedActivation = false;
            BeginDestroyInteractions();
            DetachViewUi(*this);
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return uiApplied.GetStatus();
        }
        Base::Result<void> itemGeneratorsAttached =
            AttachPendingItemGenerators(*rootVisual.Value());
        if (!itemGeneratorsAttached) {
            deferGeneratedActivation = false;
            BeginDestroyInteractions();
            DetachViewUi(*this);
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return itemGeneratorsAttached.GetStatus();
        }
        Base::Result<std::uint32_t> settledBindings =
            bindings->Flush();
        deferGeneratedActivation = false;
        if (!settledBindings) {
            BeginDestroyInteractions();
            DetachViewUi(*this);
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return settledBindings.GetStatus();
        }
        Base::Result<void> generated = FlushGeneratedVisuals();
        if (!generated) {
            BeginDestroyInteractions();
            DetachViewUi(*this);
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return generated.GetStatus();
        }
        Base::Result<std::uint32_t> startedAnimations =
            storyboards->StartLoadedAnimations(rootVisual.Value());
        if (!startedAnimations) {
            if (animations != nullptr) {
                static_cast<void>(animations->RemoveAll());
            }
            storyboards->storyboardSessions.Clear();
            BeginDestroyInteractions();
            DetachViewUi(*this);
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return startedAnimations.GetStatus();
        }
        return {};
    }

Base::Result<void> ViewState::DetachFragment(
        FragmentMount& fragment) noexcept {
        if (!fragment.document.root) return {};
        Base::Result<Aero::Media::Visual*> rootVisual =
            ResolveVisual(
                *fragment.document.root,
                fragment.document.root->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        if (interactivity != nullptr) {
            interactivity->ClearAnimationSubscriptionsFor(*rootVisual.Value());
        }

        DetachViewUi(
            *this,
            rootVisual.Value(),
            {fragment.document.visualContent.nodes.Data(),
             fragment.document.visualContent.nodes.Size()});

        ElementTree& context = *tree;
        std::uint32_t remaining = 0U;
        for (const Aero::Markup::VisualEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (!edge.state.IsAttached()) continue;
                bool hasMountedChild = false;
                for (const Aero::Markup::VisualEdge& candidate :
                     fragment.document.visualContent.mountEdges) {
                    if (candidate.state.IsAttached() &&
                        candidate.parent == edge.child) {
                        hasMountedChild = true;
                        break;
                    }
                }
                if (hasMountedChild) continue;
                Base::Result<void> detached = context.DetachElement(edge.state);
                if (!detached) return detached.GetStatus();
                --remaining;
                progressed = true;
            }
            if (!progressed) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "content fragment edges cannot be detached leaf-first");
            }
        }
        if (fragment.rootEdge.IsAttached()) {
            Base::Result<void> detached = context.DetachElement(fragment.rootEdge);
            if (!detached) return detached.GetStatus();
        }
        if (fragment.host != nullptr) {
            fragment.host->SetContent(nullptr);
        }
        fragment.document.Clear();
        return {};
    }

Base::Result<void> ViewState::UnmountFragmentAt(
        std::uint32_t index) noexcept {
        if (index >= fragmentMounts.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "content fragment index is out of range");
        }
        Base::Result<void> detached = DetachFragment(fragmentMounts[index]);
        if (!detached) return detached.GetStatus();
        for (std::uint32_t next = index + 1U;
             next < fragmentMounts.Size(); ++next) {
            fragmentMounts[next - 1U] =
                std::move(fragmentMounts[next]);
        }
        fragmentMounts.PopBack();
        return {};
    }

Base::Result<void> ViewState::UnmountAllFragments() noexcept {
        while (!fragmentMounts.Empty()) {
            Base::Result<void> detached =
                UnmountFragmentAt(fragmentMounts.Size() - 1U);
            if (!detached) return detached.GetStatus();
        }
        return {};
    }

Base::Result<void> ViewState::DetachMountedRoot(
        bool clearDocument) noexcept {
        if (!initialized) return {};
        if (!mounted) {
            if (clearDocument && loadedDocument.root) {
                ClearLoadedDocument();
            }
            return {};
        }
        if (animations != nullptr) {
            Base::Result<void> removed =
                animations->RemoveAll();
            if (!removed) return removed.GetStatus();
        }
        storyboards->storyboardSessions.Clear();
        BeginDestroyInteractions();
        DetachViewUi(*this);
        FinishDestroyInteractions();
        Base::Result<void> fragments = UnmountAllFragments();
        if (!fragments) return fragments.GetStatus();
        Base::Result<void> unmounted =
            DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()});
        mounted = false;
        root.Reset();
        if (clearDocument) ClearLoadedDocument();
        return unmounted;
    }

Base::Result<void> ViewState::UnmountRoot() noexcept {
        return DetachMountedRoot(true);
    }

Base::Result<void> MountViewContent(
    ViewState& state,
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept {
    return state.MountRoot(
        std::move(root), availableSize);
}

Base::Result<void> MountViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    Base::Result<void> ready = state.BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot mount an empty UI document");
    }
    Base::Result<void> valid = state.ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();
    state.loadedDocument =
        Aero::Markup::TakeXamlDocument(document);
    return state.MountRoot(
        state.loadedDocument.root, availableSize);
}

Base::Result<void> ReplaceViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    ViewState* state_ = &state;
    if (state_ == nullptr || !state_->initialized || !state_->mounted) {
        return ViewApiInvalidState(
            "View document replacement requires a mounted view");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot replace a document with an empty document");
    }
    Base::Result<void> valid = state_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();

    Markup::LoaderResult next =
        Aero::Markup::TakeXamlDocument(document);
    if (!next.root ||
        !state_->metadata->Types().IsDerivedFrom(
            next.root->RuntimeType(),
            Aero::Media::Visual::StaticTypeId())) {
        next.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document root must derive from Visual");
    }

    Base::Result<void> detached =
        state_->DetachMountedRoot(false);
    if (!detached) {
        Base::Result<void> restored = state_->MountRoot(
            state_->loadedDocument.root, availableSize);
        next.Clear();
        return restored ? detached : restored;
    }

    Markup::LoaderResult previous =
        std::move(state_->loadedDocument);
    state_->loadedDocument = std::move(next);
    Base::Result<void> mounted = state_->MountRoot(
        state_->loadedDocument.root, availableSize);
    if (mounted) {
        previous.Clear();
        return {};
    }

    state_->loadedDocument = std::move(previous);
    Base::Result<void> restored = state_->MountRoot(
        state_->loadedDocument.root, availableSize);
    return restored ? mounted : restored;
}

Base::Result<void> MountViewFragment(
    ViewState& state,
    Controls::ContentControl& host,
    Markup::XamlDocument&& document) noexcept {
    ViewState* state_ = &state;
    if (state_ == nullptr || !state_->initialized || !state_->mounted ||
        state_->tree == nullptr || state_->layout == nullptr) {
        return ViewApiInvalidState(
            "content fragment mounting requires a mounted View");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment document must not be empty");
    }
    if (host.GetTree() != state_->tree) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment host does not belong to this View");
    }

    std::uint32_t existing = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < state_->fragmentMounts.Size(); ++index) {
        if (state_->fragmentMounts[index].host == &host) {
            existing = index;
            break;
        }
    }
    if (existing != UINT32_MAX) {
        Base::Result<void> unmounted = state_->UnmountFragmentAt(existing);
        if (!unmounted) return unmounted.GetStatus();
    } else if (AeroGuiInternal::ContentControlContent(host) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
              "content fragment host already owns non-fragment content");
    }
    Base::Result<void> capacity = state_->fragmentMounts.Reserve(
        state_->fragmentMounts.Size() + 1U);
    if (!capacity) return capacity.GetStatus();

    ::Aero::ViewState::FragmentMount fragment;
    fragment.host = &host;
    fragment.document = Aero::Markup::TakeXamlDocument(document);
    const Aero::NameScope* previousActiveNames =
        state_->activeFragmentNames;
    state_->activeFragmentNames = &fragment.document.names;
    const auto restoreActiveNames = [&]() noexcept {
        state_->activeFragmentNames = previousActiveNames;
    };
    Base::Result<Aero::Media::Visual*> rootVisual =
        state_->ResolveVisual(
            *fragment.document.root,
            fragment.document.root->RuntimeType());
    Base::Result<Aero::UIElement*> rootElement =
        state_->ResolveUIElement(
            *fragment.document.root,
            fragment.document.root->RuntimeType());
    if (!rootVisual || !rootElement) {
        restoreActiveNames();
        fragment.document.Clear();
        return !rootVisual
            ? Base::Result<void>(rootVisual.GetStatus())
            : Base::Result<void>(rootElement.GetStatus());
    }
    Base::Result<void> tracked =
        fragment.document.visualContent.AddNode(*rootVisual.Value());
    if (!tracked) {
        restoreActiveNames();
        fragment.document.Clear();
        return tracked.GetStatus();
    }
    Base::Result<void> assigned = AeroGuiInternal::SetOwnedContent(host,
        fragment.document.root, *rootElement.Value());
    if (!assigned) {
        restoreActiveNames();
        fragment.document.Clear();
        return assigned.GetStatus();
    }

    ElementTree& context = *state_->tree;
    Base::Result<Aero::ElementAttachment> rootMounted =
        context.AttachElement(host, *rootVisual.Value());
    if (!rootMounted) {
        restoreActiveNames();
        static_cast<void>(host.SetContent(nullptr));
        fragment.document.Clear();
        return rootMounted.GetStatus();
    }
    fragment.rootEdge = std::move(rootMounted).Value();

    const auto detachFailedFragment = [&]() noexcept {
        restoreActiveNames();
        static_cast<void>(state_->DetachFragment(fragment));
    };
    const auto attachEdges = [&](bool deferred) noexcept
        -> Base::Result<void> {
        std::uint32_t attached = 0U;
        for (const Aero::Markup::VisualEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < fragment.document.visualContent.mountEdges.Size()) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (edge.state.logicalAttached || edge.parent == nullptr ||
                    edge.child == nullptr ||
                    edge.parent->GetTree() != state_->tree ||
                    (deferred && edge.child->GetTree() == state_->tree)) {
                    continue;
                }
                Base::Result<Aero::ElementAttachment> mounted =
                    context.AttachElement(*edge.parent, *edge.child);
                if (!mounted) return mounted.GetStatus();
                edge.state = std::move(mounted).Value();
                ++attached;
                progressed = true;
            }
            if (!progressed) break;
        }
        return {};
    };

    Base::Result<void> attached = attachEdges(false);
    if (!attached) {
        detachFailedFragment();
        return attached.GetStatus();
    }
    Base::Result<void> applied =
        ApplyViewUi(*state_, *rootVisual.Value());
    if (!applied) {
        detachFailedFragment();
        return applied.GetStatus();
    }
    attached = attachEdges(true);
    if (!attached) {
        detachFailedFragment();
        return attached.GetStatus();
    }
    applied = ApplyViewUi(*state_, *rootVisual.Value());
    if (!applied) {
        detachFailedFragment();
        return applied.GetStatus();
    }
    Markup::EffectRuntimeServices runtimeServices;
    runtimeServices.effectiveValues = state_->values;
    runtimeServices.bindings = state_->bindings;
    runtimeServices.fallbackResources = &state_->dynamicResourceEnvironment;
    runtimeServices.lifetime = state_->effectLifetime;
    Base::Result<void> boundEffects = fragment.document.effects.Bind(runtimeServices);
    if (!boundEffects) {
        detachFailedFragment();
        return boundEffects.GetStatus();
    }
    Base::Result<void> effects = fragment.document.effects.Commit();
    if (!effects) {
        detachFailedFragment();
        return effects.GetStatus();
    }
    Base::Result<std::uint32_t> animations =
        state_->storyboards->StartLoadedAnimations(
            rootVisual.Value(), &fragment.document.names);
    if (!animations) {
        detachFailedFragment();
        return animations.GetStatus();
    }
    restoreActiveNames();
    Base::Result<void> retained =
        state_->fragmentMounts.PushBack(std::move(fragment));
    if (!retained) {
        static_cast<void>(state_->DetachFragment(fragment));
        return retained.GetStatus();
    }
    return {};
}

Base::Result<void> UnmountViewFragment(
    ViewState& state,
    Controls::ContentControl& host) noexcept {
    ViewState* state_ = &state;
    if (state_ == nullptr || !state_->initialized || !state_->mounted) {
        return ViewApiInvalidState(
            "content fragment unmounting requires a mounted View");
    }
    for (std::uint32_t index = 0U;
         index < state_->fragmentMounts.Size(); ++index) {
        if (state_->fragmentMounts[index].host == &host) {
            return state_->UnmountFragmentAt(index);
        }
    }
    return AeroGuiInternal::ContentControlContent(host) == nullptr
        ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidState,
              "content host does not contain a mounted XAML fragment"));
}

Base::Result<void> Markup::XamlReader::MountFragment(
    View& view,
    Controls::ContentControl& host,
    Markup::XamlDocument&& document) noexcept {
    if (gui_ == nullptr || &view.GetGui() != gui_ || view.state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML fragment View does not belong to this reader");
    }
    return MountViewFragment(
        *view.state_, host, std::move(document));
}

Base::Result<void> Markup::XamlReader::UnmountFragment(
    View& view,
    Controls::ContentControl& host) noexcept {
    if (gui_ == nullptr || &view.GetGui() != gui_ || view.state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML fragment View does not belong to this reader");
    }
    return UnmountViewFragment(*view.state_, host);
}


} // namespace Aero
