#include "gui/ViewFrame.hpp"
#include "gui/styles/FrameworkResourceResolver.hpp"
#include "gui/GuiRuntime.hpp"
#include "gui/text/TextPipeline.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "render/RenderTree.hpp"
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Freezable.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

void ViewFrame::ReportFrameFailure(
        Base::Status& slot,
        Base::Status status,
        std::uint16_t diagnosticNumber) noexcept {
        const bool repeated = slot.code == status.code &&
            slot.message == status.message;
        slot = status;
        if (repeated || status.IsOk() || options.diagnostics == nullptr) {
            return;
        }
        const char* msg = status.message != nullptr ? status.message : "";
        Base::Result<Diagnostics::Diagnostic> diagnostic =
            Diagnostics::Diagnostic::Create(
                Diagnostics::MakeDiagnosticCode(
                    Diagnostics::DiagnosticDomain::Render,
                    diagnosticNumber),
                Diagnostics::DiagnosticSeverity::Error,
                Base::StringView(
                    msg,
                    static_cast<std::uint32_t>(
                        std::strlen(msg))));
        if (!diagnostic) return;
        static_cast<void>(options.diagnostics->Report(
            std::move(diagnostic).Value()));
    }

void ViewFrame::ReportUpdateFailure(Base::Status status) noexcept {
        ReportFrameFailure(updateStatus, status, 101U);
    }

void ViewFrame::ReportRendererFailure(Base::Status status) noexcept {
        ReportFrameFailure(rendererStatus, status, 102U);
    }

void ViewFrame::ClearUpdateFailure() noexcept { updateStatus = {}; }

void ViewFrame::ClearRendererFailure() noexcept { rendererStatus = {}; }

void ViewFrame::RaiseFrameRendering(View& view) noexcept {
        ::Aero::Media::CompositionTarget::RaiseRendering(view);
    }

bool ViewFrame::HasAttachedRoot() const noexcept {
        return rootAttachment.IsAttached();
    }

Base::Result<void> ViewFrame::AttachVisualGraph(
        ::Aero::Media::Visual& rootVisual,
        UIElement& rootLayout,
        FrameworkElement* rootRender,
        Base::Span<Aero::Markup::VisualEdge> edges,
        Size availableSize) noexcept {
        if (Tree() == nullptr) {
            return ViewInvalidState(
                "Gui root cannot be attached in its current state");
        }
        Base::Result<void> attached = Tree()->AttachVisualGraph(
            rootVisual, edges, availableSize, rootAttachment);
        if (!attached) return attached.GetStatus();
        attachedRootVisual = &rootVisual;
        attachedRootLayout = &rootLayout;
        attachedRootRender = rootRender;
        return {};
    }

Base::Result<void> ViewFrame::CompleteVisualEdges(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (Tree() == nullptr || !HasAttachedRoot()) {
            return ViewInvalidState(
                "Deferred visual edges require an attached root");
        }
        return Tree()->CompleteVisualEdges(edges);
    }

Base::Result<void> ViewFrame::ResizeVisualRoot(Size availableSize) noexcept {
        if (!HasAttachedRoot() || attachedRootLayout == nullptr ||
            Tree() == nullptr) {
            return ViewNotInitialized(
                "View resize requires an attached layout root");
        }
        return Tree()->ResizeRoot(
            *attachedRootLayout, availableSize, attachedRootVisual);
    }

Base::Result<void> ViewFrame::DetachVisualGraph(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() && attachedRootVisual == nullptr) return {};
        if (Tree() == nullptr) {
            return ViewInvalidState(
                "Gui context is unavailable during root detach");
        }
        Base::Result<void> detached =
            Tree()->DetachVisualGraph(rootAttachment, edges);
        if (!detached) return detached.GetStatus();
        attachedRootVisual = nullptr;
        attachedRootLayout = nullptr;
        attachedRootRender = nullptr;
        return {};
    }

ResourceHost::ResourceHost(ViewFrame& owner) noexcept
    : view(&owner) {}

void ResourceHost::Bind() noexcept {}

Aero::ResourceEnvironment ResourceHost::Environment() const noexcept {
        return {
            &applicationResources,
            &themeResources,
            &systemResources};
    }

