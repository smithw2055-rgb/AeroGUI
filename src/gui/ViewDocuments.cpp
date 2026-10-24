#include "gui/ViewFrame.hpp"
#include "gui/GuiRuntime.hpp"
#include <Aero/VisualTreeHelper.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

// ============================================================================
// Section 1: Document load (Begin/XamlSettings/Clear/FindName)
// Section 2: Resource layers (ResourceHost::Commit/Load*) starts at CommitLayer
// Section 3: Mount/Fragment lifecycle (MountRoot ... UnmountFragment) starts at MountRoot
// ============================================================================

Base::Result<void> BeginDocumentLoad(ViewFrame& state) noexcept {
        if (!state.initialized) {
            return ViewNotInitialized(
                "View must be initialized before XAML loading");
        }
        if (state.mounted || state.root || state.loadedDocument.root) {
            return ViewInvalidState(
                "View already owns a loaded document");
        }
        return {};
    }

Base::Result<Markup::XamlReaderSettings> XamlSettings(ViewFrame& state, 
        bool deferredEffects,
        const Markup::XamlReaderSettings* override) noexcept {
        Markup::XamlReaderSettings result;
        if (override != nullptr) {
            result = *override;
        }
        state.loadContext.resources = state.Resources() != nullptr
            ? &state.Resources()->dynamicResourceEnvironment
            : nullptr;
        state.loadContext.effectiveValues = state.Values();
        state.loadContext.bindings = state.Bindings();
        state.loadContext.fallbackResources = state.Resources() != nullptr
            ? &state.Resources()->dynamicResourceEnvironment
            : nullptr;
        state.loadContext.documentCache = state.documentCache;
        state.loadContext.dispatcher = state.dispatcher;
        state.loadContext.dependencyProperties =
            &(*state.metadata).DependencyProperties();
        state.loadContext.effectLifetime = state.effectLifetime;
        state.loadContext.effectCommitMode = deferredEffects
            ? Markup::EffectCommitMode::Deferred
            : Markup::EffectCommitMode::Immediate;
        return result;
    }

void ClearLoadedDocument(ViewFrame& state) noexcept {
        state.loadedDocument.Clear();
    }

