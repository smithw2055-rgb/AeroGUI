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

Base::Result<void> BeginDocumentLoad(ViewState& state) noexcept {
        if (!state.initialized) {
            return AeroNotInitialized(
                "View must be initialized before XAML loading");
        }
        if (state.mounted || state.root || state.loadedDocument.root) {
            return ViewInvalidState(
                "View already owns a loaded document");
        }
        return {};
    }

Base::Result<Markup::XamlReaderSettings> XamlSettings(ViewState& state, 
        bool deferredEffects,
        const Markup::XamlReaderSettings* override) noexcept {
        Markup::XamlReaderSettings result;
        if (override != nullptr) {
            result = *override;
        }
        state.loadContext.resources = &state.resources->dynamicResourceEnvironment;
        state.loadContext.effectiveValues = state.values;
        state.loadContext.bindings = state.bindings;
        state.loadContext.fallbackResources =
            &state.resources->dynamicResourceEnvironment;
        state.loadContext.documentCache = state.documentCache;
        state.loadContext.dispatcher = state.dispatcher;
        state.loadContext.dependencyProperties =
            &::Aero::MetadataPrivate::
                DependencyProperties(*state.metadata);
        state.loadContext.effectLifetime = state.effectLifetime;
        state.loadContext.effectCommitMode = deferredEffects
            ? Markup::EffectCommitMode::Deferred
            : Markup::EffectCommitMode::Immediate;
        return result;
    }