Base::Result<Aero::ResourceDictionary*>
ResourceHost::ResolveLayer(
        ResourceLayer layer) noexcept {
        switch (layer) {
        case ResourceLayer::Application:
            return &applicationResources;
        case ResourceLayer::Theme:
            return &themeResources;
        case ResourceLayer::System:
            return &systemResources;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View resource layer is invalid");
    }

Base::Result<void> ResourceHost::RebuildDynamicEnvironment() noexcept {
        dynamicResourceEnvironment.Clear();
        dynamicResourceEnvironment.AddMerged(systemResources);
        dynamicResourceEnvironment.AddMerged(themeResources);
        dynamicResourceEnvironment.AddMerged(applicationResources);
        return {};
    }

Aero::Media::Visual* ViewFrame::RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata->Types().IsDerivedFrom(
                root->RuntimeType(),
                Aero::Media::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Aero::Media::Visual*>(root.Get());
    }

Base::Result<Aero::Media::Visual*> ViewFrame::ResolveVisual(
        Base::Object& object, Meta::TypeId type) noexcept {
        if (!metadata->Types().IsDerivedFrom(object.RuntimeType(), type) ||
            !metadata->Types().IsDerivedFrom(
                type, Aero::Media::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        return static_cast<Aero::Media::Visual*>(&object);
    }

Base::Result<Aero::UIElement*> ViewFrame::ResolveUIElement(
        Base::Object& object, Meta::TypeId type) noexcept {
        Base::Result<Aero::Media::Visual*> visual =
            ResolveVisual(object, type);
        if (!visual) return visual.GetStatus();
        Aero::UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(visual.Value());
        if (element == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a UIElement");
        }
        return element;
    }

Aero::FrameworkElement* ViewFrame::ResolveFrameworkElement(
        Base::Object& object, Meta::TypeId type) noexcept {
        Base::Result<Aero::Media::Visual*> visual =
            ResolveVisual(object, type);
        return visual ? ::Aero::TryCast<::Aero::FrameworkElement>(visual.Value()) : nullptr;
    }

template<class T>
Base::Result<const T*> ResolveUiValue(
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

    Base::Result<Meta::Value> implicit = Aero::FrameworkResourceResolver::Lookup(
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

Base::Result<void> ApplyViewUi(ViewFrame& state, Aero::Media::Visual& root) noexcept {
        if (state.metadata == nullptr || state.Values() == nullptr || state.Bindings() == nullptr ||
            state.Events() == nullptr || state.Input() == nullptr || state.Styles() == nullptr ||
            state.Templates() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View UI state is unavailable");
        }

        if (state.Resources() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View UI state is unavailable");
        }
        const Aero::ResourceEnvironment resources =
            state.Resources()->Environment();
        Base::Vector<Aero::Media::Visual*> stack(state.allocator);
        stack.PushBack(&root);
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            if (state.gui != nullptr) {
                auto& guiRuntime = *state.gui;
                static_cast<void>(guiRuntime.AdoptPendingDocumentFor(*node, state));
            }

            static_cast<void>(state.Bindings()->ActivateDeferred(
                *static_cast<::Aero::DependencyObject*>(node)));
            if (auto* element = ::Aero::TryCast<::Aero::UIElement>(node)) {
                if (Aero::Base::Ref<Aero::Media::Transform> transform =
                        element->GetRenderTransform()) {
                    static_cast<void>(state.Bindings()->ActivateDeferred(*transform));
                }
            }
            bool styleApplied = false;
            Aero::FrameworkElement* element = ::Aero::TryCast<::Aero::FrameworkElement>(node);
            if (element != nullptr) {
                Base::Result<const Aero::Style*> resolved =
                    ResolveUiValue<Aero::Style>(
                        *element, Aero::FrameworkElement::StyleProperty,
                        resources,
                        "FrameworkElement Style value is not a Style");
                const Aero::Style* style =
                    resolved ? resolved.Value() : nullptr;
                if (style != nullptr && style->GetIsSealed()) {
                    if (state.Styles()->AppliedStyle(*element) != style) {
                        if (state.Interactivity() != nullptr) {
                            state.Interactivity()->ClearStyleDataTriggersFor(*element);
                        }
                        Base::Result<void> applied = state.Styles()->Apply(*element, *style);
                        if (applied) {
                            styleApplied = true;
                        }
                    }
                    if (state.Interactivity() != nullptr) {
                        static_cast<void>(state.Interactivity()->StartStyleDataTriggers(*element, *style));
                    }
                }
            }

            const bool isControl = state.metadata->Types().IsDerivedFrom(
                node->RuntimeType(), Controls::Control::StaticTypeId());
            if (styleApplied || isControl) {
                static_cast<void>(state.Values()->Flush());
            }

            if (isControl) {
                auto& control = *static_cast<Controls::Control*>(node);
                Base::Result<const Controls::ControlTemplate*> resolved =
                    ResolveUiValue<Controls::ControlTemplate>(
                        control, Controls::Control::TemplateProperty, resources,
                        "Control Template value is not a ControlTemplate");
                if (resolved) {
                    const Controls::ControlTemplate* controlTemplate =
                        resolved.Value();
                    if (controlTemplate != nullptr) {
                        const ::Aero::Controls::TemplateHandle existing =
                            state.Templates()->AppliedHandle(control);
                        if (!existing.IsValid() ||
                            state.Templates()->AppliedTemplate(existing) != controlTemplate) {
                            Base::Result<::Aero::Controls::TemplateHandle> applied =
                                state.Templates()->Apply(control, *controlTemplate);
                            if (applied) {
                                // TemplateEngine installs the handle while its
                                // transaction is active. Invoke the control callback
                                // only after Apply has returned so PART_* lookups and
                                // ItemsHost realization cannot re-enter that
                                // transaction.
                                (control).OnApplyTemplate();
                            }
                        }
                    } else {
                        (control).OnApplyTemplate();
                    }
                }
            }

            ForEachVisualAndLogicalChild(*node, [&stack](Aero::Media::Visual* child) {
                stack.PushBack(child);
            });
        }
        Base::Result<std::uint32_t> appliedValues = state.Values()->Flush();
        return appliedValues ? Base::Result<void>()
                             : Base::Result<void>(appliedValues.GetStatus());
    }

void DetachViewUi(
        ViewFrame& state,
        Aero::Media::Visual* root,
        Base::Span<Aero::Media::Visual* const> declarationNodes) noexcept {
        if (state.Values() == nullptr) return;

        Base::Vector<Aero::Media::Visual*> reachable(state.allocator);
        Base::HashSet<Aero::Media::Visual*> visited(state.allocator);
        const auto addReachable =
            [&reachable, &visited](Aero::Media::Visual* candidate) noexcept {
                if (candidate == nullptr) return;
                if (!visited.Insert(candidate).inserted) return;
                (void)reachable.PushBack(candidate);
            };
        if (root != nullptr) {
            addReachable(root);
        }
        for (Aero::Media::Visual* node : declarationNodes) {
            addReachable(node);
        }
        for (std::uint32_t index = 0U; index < reachable.Size(); ++index) {
            Aero::Media::Visual* node = reachable[index];
            if (node == nullptr) continue;
            for (Aero::Media::Visual* child :
                 (*node).RenderChildren()) {
                addReachable(child);
            }
            const std::uint32_t visualCount =
                Aero::Media::VisualTreeHelper::GetChildrenCount(*node);
            for (std::uint32_t i = 0U; i < visualCount; ++i) {
                if (auto* child = Aero::Media::VisualTreeHelper::GetChild(*node, i)) {
                    addReachable(child);
                }
            }
            const std::uint32_t logicalCount =
                Aero::LogicalTreeHelper::GetChildrenCount(*node);
            for (std::uint32_t i = 0U; i < logicalCount; ++i) {
                if (auto* child = Aero::LogicalTreeHelper::GetChild(*node, i)) {
                    if (auto* childVis = ::Aero::TryCast<Aero::Media::Visual>(child)) {
                        addReachable(childVis);
                    } else {
                        if (state.Animations() != nullptr) state.Animations()->RemoveTarget(*child);
                        if (state.Bindings() != nullptr) (void)state.Bindings()->DetachObject(*child);
                        if (state.Values() != nullptr) (void)state.Values()->DetachObject(*child);
                    }
                }
            }
        }

        Base::Vector<DependencyObject*> detachedPropertyObjects(state.allocator);
        for (std::uint32_t index = reachable.Size(); index > 0U; --index) {
            Aero::Media::Visual* node = reachable[index - 1U];
            if (node == nullptr) continue;
            if (state.Tree() != nullptr) {
                state.Tree()->DetachPropertyDependencyGraph(
                    *node, state.Animations(), detachedPropertyObjects);
            }
            if (state.metadata != nullptr &&
                state.metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::ItemsControl::StaticTypeId())) {
                auto& itemsControl = *static_cast<Controls::ItemsControl*>(node);
                if (auto* gen = itemsControl.GetItemContainerGenerator()) {
                    gen->Detach();
                }
                for (std::uint32_t i = 0U; i < state.trackedItemsControls.Size(); ++i) {
                    if (state.trackedItemsControls[i] == &itemsControl) {
                        state.trackedItemsControls.Erase(i);
                        break;
                    }
                }
            }
            if (state.Animations() != nullptr) {
                state.Animations()->RemoveTarget(*node);
            }
            if (state.metadata != nullptr &&
                state.metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                auto& control = *static_cast<Controls::Control*>(node);
                if (state.VisualStates() != nullptr) {
                    (void)::Aero::Controls::FrameworkTemplateState::Clear(
                        *state.VisualStates(), control);
                }
                if (state.Templates() != nullptr) {
                    (void)state.Templates()->Clear(control);
                }
            }
            if (state.Bindings() != nullptr) {
                (void)state.Bindings()->DetachObject(*node);
            }
            Aero::FrameworkElement* element = ::Aero::TryCast<::Aero::FrameworkElement>(node);
            if (element != nullptr && state.Styles() != nullptr) {
                if (state.Interactivity() != nullptr) {
                    state.Interactivity()->ClearStyleDataTriggersFor(*element);
                }
                (void)state.Styles()->DetachObject(*element);
            }
            if (state.Values() != nullptr) {
                (void)state.Values()->DetachObject(*node);
            }
            if (state.Tree() != nullptr) {
                state.Tree()->InvalidateNodeHandle(*node);
            }
        }
    }

ViewEngines::ViewEngines(
    ViewFrame& frame,
    Base::IAllocator* allocator,
    ::Aero::Threading::Dispatcher& dispatcher,
    ::Aero::Meta::Registry& metadata) noexcept
    : objectFactory(dispatcher, metadata.DependencyProperties(), metadata),
      values(dispatcher, metadata.DependencyProperties()),
      animations(dispatcher, values, allocator),
      tree(dispatcher, values),
      layout(dispatcher),
      renderTree(dispatcher),
      images(allocator),
      text(allocator),
      bindings(dispatcher, &metadata),
      events(metadata.RoutedEventState()),
      input(tree, events),
      interactivity(frame),
      storyboards(frame),
      resources(frame),
      styles(values, metadata.DependencyProperties()),
      templates(
          tree,
          values,
          metadata.DependencyProperties(),
          &layout,
          &renderTree,
          &metadata,
          &bindings,
          &resources.dynamicResourceEnvironment),
      overlays(frame),
      focus(frame) {}

ViewEngines::~ViewEngines() noexcept {
    if (visualStates != nullptr) {
        delete visualStates;
        visualStates = nullptr;
    }
}


Base::Result<void> ViewFrame::GeneratedItemSubtreeChanged(
        Aero::Media::Visual& root,
        Controls::ItemSubtreeChange change,
        void* context) noexcept {
        auto* runtime = static_cast<ViewFrame*>(context);
        if (runtime == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Generated item subtree runtime context is null");
        }
        if (change == Controls::ItemSubtreeChange::Unmounting) {
            Base::Result<Aero::VisualHandle> rootHandle = runtime->Tree()->GetHandle(root);
            if (rootHandle) {
                for (std::uint32_t index = 0U; index < runtime->pendingGeneratedVisuals.Size(); ++index) {
                    if (runtime->pendingGeneratedVisuals[index] == rootHandle.Value()) {
                        runtime->pendingGeneratedVisuals.Erase(index);
                        break;
                    }
                }
            }
            DetachViewUi(*runtime, &root, {});
            return {};
        }
        if (runtime->deferGeneratedActivation ||
            (runtime->Bindings() != nullptr &&
             runtime->Bindings()->IsFlushing())) {
            Base::Result<Aero::VisualHandle>
                handle =
                    runtime->Tree()->GetHandle(root);
            if (!handle) {
                // ItemTemplate header visuals (gallery SampleTemplate
                // StackPanel) are callback'd before PART_Header hosts them.
                // Failing Refresh here left realized=0. The generated
                // container is queued separately; ApplyViewUi walks hosted
                // header children after the control template is applied.
                return {};
            }
            runtime->
                pendingGeneratedVisuals.
                    PushBack(handle.Value());

            return {};
        }
        Base::Result<void> applied =
            ApplyViewUi(*runtime, root);
        if (!applied) {
            return {};
        }
        Base::Result<void> attached =
            runtime->VisitAndAttach(root);
        if (!attached) {
            return {};
        }
        static_cast<void>(runtime->Bindings()->Flush());
        if (runtime->Storyboards() != nullptr) {
            static_cast<void>(runtime->Storyboards()->StartLoadedAnimations(&root));
        }
        return {};
    }

Base::Result<void>
 ViewFrame::FlushGeneratedVisuals() noexcept {
        constexpr std::uint32_t MaximumWaves = 16U;
        for (std::uint32_t wave = 0U;
             wave < MaximumWaves;
             ++wave) {
            if (pendingGeneratedVisuals.Empty()) {
                return {};
            }
            Base::Vector<Aero::VisualHandle>
                pending =
                    std::move(
                        pendingGeneratedVisuals);
            pendingGeneratedVisuals.Clear();
            for (const Aero::VisualHandle handle :
                 pending) {
                Aero::Media::Visual* subtreeRoot =
                    Tree()->ResolveHandle(handle);
                if (subtreeRoot == nullptr) continue;
                Base::Result<void> applied =
                    ApplyViewUi(
                        *this,
                        *subtreeRoot);
                if (!applied) {
                    continue;
                }
                Base::Result<void> attached =
                    VisitAndAttach(
                        *subtreeRoot);
                if (!attached) {
                    continue;
                }
                if (Storyboards() != nullptr) {
                    static_cast<void>(Storyboards()->StartLoadedAnimations(subtreeRoot));
                }
            }
            Base::Result<std::uint32_t> rebound =
                Bindings()->Flush();
            if (!rebound) return rebound.GetStatus();
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Generated item visual activation exceeded the bounded activation waves");
    }

Base::Result<void> ViewFrame::AttachItemGenerator(
        Controls::ItemsControl& itemsControl) noexcept {
        if (((itemsControl).GetItemContainerGenerator() != nullptr)) {
            Controls::ItemContainerGenerator* generator =
                itemsControl.GetItemContainerGenerator();
            Controls::Panel* host = itemsControl.GetItemsHost();
            if (generator == nullptr || host == nullptr) return {};
            if (metadata->Types().IsDerivedFrom(
                    host->RuntimeType(),
                    Controls::VirtualizingPanel::StaticTypeId())) {
                return {};
            }
            if (itemsControl.GetCount() == generator->GetGeneratedCount()) {
                return {};
            }
            if (!generator->Refresh()) {
                return {};
            }
            Base::Result<void> generatedUiApplied = ApplyViewUi(*this, *host);
            if (!generatedUiApplied) return generatedUiApplied.GetStatus();
            return {};
        }
        Controls::Panel* host = itemsControl.GetItemsHost();
        if (host == nullptr) return {};
        if (VisualTree(itemsControl) != Tree() || VisualTree(host) != Tree()) {
            return {};
        }

        Base::Result<Controls::ItemContainerGenerator*> created =
            Controls::ItemContainerGeneratorFactory::Create(
                *Tree(),
                *Layout(),
                *Values(),
                Styles(),
                RenderTree(),
                Templates(),
                &ViewFrame::GeneratedItemSubtreeChanged,
                this);
        if (!created) {
            return created.GetStatus();
        }
        Controls::ItemContainerGenerator* generator = created.Value();
        bool attached = false;
        if (metadata->Types().IsDerivedFrom(
                host->RuntimeType(),
                Controls::VirtualizingPanel::StaticTypeId())) {
            attached = generator->AttachVirtualized(
                itemsControl,
                *static_cast<Controls::VirtualizingPanel*>(host));
        } else {
            attached = generator->Attach(itemsControl, *host);
        }
        if (!attached) {
            delete generator;
            return {};
        }

        Base::Result<void> generatedUiApplied = ApplyViewUi(*this, *host);
        if (!generatedUiApplied) {
            DetachViewUi(*this, host, {});
            generator->Detach();
            delete generator;
            return generatedUiApplied.GetStatus();
        }
        itemGenerators.PushBack(generator);
        return {};
    }

Base::Result<void> ViewFrame::AttachPendingItemGenerators(
        Aero::Media::Visual& /*rootVisual*/) noexcept {
        for (std::uint32_t i = 0U; i < trackedItemsControls.Size(); ++i) {
            Controls::ItemsControl* ic = trackedItemsControls[i];
            if (ic == nullptr) continue;
            Base::Result<void> attached = AttachItemGenerator(*ic);
            if (!attached) return attached.GetStatus();
        }
        return {};
    }



Base::Result<void> ViewFrame::VisitAndAttach(
        Aero::Media::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        stack.PushBack(&rootVisual);
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Meta::TypeId type = node->RuntimeType();
            if (options.textInputMethodHost != nullptr) {
                if (metadata->Types().IsDerivedFrom(type, Controls::TextBox::StaticTypeId())) {
                    static_cast<Controls::TextBox*>(node)->SetInputMethodHost(options.textInputMethodHost);
                } else if (metadata->Types().IsDerivedFrom(type, Controls::PasswordBox::StaticTypeId())) {
                    static_cast<Controls::PasswordBox*>(node)->SetInputMethodHost(options.textInputMethodHost);
                }
            }
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ItemsControl::StaticTypeId())) {
                auto* ic = static_cast<Controls::ItemsControl*>(node);
                bool alreadyTracked = false;
                for (std::uint32_t i = 0U; i < trackedItemsControls.Size(); ++i) {
                    if (trackedItemsControls[i] == ic) {
                        alreadyTracked = true;
                        break;
                    }
                }
                if (!alreadyTracked) {
                    (void)trackedItemsControls.PushBack(ic);
                }
                Base::Result<void> attached = AttachItemGenerator(*ic);
                if (!attached) return attached.GetStatus();
            }
            ForEachVisualAndLogicalChild(*node, [&stack](Aero::Media::Visual* child) {
                stack.PushBack(child);
            });
        }
        return {};
    }




namespace {

Base::Result<void> AddFrameCallbacks(
    ViewFrameResult& result,
    std::uint32_t count,
    const char* overflowMessage) noexcept {
    if (result.callbackCount > UINT32_MAX - count) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange, overflowMessage);
    }
    result.callbackCount += count;
    return {};
}