Base::Object* ViewFrame::FindNameForElement(
        void* context,
        Base::StringView name,
        Meta::TypeId expectedType,
        const DependencyObject* requester) noexcept {
        auto* runtime = static_cast<ViewFrame*>(context);
        if (runtime == nullptr || name.Empty()) return nullptr;
        Base::Object* object = runtime->activeFragmentNames != nullptr
            ? runtime->activeFragmentNames->Find(name)
            : nullptr;

        // If requester is within a loaded component mount or fragment, resolve inside that component's scope first
        if (object == nullptr && requester != nullptr &&
            (!runtime->componentMounts.Empty() || !runtime->fragmentMounts.Empty())) {
            const DependencyObject* current = requester;
            for (std::uint32_t depth = 0U; current != nullptr && depth < 256U; ++depth) {
                for (ViewFrame::FragmentMount* component : runtime->componentMounts) {
                    if (component != nullptr && component->document.root.Get() == current) {
                        object = component->document.names.Find(name);
                        break;
                    }
                }
                if (object != nullptr) break;
                for (ViewFrame::FragmentMount* fragment : runtime->fragmentMounts) {
                    if (fragment != nullptr && fragment->document.root.Get() == current) {
                        object = fragment->document.names.Find(name);
                        break;
                    }
                }
                if (object != nullptr) break;
                const auto* visual = ::Aero::TryCast<const ::Aero::Media::Visual>(current);
                if (visual == nullptr) break;
                const ::Aero::Base::Object* parent = visual->GetLogicalParent();
                if (parent == nullptr) {
                    parent = visual->GetVisualParent();
                }
                current = ::Aero::TryCast<const DependencyObject>(parent);
            }
        }

        if (object == nullptr) {
            object = runtime->loadedDocument.names.Find(name);
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
            return ViewNotInitialized(
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
        if (view->gui == nullptr || view->schemaBundle == nullptr) {
            return ViewNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            view->gui->Load(
            &view->loadContext,
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
            return ViewNotInitialized(
                "View must be initialized before loading resources");
        }
        if (view->mounted || view->root || view->loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
        }
        Base::Result<Markup::XamlReaderSettings> loadOptions =
            XamlSettings(*view);
        if (!loadOptions) return loadOptions.GetStatus();
        if (view->gui == nullptr || view->schemaBundle == nullptr) {
            return ViewNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            view->gui->LoadCompiled(
                &view->loadContext,
                bytes, originUri, loadOptions.Value());
        if (!loaded) return loaded.GetStatus();

        return CommitLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

Base::Result<void> ValidateDocumentRoot(ViewFrame& state, 
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

namespace {

Base::Result<void> MountOwnedLayoutChildren(
    ElementTree& context,
    Aero::Media::Visual& node) noexcept {
    if (auto* panel = ::Aero::TryCast<Controls::Panel>(&node)) {
        const std::uint32_t count = panel->GetChildren().GetCount();
        for (std::uint32_t index = 0U; index < count; ++index) {
            UIElement* child = panel->GetChildren().GetItem(index);
            if (child == nullptr) continue;
            if (child->GetVisualParent() == panel &&
                child->GetIsLayoutAttached()) {
                continue;
            }
            if (child->GetVisualParent() != nullptr &&
                child->GetVisualParent() != panel) {
                continue;
            }
            Base::Result<VisualAttachment> mounted =
                context.AttachVisualChild(*panel, *child);
            if (!mounted) return mounted.GetStatus();
        }
    }
    if (auto* host = ::Aero::TryCast<Controls::ContentControl>(&node)) {
        if ((*host).GetTemplateRoot() == nullptr) {
            UIElement* content =
                (*host).GetContentElement();
            if (content != nullptr &&
                (content->GetVisualParent() != host ||
                 !content->GetIsLayoutAttached()) &&
                (content->GetVisualParent() == nullptr ||
                 content->GetVisualParent() == host)) {
                Base::Result<VisualAttachment> mounted =
                    context.AttachVisualChild(*host, *content);
                if (!mounted) return mounted.GetStatus();
            }
        }
    }
    if (auto* decorator = ::Aero::TryCast<Controls::Decorator>(&node)) {
        UIElement* child = (*decorator).GetChild();
        if (child != nullptr &&
            (child->GetVisualParent() != decorator ||
             !child->GetIsLayoutAttached()) &&
            (child->GetVisualParent() == nullptr ||
             child->GetVisualParent() == decorator)) {
            Base::Result<VisualAttachment> mounted =
                context.AttachVisualChild(*decorator, *child);
            if (!mounted) return mounted.GetStatus();
        }
    }

    Base::Result<void> result;
    ForEachVisualAndLogicalChild(node, [&](Aero::Media::Visual* child) {
        if (!result) return;
        UIElement* childEl = ::Aero::TryCast<UIElement>(child);
        UIElement* parentEl = ::Aero::TryCast<UIElement>(&node);
        if (childEl != nullptr && parentEl != nullptr &&
            child->GetVisualParent() == &node &&
            (!childEl->GetIsLayoutAttached() ||
             childEl->LayoutParent() != parentEl)) {
            Base::Result<VisualAttachment> mounted = context.AttachVisualChild(node, *child);
            if (!mounted) {
                result = mounted.GetStatus();
                return;
            }
        }
        result = MountOwnedLayoutChildren(context, *child);
    });
    if (!result) return result.GetStatus();
    return {};
}

void InvalidateLayoutSubtree(UIElement& element) noexcept {
    element.InvalidateMeasure();
    element.InvalidateArrange();
    ForEachVisualAndLogicalChild(element, [](Aero::Media::Visual* child) {
        if (auto* childElement = ::Aero::TryCast<UIElement>(child)) {
            InvalidateLayoutSubtree(*childElement);
        }
    });
}

Base::Result<void> ActivateMountedSubtree(
    ViewFrame& state,
    Aero::Media::Visual& subtreeRoot,
    Markup::LoaderResult& doc) noexcept {
    if (state.Tree() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "View has no element tree");
    }
    ElementTree& context = *state.Tree();

    const auto hostLoadedContent = [&]() noexcept -> Base::Result<void> {
        Controls::ContentControl* host =
            ::Aero::TryCast<Controls::ContentControl>(doc.root.Get());
        if (host == nullptr || VisualTree(host) != &context) {
            return {};
        }
        UIElement* content = (*host).GetContentElement();
        if (content == nullptr) {
            for (Markup::VisualEdge& edge : doc.visualContent.mountEdges) {
                if (edge.parent != host || edge.child == nullptr) {
                    continue;
                }
                UIElement* child = ::Aero::TryCast<UIElement>(edge.child);
                if (child == nullptr || child == host) {
                    continue;
                }
                (void)(*host).SetContentValue(
                    Base::Ref<Base::Object>::FromBorrowed(*child));
                content = (*host).GetContentElement();
                break;
            }
        }
        if (content == nullptr ||
            content->GetIsLayoutAttached() ||
            (content->GetVisualParent() == host &&
             content->GetIsLayoutAttached())) {
            return {};
        }
        ::Aero::Media::Visual* visualParent = host;
        if (content->GetVisualParent() != nullptr) {
            visualParent = content->GetVisualParent();
        }
        if (VisualTree(content) == nullptr &&
            content->GetLogicalParent() == nullptr) {
            Base::Result<ElementAttachment> mounted =
                context.AttachElement(*host, *visualParent, *content);
            if (!mounted) return mounted.GetStatus();
        } else {
            Base::Result<VisualAttachment> mounted =
                context.AttachVisualChild(*visualParent, *content);
            if (!mounted) return mounted.GetStatus();
        }
        return {};
    };

    const auto attachEdges = [&](bool deferred) noexcept -> Base::Result<void> {
        std::uint32_t attached = 0U;
        for (const Markup::VisualEdge& edge : doc.visualContent.mountEdges) {
            if (edge.state.layoutAttached || edge.state.logicalAttached) {
                ++attached;
            }
        }
        while (attached < doc.visualContent.mountEdges.Size()) {
            bool progressed = false;
            for (Markup::VisualEdge& edge : doc.visualContent.mountEdges) {
                if (edge.parent == nullptr || edge.child == nullptr) {
                    continue;
                }
                if (edge.state.layoutAttached && edge.state.logicalAttached) {
                    continue;
                }
                if (UIElement* childEl = ::Aero::TryCast<UIElement>(edge.child);
                    childEl != nullptr &&
                    childEl->GetIsLayoutAttached() &&
                    (childEl->GetVisualParent() == edge.parent ||
                     childEl->GetLogicalParent() == edge.parent)) {
                    edge.state.layoutAttached = true;
                    if (childEl->GetLogicalParent() == edge.parent) {
                        edge.state.logicalAttached = true;
                    }
                    ++attached;
                    progressed = true;
                    continue;
                }
                if (VisualTree(edge.parent) != &context) {
                    continue;
                }
                if (deferred && VisualTree(edge.child) == &context &&
                    (edge.state.layoutAttached || edge.state.logicalAttached)) {
                    continue;
                }
                if (UIElement* childEl = ::Aero::TryCast<UIElement>(edge.child);
                    childEl != nullptr && VisualTree(childEl) == &context) {
                    ::Aero::Media::Visual* visualParent = edge.parent;
                    if (childEl->GetVisualParent() != nullptr &&
                        VisualTree(childEl->GetVisualParent()) == &context) {
                        visualParent = childEl->GetVisualParent();
                    }
                    if (childEl->GetLogicalParent() == nullptr &&
                        edge.parent != nullptr) {
                        (void)context.AttachLogical(*edge.parent, *childEl);
                    }
                    if (!childEl->GetIsLayoutAttached() ||
                        childEl->GetVisualParent() != visualParent) {
                        Base::Result<VisualAttachment> mounted =
                            context.AttachVisualChild(*visualParent, *childEl);
                        if (!mounted) return mounted.GetStatus();
                        edge.state.visualAttached = true;
                        edge.state.layoutAttached = true;
                    } else {
                        edge.state.visualAttached = true;
                        edge.state.layoutAttached = true;
                    }
                    if (childEl->GetLogicalParent() == edge.parent) {
                        edge.state.logicalAttached = true;
                    }
                    ++attached;
                    progressed = true;
                    continue;
                }
                if (VisualTree(edge.child) == &context) {
                    continue;
                }
                ::Aero::Media::Visual* visualParent = edge.parent;
                if (edge.child->GetVisualParent() != nullptr &&
                    VisualTree(edge.child->GetVisualParent()) == &context) {
                    visualParent = edge.child->GetVisualParent();
                }
                Base::Result<ElementAttachment> mounted =
                    context.AttachElement(*edge.parent, *visualParent, *edge.child);
                if (!mounted) return mounted.GetStatus();
                edge.state = std::move(mounted).Value();
                ++attached;
                progressed = true;
            }
            if (!progressed) break;
        }
        return {};
    };

    struct DeferGuard {
        bool& flag;
        bool active = true;
        ~DeferGuard() {
            if (active) flag = false;
        }
    } deferGuard{state.deferGeneratedActivation};
    state.deferGeneratedActivation = true;

    // Round 1
    Base::Result<void> hosted = hostLoadedContent();
    if (!hosted) return hosted.GetStatus();
    Base::Result<void> attached = attachEdges(false);
    if (!attached) return attached.GetStatus();
    hosted = hostLoadedContent();
    if (!hosted) return hosted.GetStatus();
    attached = MountOwnedLayoutChildren(context, subtreeRoot);
    if (!attached) return attached.GetStatus();

    // Round 2 (deferred)
    attached = attachEdges(true);
    if (!attached) return attached.GetStatus();
    hosted = hostLoadedContent();
    if (!hosted) return hosted.GetStatus();
    attached = MountOwnedLayoutChildren(context, subtreeRoot);
    if (!attached) return attached.GetStatus();

    Markup::EffectServices runtimeServices;
    runtimeServices.effectiveValues = state.Values();
    runtimeServices.bindings = state.Bindings();
    runtimeServices.fallbackResources =
        state.Resources() != nullptr
            ? &state.Resources()->dynamicResourceEnvironment
            : nullptr;
    runtimeServices.lifetime = state.effectLifetime;
    Base::Result<void> bound = doc.effects.Bind(runtimeServices);
    if (!bound) return bound.GetStatus();
    Base::Result<void> prepared = doc.effects.Prepare(doc.names);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> committed = doc.effects.Commit();
    if (!committed) return committed.GetStatus();

    Base::Result<void> applied = ApplyViewUi(state, subtreeRoot);
    if (!applied) return applied.GetStatus();

    Base::Result<void> interactions = state.VisitAndAttach(subtreeRoot);
    if (!interactions) return interactions.GetStatus();

    if (UIElement* rootElement = ::Aero::TryCast<UIElement>(&subtreeRoot)) {
        InvalidateLayoutSubtree(*rootElement);
    }

    state.deferGeneratedActivation = false;
    deferGuard.active = false;
    if (state.Bindings() != nullptr) {
        Base::Result<std::uint32_t> flushed = state.Bindings()->Flush();
        if (!flushed) return flushed.GetStatus();
    }

    Base::Result<void> generated = state.FlushGeneratedVisuals();
    if (!generated) return generated.GetStatus();

    if (state.Storyboards() != nullptr) {
        Base::Result<std::uint32_t> animations =
            state.Storyboards()->StartLoadedAnimations(&subtreeRoot, &doc.names);
        if (!animations) return animations.GetStatus();
    }

    if (DependencyObject* target =
            ::Aero::TryCast<DependencyObject>(doc.root.Get())) {
        if (state.Bindings() != nullptr) {
            static_cast<void>(
                state.Bindings()->ActivateDeferredWhenReady(*target));
        }
    }
    return {};
}

} // namespace

Base::Result<void> MountRoot(ViewFrame& state, 
        Base::Ref<Base::Object> requestedRoot,
        Aero::Size availableSize) noexcept {
        if (!state.initialized) {
            return ViewNotInitialized(
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
        const auto rollbackRoot = [&]() noexcept {
            if (state.Animations() != nullptr) {
                static_cast<void>(state.Animations()->RemoveAll());
            }
            if (state.Storyboards() != nullptr) {
                state.Storyboards()->storyboardSessions.Clear();
            }
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
        };

        Base::Result<void> interactions = state.CreateInteractions();
        if (!interactions) {
            rollbackRoot();
            return interactions.GetStatus();
        }
        Base::Result<void> activated = ActivateMountedSubtree(
            state, *rootVisual.Value(), state.loadedDocument);
        if (!activated) {
            rollbackRoot();
            return activated.GetStatus();
        }
        state.mounted = true;
        return {};
    }

Base::Result<void> DetachFragment(ViewFrame& state, 
        ViewFrame::FragmentMount& fragment) noexcept {
        if (!fragment.document.root) return {};
        Base::Result<Aero::Media::Visual*> rootVisual =
            state.ResolveVisual(
                *fragment.document.root,
                fragment.document.root->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        if (state.Interactivity() != nullptr) {
            state.Interactivity()->ClearAnimationSubscriptionsFor(*rootVisual.Value());
        }

        DetachViewUi(
            state,
            rootVisual.Value(),
            {fragment.document.visualContent.nodes.Data(),
             fragment.document.visualContent.nodes.Size()});

        ElementTree& context = *state.Tree();
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
                    edgeState.child->IsRenderAttached() &&
                    edgeState.child->GetVisualParent() == edgeState.visualParent;
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

Base::Result<void> UnmountFragmentAt(ViewFrame& state, 
        std::uint32_t index) noexcept {
        if (index >= state.fragmentMounts.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "content fragment index is out of range");
        }
        ViewFrame::FragmentMount* mount = state.fragmentMounts[index];
        Base::Result<void> detached{};
        if (mount != nullptr) {
            detached = DetachFragment(state, *mount);
            FreeObject(*state.allocator, Base::MemoryTag::Ui, mount);
        }
        for (std::uint32_t next = index + 1U;
             next < state.fragmentMounts.Size(); ++next) {
            state.fragmentMounts[next - 1U] = state.fragmentMounts[next];
        }
        state.fragmentMounts.PopBack();
        return detached;
    }

Base::Result<void> UnmountAllFragments(ViewFrame& state) noexcept {
        while (!state.fragmentMounts.Empty()) {
            Base::Result<void> detached =
                UnmountFragmentAt(state, state.fragmentMounts.Size() - 1U);
            if (!detached) return detached.GetStatus();
        }
        return {};
    }

Base::Result<void> DetachMountedRoot(ViewFrame& state, 
        bool clearDocument) noexcept {
        if (!state.initialized) return {};
        if (!state.mounted) {
            if (clearDocument && state.loadedDocument.root) {
                ClearLoadedDocument(state);
            }
            return {};
        }
        if (state.Animations() != nullptr) {
            Base::Result<void> removed =
                state.Animations()->RemoveAll();
            if (!removed) return removed.GetStatus();
        }
        if (state.Storyboards() != nullptr) {
            state.Storyboards()->storyboardSessions.Clear();
        }
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

Base::Result<void> UnmountRoot(ViewFrame& state) noexcept {
        return DetachMountedRoot(state, true);
    }

Base::Result<void> MountViewContent(
    ViewFrame& state,
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept {
    return MountRoot(state, 
        std::move(root), availableSize);
}

Base::Result<void> MountViewDocument(
    ViewFrame& state,
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
    ViewFrame& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    if (!state.initialized || !state.mounted) {
        return ViewInvalidState(
            "View document replacement requires a mounted view");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot replace a document with an empty document");
    }
    Base::Result<void> valid = ValidateDocumentRoot(state, document.Root());
    if (!valid) return valid.GetStatus();

    Markup::LoaderResult next =
        Aero::Markup::TakeXamlDocument(document);
    if (!next.root ||
        !state.metadata->Types().IsDerivedFrom(
            next.root->RuntimeType(),
            Aero::Media::Visual::StaticTypeId())) {
        next.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document root must derive from Visual");
    }

    Base::Result<void> detached =
        DetachMountedRoot(state, false);
    if (!detached) {
        Base::Result<void> restored = MountRoot(state, 
            state.loadedDocument.root, availableSize);
        next.Clear();
        return restored ? detached : restored;
    }

    Markup::LoaderResult previous =
        std::move(state.loadedDocument);
    state.loadedDocument = std::move(next);
    Base::Result<void> mounted = MountRoot(state, 
        state.loadedDocument.root, availableSize);
    if (mounted) {
        previous.Clear();
        return {};
    }

    state.loadedDocument = std::move(previous);
    Base::Result<void> restored = MountRoot(state, 
        state.loadedDocument.root, availableSize);
    return restored ? mounted : restored;
}

Base::Result<void> MountViewFragment(
    ViewFrame& state,
    Controls::ContentControl& host,
    Markup::XamlDocument&& document) noexcept {
    if (!state.initialized || !state.mounted ||
        state.Tree() == nullptr || state.Layout() == nullptr) {
        return ViewInvalidState(
            "content fragment mounting requires a mounted View");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment document must not be empty");
    }
    if (VisualTree(host) != state.Tree()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment host does not belong to this View");
    }

    std::uint32_t existing = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < state.fragmentMounts.Size(); ++index) {
        if (state.fragmentMounts[index] != nullptr &&
            state.fragmentMounts[index]->host == &host) {
            existing = index;
            break;
        }
    }
    if (existing != UINT32_MAX) {
        Base::Result<void> unmounted = UnmountFragmentAt(state, existing);
        if (!unmounted) {
            // Best-effort unmount: a stale visual/logical parent pointer left by
            // a prior template substitution can make DetachVisual report NotFound
            // here. Tolerate it so the incoming fragment still mounts and renders
            // instead of leaving the host stuck on the previous sample.
            static_cast<void>(unmounted.GetStatus());
        }
    } else if ((host).GetContentElement() != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
              "content fragment host already owns non-fragment content");
    }
    state.fragmentMounts.Reserve(
        state.fragmentMounts.Size() + 1U);

    ViewFrame::FragmentMount* fragment = nullptr;
    Base::Result<void> allocated = AllocateObject(
        *state.allocator, Base::MemoryTag::Ui, fragment);
    if (!allocated) return allocated.GetStatus();

    fragment->host = &host;
    fragment->document = Aero::Markup::TakeXamlDocument(document);
    const Aero::NameScope* previousActiveNames =
        state.activeFragmentNames;
    state.activeFragmentNames = &fragment->document.names;
    const auto restoreActiveNames = [&]() noexcept {
        state.activeFragmentNames = previousActiveNames;
    };
    Base::Result<Aero::Media::Visual*> rootVisual =
        state.ResolveVisual(
            *fragment->document.root,
            fragment->document.root->RuntimeType());
    Base::Result<Aero::UIElement*> rootElement =
        state.ResolveUIElement(
            *fragment->document.root,
            fragment->document.root->RuntimeType());
    if (!rootVisual || !rootElement) {
        restoreActiveNames();
        fragment->document.Clear();
        Base::Status status = !rootVisual
            ? rootVisual.GetStatus()
            : rootElement.GetStatus();
        FreeObject(*state.allocator, Base::MemoryTag::Ui, fragment);
        return status;
    }
    Base::Result<void> tracked =
        fragment->document.visualContent.AddNode(*rootVisual.Value());
    if (!tracked) {
        restoreActiveNames();
        fragment->document.Clear();
        FreeObject(*state.allocator, Base::MemoryTag::Ui, fragment);
        return tracked.GetStatus();
    }
    (host).SetOwnedContent(
        fragment->document.root, *rootElement.Value());

    ElementTree& context = *state.Tree();
    // Templated ContentControl hosts Content through a ContentPresenter.
    // SetOwnedContent / Content DP already joined the fragment root there.
    // A second AttachElement(host, root) then fails with "already attached".
    if (rootElement->GetIsLayoutAttached() ||
        (host).GetTemplateRoot() != nullptr) {
        fragment->rootEdge.logicalParent = &host;
        fragment->rootEdge.visualParent =
            rootVisual->GetVisualParent() != nullptr
                ? rootVisual->GetVisualParent()
                : static_cast<Aero::Media::Visual*>(&host);
        fragment->rootEdge.child = rootVisual.Value();
    } else {
        Base::Result<Aero::ElementAttachment> rootMounted =
            context.AttachElement(host, *rootVisual.Value());
        if (!rootMounted) {
            restoreActiveNames();
            static_cast<void>(host.SetContent(nullptr));
            fragment->document.Clear();
            FreeObject(*state.allocator, Base::MemoryTag::Ui, fragment);
            return rootMounted.GetStatus();
        }
        fragment->rootEdge = std::move(rootMounted).Value();
    }

    const auto detachFailedFragment = [&]() noexcept {
        restoreActiveNames();
        static_cast<void>(DetachFragment(state, *fragment));
        FreeObject(*state.allocator, Base::MemoryTag::Ui, fragment);
    };

    Base::Result<void> activated = ActivateMountedSubtree(
        state, *rootVisual.Value(), fragment->document);
    if (!activated) {
        detachFailedFragment();
        return activated.GetStatus();
    }
    restoreActiveNames();
    state.fragmentMounts.PushBack(fragment);
    return {};
}

Base::Result<void> UnmountViewFragment(
    ViewFrame& state,
    Controls::ContentControl& host) noexcept {
    if (!state.initialized || !state.mounted) {
        return ViewInvalidState(
            "content fragment unmounting requires a mounted View");
    }
    for (std::uint32_t index = 0U;
         index < state.fragmentMounts.Size(); ++index) {
        if (state.fragmentMounts[index] != nullptr &&
            state.fragmentMounts[index]->host == &host) {
            return UnmountFragmentAt(state, index);
        }
    }
    return (host).GetContentElement() == nullptr
        ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidState,
              "content host does not contain a mounted XAML fragment"));
}


Base::Result<void> AdoptLoadedComponent(
    ViewFrame& state,
    Markup::LoaderResult&& incoming) noexcept {
    if (!state.initialized || !state.mounted ||
        state.Tree() == nullptr || state.Bindings() == nullptr ||
        state.Values() == nullptr || state.allocator == nullptr) {
        return {};
    }
    if (!incoming.root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "LoadComponent document has no root object");
    }

    ViewFrame::FragmentMount* mount = nullptr;
    Base::Result<void> allocated = AllocateObject(
        *state.allocator, Base::MemoryTag::Ui, mount);
    if (!allocated) return allocated.GetStatus();
    mount->document = std::move(incoming);
    state.componentMounts.PushBack(mount);
    Markup::LoaderResult& document = mount->document;
    Base::Result<Aero::Media::Visual*> rootVisual =
        state.ResolveVisual(
            *document.root, document.root->RuntimeType());
    if (!rootVisual) {
        state.componentMounts.PopBack();
        FreeObject(*state.allocator, Base::MemoryTag::Ui, mount);
        return rootVisual.GetStatus();
    }
    if (FrameworkElement* fe = ::Aero::TryCast<FrameworkElement>(document.root.Get())) {
        for (const auto& entry : document.names.Entries()) {
            if (entry.object != nullptr) {
                static_cast<void>(fe->RegisterName(entry.name.View(), *entry.object));
            }
        }
    }
    const Aero::NameScope* previousActiveNames = state.activeFragmentNames;
    state.activeFragmentNames = &document.names;
    struct ActiveNamesGuard {
        const Aero::NameScope*& target;
        const Aero::NameScope* previous;
        ~ActiveNamesGuard() { target = previous; }
    } namesGuard{state.activeFragmentNames, previousActiveNames};
    Base::Result<void> activated = ActivateMountedSubtree(
        state, *rootVisual.Value(), document);
    if (!activated) {
        state.componentMounts.PopBack();
        FreeObject(*state.allocator, Base::MemoryTag::Ui, mount);
        return activated.GetStatus();
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