void ClearLoadedDocument(ViewState& state) noexcept {
        state.loadedDocument.Clear();
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
        if (object == nullptr) {
            for (ViewState::FragmentMount* component :
                 runtime->componentMounts) {
                if (component == nullptr) continue;
                object = component->document.names.Find(name);
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

Base::Result<void> ResourceHost::CommitLayer(
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
                RebuildDynamicEnvironment();
            if (rebuilt) return {};
            Base::Result<bool> removed =
                target.RemoveMerged(dictionary);
            Base::Result<void> restored =
                removed && removed.Value()
                ? RebuildDynamicEnvironment()
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
            RebuildDynamicEnvironment();
        if (rebuilt) return {};
        target = std::move(previous);
        Base::Result<void> restored =
            RebuildDynamicEnvironment();
        return restored
            ? Base::Result<void>(rebuilt.GetStatus())
            : restored;
    }

Base::Result<void> ResourceHost::LoadLayer(
        Base::StringView uri,
        Aero::ResourceDictionary& target,
        Diagnostics::IDiagnosticSink* diagnostics,
        bool merge) noexcept {
        if (!view->initialized) {
            return AeroNotInitialized(
                "View must be initialized before loading resources");
        }
        if (view->mounted || view->root || view->loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
        }
        Base::Result<Markup::XamlReaderSettings> loadOptions =
            XamlSettings(*view);
        if (!loadOptions) {
            return loadOptions.GetStatus();
        }
        if (view->xamlRuntime == nullptr) {
            return AeroNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            view->xamlRuntime->Load(
            view->xamlRuntime->Providers(),
            &view->loadContext,
            view->allocator,
            uri, loadOptions.Value(), diagnostics);
        if (!loaded) {
            return loaded.GetStatus();
        }
        return CommitLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

Base::Result<void> ResourceHost::LoadCompiledLayer(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        Aero::ResourceDictionary& target,
        bool merge) noexcept {
        if (!view->initialized) {
            return AeroNotInitialized(
                "View must be initialized before loading resources");
        }
        if (view->mounted || view->root || view->loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
        }
        Base::Result<Markup::XamlReaderSettings> loadOptions =
            XamlSettings(*view);
        if (!loadOptions) return loadOptions.GetStatus();
        if (view->xamlRuntime == nullptr) {
            return AeroNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            view->xamlRuntime->LoadCompiled(
                view->xamlRuntime->Providers(), &view->loadContext, view->allocator,
                bytes, originUri, loadOptions.Value());
        if (!loaded) return loaded.GetStatus();

        return CommitLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

Base::Result<void> ValidateDocumentRoot(ViewState& state, 
        const Base::Ref<Base::Object>& requestedRoot) noexcept {
        if (!requestedRoot) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root must not be null");
        }
        if (!state.metadata->Types().IsDerivedFrom(
                requestedRoot->RuntimeType(),
                Aero::Media::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root must derive from Visual");
        }
        Base::Result<Aero::UIElement*> rootLayout =
            state.ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        return rootLayout
            ? Base::Result<void>()
            : Base::Result<void>(rootLayout.GetStatus());
    }

Base::Result<void> MountRoot(ViewState& state, 
        Base::Ref<Base::Object> requestedRoot,
        Aero::Size availableSize) noexcept {
        if (!state.initialized) {
            return AeroNotInitialized(
                "View must be initialized before mounting");
        }
        if (state.mounted || state.root) {
            return ViewInvalidState(
                "View already has a mounted root");
        }
        const bool needsViewport =
            state.viewport.logicalSize.width != availableSize.width ||
            state.viewport.logicalSize.height != availableSize.height ||
            (availableSize.width > 0.0 && state.viewport.pixelWidth == 0U) ||
            (availableSize.height > 0.0 && state.viewport.pixelHeight == 0U);
        if (needsViewport) {
            Base::Result<ViewViewport> nextViewport =
                MakeLogicalViewport(availableSize, state.viewport.dpiScale);
            if (!nextViewport) return nextViewport.GetStatus();
            Base::Result<void> viewportApplied =
                state.ApplyViewport(nextViewport.Value());
            if (!viewportApplied) return viewportApplied.GetStatus();
        }
        Base::Result<void> validRoot = ValidateDocumentRoot(state, requestedRoot);
        if (!validRoot) return validRoot.GetStatus();
        if (state.loadedDocument.root &&
            state.loadedDocument.root.Get() != requestedRoot.Get()) {
            return ViewInvalidState(
                "Mounted root does not match the staged XAML document");
        }
        Base::Result<Aero::Media::Visual*> rootVisual =
            state.ResolveVisual(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        Base::Result<Aero::UIElement*> rootLayout =
            state.ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootLayout) return rootLayout.GetStatus();
        Base::Result<void> rootTracked =
            state.loadedDocument.visualContent.AddNode(*rootVisual.Value());
        if (!rootTracked) return rootTracked.GetStatus();
        Base::Result<void> mountedResult = state.AttachVisualGraph(
            *rootVisual.Value(),
            *rootLayout.Value(),
            state.ResolveFrameworkElement(*requestedRoot, requestedRoot->RuntimeType()),
            {state.loadedDocument.visualContent.mountEdges.Data(),
             state.loadedDocument.visualContent.mountEdges.Size()},
            availableSize);
        if (!mountedResult) return mountedResult.GetStatus();
        state.root = std::move(requestedRoot);
        state.mounted = true;
        Markup::EffectRuntimeServices runtimeServices;
        runtimeServices.effectiveValues = state.values;
        runtimeServices.bindings = state.bindings;
        runtimeServices.fallbackResources = &state.resources->dynamicResourceEnvironment;
        runtimeServices.lifetime = state.effectLifetime;
        Base::Result<void> bound = state.loadedDocument.effects.Bind(runtimeServices);
        if (!bound) {
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return bound.GetStatus();
        }
        Base::Result<void> effects = state.loadedDocument.effects.Commit();
        if (!effects) {
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return effects.GetStatus();
        }
        Base::Result<std::uint32_t> initialBindings =
            state.bindings->Flush();
        if (!initialBindings) {
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return initialBindings.GetStatus();
        }
        state.deferGeneratedActivation = true;
        Base::Result<void> uiApplied =
            ApplyViewUi(state, *rootVisual.Value());
        if (!uiApplied) {
            state.deferGeneratedActivation = false;
            DetachViewUi(state);
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return uiApplied.GetStatus();
        }
        Base::Result<void> interactions =
            state.CreateInteractions();
        if (!interactions) {
            state.deferGeneratedActivation = false;
            state.BeginDestroyInteractions();
            DetachViewUi(state);
            state.FinishDestroyInteractions();
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return interactions.GetStatus();
        }
        Base::Result<void> completed =
            state.CompleteVisualEdges({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()});
        if (!completed) {
            state.deferGeneratedActivation = false;
            state.BeginDestroyInteractions();
            DetachViewUi(state);
            state.FinishDestroyInteractions();
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return completed.GetStatus();
        }
        uiApplied =
            ApplyViewUi(
                state,
                *rootVisual.Value());
        if (!uiApplied) {
            state.deferGeneratedActivation = false;
            state.BeginDestroyInteractions();
            DetachViewUi(state);
            state.FinishDestroyInteractions();
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return uiApplied.GetStatus();
        }
        Base::Result<void> itemGeneratorsAttached =
            state.AttachPendingItemGenerators(*rootVisual.Value());
        if (!itemGeneratorsAttached) {
            state.deferGeneratedActivation = false;
            state.BeginDestroyInteractions();
            DetachViewUi(state);
            state.FinishDestroyInteractions();
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return itemGeneratorsAttached.GetStatus();
        }
        Base::Result<std::uint32_t> settledBindings =
            state.bindings->Flush();
        state.deferGeneratedActivation = false;
        if (!settledBindings) {
            state.BeginDestroyInteractions();
            DetachViewUi(state);
            state.FinishDestroyInteractions();
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return settledBindings.GetStatus();
        }
        Base::Result<void> generated = state.FlushGeneratedVisuals();
        if (!generated) {
            state.BeginDestroyInteractions();
            DetachViewUi(state);
            state.FinishDestroyInteractions();
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return generated.GetStatus();
        }
        Base::Result<std::uint32_t> startedAnimations =
            state.storyboards->StartLoadedAnimations(rootVisual.Value());
        if (!startedAnimations) {
            if (state.animations != nullptr) {
                static_cast<void>(state.animations->RemoveAll());
            }
            state.storyboards->storyboardSessions.Clear();
            state.BeginDestroyInteractions();
            DetachViewUi(state);
            state.FinishDestroyInteractions();
            static_cast<void>(state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()}));
            state.mounted = false;
            state.root.Reset();
            ClearLoadedDocument(state);
            return startedAnimations.GetStatus();
        }
        return {};
    }

Base::Result<void> DetachFragment(ViewState& state, 
        ViewState::FragmentMount& fragment) noexcept {
        if (!fragment.document.root) return {};
        Base::Result<Aero::Media::Visual*> rootVisual =
            state.ResolveVisual(
                *fragment.document.root,
                fragment.document.root->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        if (state.interactivity != nullptr) {
            state.interactivity->ClearAnimationSubscriptionsFor(*rootVisual.Value());
        }

        DetachViewUi(
            state,
            rootVisual.Value(),
            {fragment.document.visualContent.nodes.Data(),
             fragment.document.visualContent.nodes.Size()});

        ElementTree& context = *state.tree;
        const auto reconcileAttachment =
            [](Aero::Markup::VisualEdge& edge) noexcept {
                auto& edgeState = edge.state;
                if (edgeState.child == nullptr) {
                    edgeState.logicalAttached = false;
                    edgeState.visualAttached = false;
                    edgeState.layoutAttached = false;
                    edgeState.renderAttached = false;
                    return;
                }
                edgeState.logicalAttached =
                    edgeState.logicalParent != nullptr &&
                    edgeState.child->GetLogicalParent() == edgeState.logicalParent;
                edgeState.visualAttached =
                    edgeState.visualParent != nullptr &&
                    edgeState.child->GetVisualParent() == edgeState.visualParent;
                UIElement* childElement = ::Aero::TryCast<UIElement>(edgeState.child);
                UIElement* parentElement = edgeState.visualParent != nullptr
                    ? ::Aero::TryCast<UIElement>(edgeState.visualParent)
                    : nullptr;
                edgeState.layoutAttached =
                    childElement != nullptr && parentElement != nullptr &&
                    childElement->GetIsLayoutAttached() &&
                    childElement->LayoutParent() == parentElement;
                edgeState.renderAttached =
                    AeroGuiInternal::RenderAttached(*edgeState.child) &&
                    AeroGuiInternal::RenderParent(*edgeState.child) == edgeState.visualParent;
            };

        std::uint32_t remaining = 0U;
        for (Aero::Markup::VisualEdge& edge :
             fragment.document.visualContent.mountEdges) {
            reconcileAttachment(edge);
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                reconcileAttachment(edge);
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
                if (!detached && detached.GetStatus().code != Base::ErrorCode::NotFound) {
                    return detached.GetStatus();
                }
                edge.state = {};
                --remaining;
                progressed = true;
            }
            if (!progressed) {
                break;
            }
        }
        if (fragment.rootEdge.child != nullptr) {
            fragment.rootEdge.logicalAttached =
                fragment.rootEdge.logicalParent != nullptr &&
                fragment.rootEdge.child->GetLogicalParent() == fragment.rootEdge.logicalParent;
            fragment.rootEdge.visualAttached =
                fragment.rootEdge.visualParent != nullptr &&
                fragment.rootEdge.child->GetVisualParent() == fragment.rootEdge.visualParent;
        }
        if (fragment.rootEdge.IsAttached()) {
            Base::Result<void> detached = context.DetachElement(fragment.rootEdge);
            if (!detached && detached.GetStatus().code != Base::ErrorCode::NotFound) {
                return detached.GetStatus();
            }
            fragment.rootEdge = {};
        }
        if (fragment.host != nullptr) {
            fragment.host->SetContent(nullptr);
        }
        fragment.document.Clear();
        return {};
    }

Base::Result<void> UnmountFragmentAt(ViewState& state, 
        std::uint32_t index) noexcept {
        if (index >= state.fragmentMounts.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "content fragment index is out of range");
        }
        Base::Result<void> detached = DetachFragment(state, state.fragmentMounts[index]);
        if (!detached) return detached.GetStatus();
        for (std::uint32_t next = index + 1U;
             next < state.fragmentMounts.Size(); ++next) {
            state.fragmentMounts[next - 1U] =
                std::move(state.fragmentMounts[next]);
        }
        state.fragmentMounts.PopBack();
        return {};
    }

Base::Result<void> UnmountAllFragments(ViewState& state) noexcept {
        while (!state.fragmentMounts.Empty()) {
            Base::Result<void> detached =
                UnmountFragmentAt(state, state.fragmentMounts.Size() - 1U);
            if (!detached) return detached.GetStatus();
        }
        return {};
    }

Base::Result<void> DetachMountedRoot(ViewState& state, 
        bool clearDocument) noexcept {
        if (!state.initialized) return {};
        if (!state.mounted) {
            if (clearDocument && state.loadedDocument.root) {
                ClearLoadedDocument(state);
            }
            return {};
        }
        if (state.animations != nullptr) {
            Base::Result<void> removed =
                state.animations->RemoveAll();
            if (!removed) return removed.GetStatus();
        }
        state.storyboards->storyboardSessions.Clear();
        state.BeginDestroyInteractions();
        DetachViewUi(state);
        state.FinishDestroyInteractions();
        Base::Result<void> fragments = UnmountAllFragments(state);
        if (!fragments) return fragments.GetStatus();
        for (std::uint32_t index = state.componentMounts.Size();
             index > 0U; --index) {
            FreeObject(
                *state.allocator,
                Base::MemoryTag::Ui,
                state.componentMounts[index - 1U]);
        }
        state.componentMounts.Clear();
        Base::Result<void> unmounted =
            state.DetachVisualGraph({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()});
        state.mounted = false;
        state.root.Reset();
        if (clearDocument) ClearLoadedDocument(state);
        return unmounted;
    }

Base::Result<void> UnmountRoot(ViewState& state) noexcept {
        return DetachMountedRoot(state, true);
    }

Base::Result<void> MountViewContent(
    ViewState& state,
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept {
    return MountRoot(state, 
        std::move(root), availableSize);
}

Base::Result<void> MountViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    Base::Result<void> ready = BeginDocumentLoad(state);
    if (!ready) return ready.GetStatus();
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot mount an empty UI document");
    }
    Base::Result<void> valid = ValidateDocumentRoot(state, document.Root());
    if (!valid) return valid.GetStatus();
    state.loadedDocument =
        Aero::Markup::TakeXamlDocument(document);
    return MountRoot(state, 
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
    Base::Result<void> valid = ValidateDocumentRoot(*state_, document.Root());
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
        DetachMountedRoot(*state_, false);
    if (!detached) {
        Base::Result<void> restored = MountRoot(*state_, 
            state_->loadedDocument.root, availableSize);
        next.Clear();
        return restored ? detached : restored;
    }

    Markup::LoaderResult previous =
        std::move(state_->loadedDocument);
    state_->loadedDocument = std::move(next);
    Base::Result<void> mounted = MountRoot(*state_, 
        state_->loadedDocument.root, availableSize);
    if (mounted) {
        previous.Clear();
        return {};
    }

    state_->loadedDocument = std::move(previous);
    Base::Result<void> restored = MountRoot(*state_, 
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
        Base::Result<void> unmounted = UnmountFragmentAt(*state_, existing);
        if (!unmounted) {
            // Best-effort unmount: a stale visual/logical parent pointer left by
            // a prior template substitution can make DetachVisual report NotFound
            // here. Tolerate it so the incoming fragment still mounts and renders
            // instead of leaving the host stuck on the previous sample.
            static_cast<void>(unmounted.GetStatus());
        }
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
    // Templated ContentControl hosts Content through a ContentPresenter.
    // SetOwnedContent / Content DP already joined the fragment root there.
    // A second AttachElement(host, root) then fails with "already attached".
    if (rootElement.Value()->GetIsLayoutAttached() ||
        AeroGuiInternal::TemplateRoot(host) != nullptr) {
        fragment.rootEdge.logicalParent = &host;
        fragment.rootEdge.visualParent =
            rootVisual.Value()->GetVisualParent() != nullptr
                ? rootVisual.Value()->GetVisualParent()
                : static_cast<Aero::Media::Visual*>(&host);
        fragment.rootEdge.child = rootVisual.Value();
    } else {
        Base::Result<Aero::ElementAttachment> rootMounted =
            context.AttachElement(host, *rootVisual.Value());
        if (!rootMounted) {
            restoreActiveNames();
            static_cast<void>(host.SetContent(nullptr));
            fragment.document.Clear();
            return rootMounted.GetStatus();
        }
        fragment.rootEdge = std::move(rootMounted).Value();
    }

    const auto detachFailedFragment = [&]() noexcept {
        restoreActiveNames();
        static_cast<void>(DetachFragment(*state_, fragment));
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
                    edge.child == nullptr) {
                    continue;
                }
                if (Aero::UIElement* childEl =
                        ::Aero::TryCast<Aero::UIElement>(edge.child);
                    childEl != nullptr && childEl->GetIsLayoutAttached()) {
                    ++attached;
                    progressed = true;
                    continue;
                }
                if (edge.parent->GetTree() != state_->tree ||
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
    runtimeServices.fallbackResources = &state_->resources->dynamicResourceEnvironment;
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
        static_cast<void>(DetachFragment(*state_, fragment));
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
            return UnmountFragmentAt(*state_, index);
        }
    }
    return AeroGuiInternal::ContentControlContent(host) == nullptr
        ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidState,
              "content host does not contain a mounted XAML fragment"));
}

Base::Result<void> AdoptLoadedComponent(
    ViewState& state,
    Markup::LoaderResult&& incoming) noexcept {
    if (!state.initialized || !state.mounted ||
        state.tree == nullptr || state.bindings == nullptr ||
        state.values == nullptr || state.allocator == nullptr) {
        return {};
    }
    if (!incoming.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "LoadComponent document has no root object");
    }

    ViewState::FragmentMount* mount = nullptr;
    Base::Result<void> allocated = AllocateObject(
        *state.allocator, Base::MemoryTag::Ui, mount);
    if (!allocated) return allocated.GetStatus();
    mount->document = std::move(incoming);
    Base::Result<void> retained = state.componentMounts.PushBack(mount);
    if (!retained) {
        FreeObject(*state.allocator, Base::MemoryTag::Ui, mount);
        return retained.GetStatus();
    }
    Markup::LoaderResult& document = mount->document;

    ElementTree& context = *state.tree;
    const auto attachEdges = [&]() noexcept -> Base::Result<void> {
        std::uint32_t attached = 0U;
        for (const Markup::VisualEdge& edge :
             document.visualContent.mountEdges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < document.visualContent.mountEdges.Size()) {
            bool progressed = false;
            for (Markup::VisualEdge& edge :
                 document.visualContent.mountEdges) {
                if (edge.state.logicalAttached ||
                    edge.parent == nullptr ||
                    edge.child == nullptr) {
                    continue;
                }
                if (UIElement* childEl =
                        ::Aero::TryCast<UIElement>(edge.child);
                    childEl != nullptr &&
                    childEl->GetIsLayoutAttached()) {
                    ++attached;
                    progressed = true;
                    continue;
                }
                if (edge.parent->GetTree() != &context) {
                    continue;
                }
                if (edge.child->GetTree() == &context) {
                    ++attached;
                    progressed = true;
                    continue;
                }
                Base::Result<ElementAttachment> mounted =
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

    if (Controls::ContentControl* host =
            ::Aero::TryCast<Controls::ContentControl>(
                document.root.Get())) {
        if (UIElement* content =
                AeroGuiInternal::ContentControlContent(*host);
            content != nullptr &&
            content->GetVisualParent() == nullptr &&
            host->GetTree() == &context) {
            if (content->GetTree() == nullptr &&
                content->GetLogicalParent() == nullptr) {
                Base::Result<ElementAttachment> mounted =
                    context.AttachElement(*host, *content);
                if (!mounted) return mounted.GetStatus();
            } else if (content->GetVisualParent() != host) {
                Base::Result<VisualAttachment> mounted =
                    context.AttachVisualChild(*host, *content);
                if (!mounted) return mounted.GetStatus();
            }
        }
    }

    Base::Result<void> attached = attachEdges();
    if (!attached) return attached.GetStatus();

    Base::Result<Aero::Media::Visual*> rootVisual =
        state.ResolveVisual(
            *document.root, document.root->RuntimeType());
    if (rootVisual) {
        Base::Result<void> applied =
            ApplyViewUi(state, *rootVisual.Value());
        if (!applied) return applied.GetStatus();
        attached = attachEdges();
        if (!attached) return attached.GetStatus();
        applied = ApplyViewUi(state, *rootVisual.Value());
        if (!applied) return applied.GetStatus();
        // LoadComponent can populate a UserControl after the View has already
        // created its interaction services. Attach newly materialized
        // controls and template-generated visuals so ButtonBase receives
        // pointer/key behavior and text/path services are initialized.
        Base::Result<void> interactions =
            state.VisitAndAttach(*rootVisual.Value());
        if (!interactions) return interactions.GetStatus();
    }

    Markup::EffectRuntimeServices runtimeServices;
    runtimeServices.effectiveValues = state.values;
    runtimeServices.bindings = state.bindings;
    runtimeServices.fallbackResources =
        state.resources != nullptr
            ? &state.resources->dynamicResourceEnvironment
            : nullptr;
    runtimeServices.lifetime = state.effectLifetime;
    Base::Result<void> bound = document.effects.Bind(runtimeServices);
    if (!bound) return bound.GetStatus();
    Base::Result<void> prepared = document.effects.Prepare(document.names);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> committed = document.effects.Commit();
    if (!committed) return committed.GetStatus();
    Base::Result<std::uint32_t> flushed = state.bindings->Flush();
    if (!flushed) return flushed.GetStatus();

    if (rootVisual && state.storyboards != nullptr) {
        Base::Result<std::uint32_t> animations =
            state.storyboards->StartLoadedAnimations(
                rootVisual.Value(), &document.names);
        if (!animations) return animations.GetStatus();
    }
    if (DependencyObject* target =
            ::Aero::TryCast<DependencyObject>(document.root.Get())) {
        static_cast<void>(
            state.bindings->ActivateDeferredWhenReady(*target));
    }
    return {};
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