Base::Result<void> SynchronizeFrameResources(ViewFrame& state) noexcept {
    bool deviceGenerationChanged = false;
    GuiRuntime& guiState = *state.gui;
    const bool fontProviderChanged =
        guiState.fontChangeGeneration != state.seenFontProviderChange;
    if (fontProviderChanged) {
        state.seenFontProviderChange = guiState.fontChangeGeneration;
    }
    if (state.Images() != nullptr &&
        guiState.textureChangeGeneration != state.seenTextureProviderChange) {
        if (guiState.textureChangesLost) {
            state.Images()->Invalidate({}, state.GetImageResources());
        } else {
            for (const XamlProviderChangeRecord& change :
                 guiState.textureChanges) {
                if (change.generation <= state.seenTextureProviderChange) {
                    continue;
                }
                state.Images()->Invalidate(change.uri, state.GetImageResources());
            }
        }
        state.seenTextureProviderChange = guiState.textureChangeGeneration;
    }
    if (state.device) {
        if (state.device->State() != RenderDeviceState::Ready) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState, "Device is not ready");
        }
        const std::uint64_t generation = state.device->Generation();
        if (generation != state.deviceGeneration) {
            deviceGenerationChanged = true;
            Aero::Media::Visual* rootVisual = state.RootVisual();
            if (rootVisual != nullptr) {
                state.RenderTree()->Invalidate(
                    *rootVisual, Aero::Render::RenderInvalidation::All);
            }
            if (state.Tree() != nullptr) {
                state.Tree()->SetMeshResources(state.GetMeshResources());
            }
            state.deviceGeneration = generation;
        }
    }
    if (state.Text() != nullptr) {
        Base::Result<bool> synchronized = state.Text()->SynchronizeBackend(
            *state.device,
            state.publicRenderer.Resources().text,
            deviceGenerationChanged || fontProviderChanged);
        if (!synchronized) return synchronized.GetStatus();
        if (synchronized.Value()) {
            if (state.RootVisual() != nullptr) {
                if (auto* element = ::Aero::TryCast<::Aero::UIElement>(state.RootVisual())) {
                    element->InvalidateMeasure();
                }
                state.RenderTree()->Invalidate(
                    *state.RootVisual(), Aero::Render::RenderInvalidation::All);
            }
        }
    }
    if (state.Images() != nullptr) {
        Base::Result<bool> synchronized = state.Images()->Synchronize(
            state.RootVisual(),
            state.loadedDocument.canonicalUri,
            *state.xamlProviders,
            guiState.textureProvider.Get(),
            state.GetImageResources(),
            deviceGenerationChanged);
        if (!synchronized) return synchronized.GetStatus();
        if (synchronized.Value()) {
            Aero::Media::Visual* rootVisual = state.RootVisual();
            if (rootVisual != nullptr) {
                state.RenderTree()->Invalidate(
                    *rootVisual, Aero::Render::RenderInvalidation::All);
            }
        }
    }
    return {};
}

} // namespace

Base::Result<std::uint32_t> ExecuteViewFrame(ViewFrame& state, View& view) noexcept {
    if (!state.initialized) {
        return ViewNotInitialized(
            "View must be initialized before running frames");
    }
    // P3.1: pump cross-thread work first. This is the single frame funnel
    // for DesktopHost, tests and samples, so one hookup covers every host.
    // Failures here must not abort the frame: async work is best-effort.
    if (state.dispatcher != nullptr) {
        static_cast<void>(state.dispatcher->ProcessPending());
    }
    const bool skipAnimationPhase =
        state.Animations() == nullptr;

    bool skipUnsyncedVisualPhases = false;
    Base::Result<void> resources = SynchronizeFrameResources(state);
    if (!resources) {
        // Missing theme/image/XAML source files must not skip DataBind.
        // Layout/render can dereference unsynchronized image/font backends.
        state.ReportRendererFailure(resources.GetStatus());
        skipUnsyncedVisualPhases = true;
    }

    // P3.2: explicit direct phase driving. The Dispatcher::RunFramePhase
    // hook table is retired (no engine registers hooks anymore); ViewFrame
    // is the single frame funnel and calls each engine entry directly in
    // WPF order:
    //   PropertyChanges (values->Flush)
    //   -> DataBind (bindings DataBindHook, single canonical call)
    //   -> Animation (animations AnimationFrameHook)
    //   -> Lifecycle (focus + initial values)
    //   -> Layout (layout LayoutHook)
    //   -> RenderCommit (renderer RenderCommitHook)
    // BeginFrame/Input/EndFrame had no registered hooks (P1.3) and are gone.
    ViewFrameResult result;

    // ---- PropertyChanges ----
    if (state.Values() != nullptr) {
        Base::Result<std::uint32_t> flushed = state.Values()->Flush();
        if (!flushed) return flushed.GetStatus();
        Base::Result<void> counted = AddFrameCallbacks(
            result, flushed.Value(), "Frame callback count overflow");
        if (!counted) return counted.GetStatus();
    }

    // ---- DataBind (single canonical call, merges the two discrete stages) ----
    if (state.Bindings() != nullptr) {
        Aero::BindingEngine::DataBindHook(state.Bindings());
    }
    {
        Base::Result<void> generatedVisualsFlushed =
            state.FlushGeneratedVisuals();
        if (!generatedVisualsFlushed) {
            return generatedVisualsFlushed.GetStatus();
        }
        if (Aero::Media::Visual* rootVisual = state.RootVisual()) {
            Base::Result<void> generators =
                state.AttachPendingItemGenerators(*rootVisual);
            if (!generators) {
                return generators.GetStatus();
            }
        }
        if (state.Styles() != nullptr) {
            state.Styles()->Flush();
        }
        if (state.Interactivity() != nullptr) {
            state.Interactivity()->FlushPendingStyleDataTriggerEvaluations();
            state.Interactivity()->RetryPendingInteractionTriggers();
        }
    }

    // ---- Animation ----
    if (!skipAnimationPhase && state.Animations() != nullptr) {
        Aero::AnimationEngine::AnimationFrameHook(state.Animations());
        const Base::Status animationStatus =
            state.Animations()->LastTickStatus();
        if (!animationStatus.IsOk()) {
            state.ReportUpdateFailure(animationStatus);
        }

        if (state.Storyboards() != nullptr) {
            Base::Result<std::uint32_t> completed =
                state.Storyboards()->ProcessStoryboardCompletions();
            if (!completed) return completed.GetStatus();
            Base::Result<void> counted = AddFrameCallbacks(
                result,
                completed.Value(),
                "Frame callback count overflow");
            if (!counted) return counted.GetStatus();
        }
        if (state.Animations()->Diagnostics().appliedValueCount != 0U) {
            if (Aero::Media::Visual* rootVisual = state.RootVisual()) {
                if (state.RenderTree() != nullptr &&
                    state.RenderTree()->DirtyCount() == 0U) {
                    state.RenderTree()->Invalidate(
                        *rootVisual, ::Aero::Render::RenderInvalidation::Drawing);
                }
            }
        }
    }

    // ---- Lifecycle ----
    {
        if (state.Tree() != nullptr) {
            Base::Result<std::uint32_t> lifecycleCount =
                state.Tree()->FlushLifecycle();
            if (!lifecycleCount) return lifecycleCount.GetStatus();
            Base::Result<void> countedLifecycle = AddFrameCallbacks(
                result, lifecycleCount.Value(), "Lifecycle callback count overflow");
            if (!countedLifecycle) return countedLifecycle.GetStatus();
        }
        Base::Result<std::uint32_t> focused = state.Focus() != nullptr
            ? state.Focus()->ProcessPendingFocus()
            : Base::Result<std::uint32_t>(0U);
        if (!focused) return focused.GetStatus();
        Base::Result<void> counted = AddFrameCallbacks(
            result, focused.Value(), "View callback count overflow");
        if (!counted) return counted.GetStatus();
        if (state.Animations() != nullptr) {
            Base::Result<std::uint32_t> initialValues =
                state.Animations()->ApplyPendingInitialValues();
            if (!initialValues) return initialValues.GetStatus();
            Base::Result<void> countedInitial = AddFrameCallbacks(
                result,
                initialValues.Value(),
                "Initial animation callback count overflow");
            if (!countedInitial) return countedInitial.GetStatus();
        }
    }

    // ---- Layout ----
    if (!skipUnsyncedVisualPhases) {
        if (state.HasAttachedRoot() && state.Tree() != nullptr) {
            Base::Result<void> completed = state.Tree()->CompleteVisualEdges({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()});
            if (!completed) {
                return completed.GetStatus();
            }
        }
        if (state.Layout() != nullptr) {
            Aero::LayoutEngine::LayoutHook(state.Layout());
            if (!state.Layout()->LastFlushStatus().IsOk()) {
                return state.Layout()->LastFlushStatus();
            }
        }
        if (Aero::Media::Visual* rootVisual = state.RootVisual()) {
            Base::Result<void> generators =
                state.AttachPendingItemGenerators(*rootVisual);
            if (!generators) return generators.GetStatus();
        }
        if (state.Layout() != nullptr &&
            state.Layout()->Diagnostics().arrangedCount != 0U) {
            if (state.Bindings() != nullptr) {
                Base::Result<std::uint32_t> rebound =
                    state.Bindings()->Flush();
                if (!rebound) return rebound.GetStatus();
            }
            if (state.Storyboards() != nullptr) {
                Base::Result<void> loaded =
                    state.Storyboards()->FlushPendingLoadedTriggers();
                if (!loaded) return loaded.GetStatus();
            }
        }
        if (state.Layout() != nullptr &&
            state.Layout()->Diagnostics().arrangedCount != 0U) {
            if (state.Interactivity() != nullptr) {
                state.Interactivity()->NotifyLayoutUpdated();
            }
        }
    }

    // ---- RenderCommit ----
    if (!skipUnsyncedVisualPhases) {
        state.RaiseFrameRendering(view);
        if (state.Overlays() != nullptr) {
            Base::Result<void> overlaySync =
                state.Overlays()->SynchronizeOverlays();
            if (!overlaySync) return overlaySync.GetStatus();
        }
        if (state.RenderTree() != nullptr) {
            ::Aero::Render::RenderTree::RenderCommitHook(state.RenderTree());
            const Base::Status committed = state.RenderTree()->LastCommitStatus();
            if (!committed.IsOk()) return committed;
        }
        if (state.Animations() != nullptr) {
            state.Animations()->CommitPendingInitialValues();
        }
    }

    if (state.Text() != nullptr) {
        state.Text()->CollectGarbage();
    }
    result.frameNumber = ++state.frameNumber;
    const Aero::LayoutDiagnostics layoutDiagnostics =
        state.Layout()->Diagnostics();
    result.layout.passVersion = layoutDiagnostics.passVersion;
    result.layout.measuredCount = layoutDiagnostics.measuredCount;
    result.layout.arrangedCount = layoutDiagnostics.arrangedCount;
    result.layout.pendingMeasureCount = layoutDiagnostics.pendingMeasureCount;
    result.layout.pendingArrangeCount = layoutDiagnostics.pendingArrangeCount;
    const ::Aero::Render::RenderDiagnostics render =
        state.RenderTree()->Diagnostics();
    result.render.snapshotVersion = render.commitVersion;
    result.render.nodeCount = render.nodeCount;
    result.render.commandCount = render.commandCount;
    result.render.glyphCommandCount = render.glyphCommandCount;
    result.render.dirtyCount = render.dirtyCount;
    result.render.snapshotHash = render.frameHash;
    if (state.device) {
        const Diagnostics::RenderFrameStatistics deviceStatistics =
            Diagnostics::GetLastRenderFrameStatistics(*state.device);
        result.render.drawPacketCount = deviceStatistics.drawPacketCount;
        result.render.batchCount = deviceStatistics.batchCount;
        result.render.drawCallCount = deviceStatistics.drawCallCount;
        result.render.mergedPacketCount = deviceStatistics.mergedPacketCount;
        result.render.barrierCount = deviceStatistics.barrierCount;
        result.render.instanceCount = deviceStatistics.instanceCount;
        result.render.stateBindingCount = deviceStatistics.stateBindingCount;
        result.render.batchingEnabled = deviceStatistics.batchingEnabled;
    }
    return result.callbackCount;
}


Base::Result<std::uint32_t> AdvanceViewClocks(
    ViewFrame& state,
    std::uint64_t elapsedMicroseconds) noexcept {
    if (!state.mounted || state.Animations() == nullptr) {
        return ViewNotInitialized(
            "View timing requires a mounted animation manager");
    }
    const std::uint32_t elapsedMilliseconds =
        static_cast<std::uint32_t>(std::min<std::uint64_t>(
            elapsedMicroseconds / 1000U, UINT32_MAX));
    std::uint32_t actionCount = 0U;
    if (state.Tree() != nullptr) {
        actionCount += state.Tree()->AdvanceRepeatButtonTime(elapsedMilliseconds);
    }
    const std::uint32_t toolTips =
        state.Overlays() != nullptr
            ? state.Overlays()->AdvanceToolTipTime(elapsedMilliseconds)
            : 0U;
    if (actionCount > UINT32_MAX - toolTips) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Control timing action count overflow");
    }
    actionCount += toolTips;
    if (state.options.automaticAnimationClock) {
        return actionCount;
    }
    Base::Result<std::uint32_t> animations =
        state.Animations()->AdvanceBy(
            static_cast<Aero::Media::Animation::AnimationTime>(
                elapsedMicroseconds));
    if (!animations) return animations.GetStatus();
    Base::Result<std::uint32_t> completed =
        state.Storyboards() != nullptr
            ? state.Storyboards()->ProcessStoryboardCompletions()
            : Base::Result<std::uint32_t>(0U);
    if (!completed) return completed.GetStatus();
    if (actionCount > UINT32_MAX - animations.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "View timing action count overflow");
    }
    actionCount += animations.Value();
    if (actionCount > UINT32_MAX - completed.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Storyboard completed action count overflow");
    }
    return actionCount + completed.Value();
}


void ViewFrame::BeginDestroyInteractions() noexcept {
        if (Aero::Media::Visual* rootVisual = RootVisual()) {
            if (Interactivity() != nullptr) {
                Interactivity()->DetachBehaviorsInSubtree(*rootVisual);
            }
        }
        if (Overlays() != nullptr) {
            Overlays()->CloseAllOverlays();
            Overlays()->ClearOverlays();
        }
        if (Interactivity() != nullptr) {
            Interactivity()->ClearAnimationEventSubscriptions();
        }
        if (Overlays() != nullptr) {
            if (Overlays()->activeToolTip) {
                static_cast<void>(
                    Overlays()->activeToolTip->SetIsOpen(false));
            }
            Overlays()->pendingToolTip.Reset();
            Overlays()->activeToolTip.Reset();
            Overlays()->toolTipTarget.Reset();
        }
        if (options.textInputMethodHost != nullptr) {
            options.textInputMethodHost->SetClient(nullptr);
        }
    }

void ViewFrame::FinishDestroyInteractions() noexcept {
        while (!itemGenerators.Empty()) {
            Controls::ItemContainerGenerator*
                generator = itemGenerators.Back();
            itemGenerators.PopBack();
            if (generator != nullptr) {
                generator->Detach();
                delete generator;
                generator = nullptr;
            }
        }
        trackedItemsControls.Clear();
        if (Input() != nullptr) {
            Input()->SetRoot(nullptr);
        }
    }

void ViewFrame::DestroyInteractions() noexcept {
        BeginDestroyInteractions();
        FinishDestroyInteractions();
    }

Base::Result<void> ViewFrame::CreateInteractions() noexcept {
        Aero::Media::Visual* rootVisual = RootVisual();
        if (rootVisual == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        if (Input() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "InputRouter is unavailable");
        }
        Input()->SetRoot(rootVisual);
        return {};
    }


} // namespace Aero
