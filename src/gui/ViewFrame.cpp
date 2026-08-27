#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include <Aero/Controls/ControlTemplate.hpp>

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

void ViewState::ReportFrameFailure(
        Base::Status& slot,
        Base::Status status,
        std::uint16_t diagnosticNumber) noexcept {
        const bool repeated = slot.code == status.code &&
            slot.message == status.message;
        slot = status;
        if (repeated || status.IsOk() || options.diagnostics == nullptr) {
            return;
        }
        Base::Result<Diagnostics::Diagnostic> diagnostic =
            Diagnostics::Diagnostic::Create(
                Diagnostics::MakeDiagnosticCode(
                    Diagnostics::DiagnosticDomain::Render,
                    diagnosticNumber),
                Diagnostics::DiagnosticSeverity::Error,
                Base::StringView(
                    status.message,
                    static_cast<std::uint32_t>(
                        std::strlen(status.message))));
        if (!diagnostic) return;
        static_cast<void>(options.diagnostics->Report(
            std::move(diagnostic).Value()));
    }

void ViewState::ReportUpdateFailure(Base::Status status) noexcept {
        ReportFrameFailure(updateStatus, status, 101U);
    }

void ViewState::ReportRendererFailure(Base::Status status) noexcept {
        ReportFrameFailure(rendererStatus, status, 102U);
    }

void ViewState::ClearUpdateFailure() noexcept { updateStatus = {}; }

void ViewState::ClearRendererFailure() noexcept { rendererStatus = {}; }

void ViewState::RaiseFrameRendering(View& view) noexcept {
        ::Aero::Media::CompositionTarget::RaiseRendering(view);
    }

bool ViewState::HasAttachedRoot() const noexcept {
        return rootAttachment.IsAttached();
    }

Base::Result<void> ViewState::AttachVisualGraph(
        ::Aero::Media::Visual& rootVisual,
        UIElement& rootLayout,
        FrameworkElement* rootRender,
        Base::Span<Aero::Markup::VisualEdge> edges,
        Size availableSize) noexcept {
        if (tree == nullptr) {
            return ViewInvalidState(
                "Gui root cannot be attached in its current state");
        }
        Base::Result<void> attached = tree->AttachVisualGraph(
            rootVisual, edges, availableSize, rootAttachment);
        if (!attached) return attached.GetStatus();
        attachedRootVisual = &rootVisual;
        attachedRootLayout = &rootLayout;
        attachedRootRender = rootRender;
        return {};
    }

Base::Result<void> ViewState::CompleteVisualEdges(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (tree == nullptr || !HasAttachedRoot()) {
            return ViewInvalidState(
                "Deferred visual edges require an attached root");
        }
        return tree->CompleteVisualEdges(edges);
    }

Base::Result<void> ViewState::ResizeVisualRoot(Size availableSize) noexcept {
        if (!HasAttachedRoot() || attachedRootLayout == nullptr ||
            tree == nullptr) {
            return AeroNotInitialized(
                "View resize requires an attached layout root");
        }
        return tree->ResizeRoot(
            *attachedRootLayout, availableSize, attachedRootVisual);
    }

Base::Result<void> ViewState::DetachVisualGraph(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() && attachedRootVisual == nullptr) return {};
        if (tree == nullptr) {
            return ViewInvalidState(
                "Gui context is unavailable during root detach");
        }
        Base::Result<void> detached =
            tree->DetachVisualGraph(rootAttachment, edges);
        if (!detached) return detached.GetStatus();
        attachedRootVisual = nullptr;
        attachedRootLayout = nullptr;
        attachedRootRender = nullptr;
        return {};
    }

ResourceHost::ResourceHost(ViewState& owner) noexcept
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
        Base::Result<void> rebuilt =
            dynamicResourceEnvironment.AddMerged(
                systemResources);
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.AddMerged(
                    themeResources);
        }
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.AddMerged(
                    applicationResources);
        }
        return rebuilt;
    }

Aero::Media::Visual* ViewState::RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata->Types().IsDerivedFrom(
                root->RuntimeType(),
                Aero::Media::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Aero::Media::Visual*>(root.Get());
    }

Base::Result<Aero::Media::Visual*> ViewState::ResolveVisual(
        Base::Object& object, Meta::TypeId type) noexcept {
        if (object.RuntimeType() != type ||
            !metadata->Types().IsDerivedFrom(
                type, Aero::Media::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        return static_cast<Aero::Media::Visual*>(&object);
    }

Base::Result<Aero::UIElement*> ViewState::ResolveUIElement(
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

Aero::FrameworkElement* ViewState::ResolveFrameworkElement(
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

Base::Result<void> ApplyViewUi(ViewState& state, Aero::Media::Visual& root) noexcept {
        if (state.metadata == nullptr || state.values == nullptr || state.bindings == nullptr ||
            state.events == nullptr || state.input == nullptr || state.styles == nullptr ||
            state.templates == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View UI state is unavailable");
        }

        if (state.resources == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View UI state is unavailable");
        }
        const Aero::ResourceEnvironment resources =
            state.resources->Environment();
        Base::Vector<Aero::Media::Visual*> stack(state.allocator);
        Base::Result<void> pushed = stack.PushBack(&root);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;

            Base::Result<std::uint32_t> activated =
                state.bindings->ActivateDeferred(
                    *static_cast<::Aero::DependencyObject*>(node));
            if (!activated) return activated.GetStatus();

            Aero::FrameworkElement* element = ::Aero::TryCast<::Aero::FrameworkElement>(node);
            if (element != nullptr) {
                Base::Result<const Aero::Style*> resolved =
                    ResolveUiValue<Aero::Style>(
                        *element, Aero::FrameworkElement::StyleProperty,
                        resources,
                        "FrameworkElement Style value is not a Style");
                if (!resolved) return resolved.GetStatus();
                const Aero::Style* style = resolved.Value();
                if (style != nullptr) {
                    if (!style->GetIsSealed()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "Implicit Style is not sealed");
                    }
                    if (state.styles->AppliedStyle(*element) != style) {
                        if (state.interactivity != nullptr) {
                            state.interactivity->ClearStyleDataTriggersFor(*element);
                        }
                        Base::Result<void> applied = state.styles->Apply(*element, *style);
                        if (!applied) return applied.GetStatus();
                    }
                    Base::Result<std::uint32_t> dataTriggers =
                        state.interactivity != nullptr
                        ? state.interactivity->StartStyleDataTriggers(*element, *style)
                        : Base::Result<std::uint32_t>(std::uint32_t{0U});
                    if (!dataTriggers) return dataTriggers.GetStatus();
                }
            }

            Base::Result<std::uint32_t> styleValues = state.values->Flush();
            if (!styleValues) return styleValues.GetStatus();

            if (state.metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                auto& control = *static_cast<Controls::Control*>(node);
                Base::Result<const Controls::ControlTemplate*> resolved =
                    ResolveUiValue<Controls::ControlTemplate>(
                        control, Controls::Control::TemplateProperty, resources,
                        "Control Template value is not a ControlTemplate");
                if (!resolved) return resolved.GetStatus();
                const Controls::ControlTemplate* controlTemplate =
                    resolved.Value();
                if (controlTemplate != nullptr) {
                    const ::Aero::Controls::TemplateHandle existing =
                        state.templates->AppliedHandle(control);
                    if (!existing.IsValid() ||
                        state.templates->AppliedTemplate(existing) != controlTemplate) {
                        Base::Result<::Aero::Controls::TemplateHandle> applied =
                            state.templates->Apply(control, *controlTemplate);
                        if (!applied) return applied.GetStatus();
                        // TemplateEngine installs the handle while its
                        // transaction is active. Invoke the control callback
                        // only after Apply has returned so PART_* lookups and
                        // ItemsHost realization cannot re-enter that
                        // transaction.
                        AeroGuiInternal::
                            InvokeTemplateApplied(control);
                    }
                }
            }

            for (Aero::Media::Visual* child :
                 AeroGuiInternal::RenderChildren(*node)) {
                pushed = stack.PushBack(child);
                if (!pushed) return pushed.GetStatus();
            }
        }
        Base::Result<std::uint32_t> appliedValues = state.values->Flush();
        return appliedValues ? Base::Result<void>()
                             : Base::Result<void>(appliedValues.GetStatus());
    }

void DetachViewUi(
        ViewState& state,
        Aero::Media::Visual* root,
        Base::Span<Aero::Media::Visual* const> declarationNodes) noexcept {
        if (state.values == nullptr) return;

        Base::Vector<Aero::Media::Visual*> reachable(state.allocator);
        if (root != nullptr) {
            (void)reachable.PushBack(root);
            for (std::uint32_t index = 0U; index < reachable.Size(); ++index) {
                Aero::Media::Visual* node = reachable[index];
                if (node == nullptr) continue;
                for (Aero::Media::Visual* child :
                     AeroGuiInternal::RenderChildren(*node)) {
                    if (child != nullptr) (void)reachable.PushBack(child);
                }
            }
        }

        for (Aero::Media::Visual* node : reachable) {
            if (node == nullptr) continue;
            if (state.bindings != nullptr) (void)state.bindings->DetachObject(*node);
            Aero::FrameworkElement* element = ::Aero::TryCast<::Aero::FrameworkElement>(node);
            if (element != nullptr && state.styles != nullptr) {
                if (state.interactivity != nullptr) {
                    state.interactivity->ClearStyleDataTriggersFor(*element);
                }
                (void)state.styles->DetachObject(*element);
            }
        }
        for (std::uint32_t index = reachable.Size(); index > 0U; --index) {
            Aero::Media::Visual* node = reachable[index - 1U];
            if (node == nullptr || state.metadata == nullptr ||
                !state.metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                continue;
            }
            auto& control = *static_cast<Controls::Control*>(node);
            if (state.visualStates != nullptr) {
                (void)::Aero::VisualStateManagerRuntime::Clear(
                    *state.visualStates, control);
            }
            if (state.templates != nullptr) {
                (void)state.templates->Clear(control);
            }
        }
        for (Aero::Media::Visual* node : declarationNodes) {
            if (node != nullptr) (void)state.values->DetachObject(*node);
        }
    }

Base::Result<void> ViewState::CreateUiEngines() noexcept {
        Base::Result<void> status = AllocateObject(
            *allocator, Base::MemoryTag::Ui, resources, *this);
        if (!status) return status.GetStatus();
        status = AllocateObject(*allocator, Base::MemoryTag::Ui, templates, *tree, *values,
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata),
            layout, renderer, metadata, bindings,
            &resources->dynamicResourceEnvironment);
        if (!status) return status.GetStatus();
        Base::Result<VisualStateManager*> createdStates =
            ::Aero::VisualStateManagerRuntime::Create(
                *values,
                *templates,
                *animations,
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata));
        if (!createdStates) return createdStates.GetStatus();
        visualStates = createdStates.Value();
        status = AllocateObject(*allocator, Base::MemoryTag::Ui, styles, *values,
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata));
        if (!status) return status.GetStatus();
        styles->SetTriggerActionHandler(
            &InteractivityEngine::ExecuteStyleTriggerActions, interactivity);
        if (tree != nullptr) {
            tree->SetLayout(layout);
            tree->SetBindings(bindings);
            tree->SetStyles(styles);
            tree->SetEvents(events);
            tree->SetInput(input);
            tree->SetAnimations(animations);
            tree->SetVisualStates(visualStates);
            tree->SetTemplates(templates);
            tree->SetTextLayout(text != nullptr ? text->Layout() : nullptr);
            tree->SetMeshResources(GetMeshResources());
            if (controlBehaviors != nullptr) {
                tree->SetControlBehaviors(controlBehaviors);
            }
            tree->AttachResourceEnvironment(resources->Environment());
            tree->SetNameScope(this, &ViewState::FindNameForElement);
        }
        status = AllocateObject(*allocator, Base::MemoryTag::Ui, overlays, *this);
        if (!status) return status.GetStatus();
        status = AllocateObject(*allocator, Base::MemoryTag::Ui, focus, *this);
        if (!status) return status.GetStatus();
        return {};
    }

Base::Result<void> ViewState::GeneratedItemSubtreeChanged(
        Aero::Media::Visual& root,
        Controls::ItemSubtreeChange change,
        void* context) noexcept {
        auto* runtime = static_cast<ViewState*>(context);
        if (runtime == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Generated item subtree runtime context is null");
        }
        if (change ==
            Controls::ItemSubtreeChange::Unmounting) {
            Base::Result<Aero::VisualHandle>
                rootHandle =
                    runtime->tree->GetHandle(root);
            if (rootHandle) {
                for (std::uint32_t index = 0U;
                     index <
                         runtime->
                             pendingGeneratedVisuals.
                                 Size();) {
                    if (runtime->
                            pendingGeneratedVisuals[
                                index].index !=
                            rootHandle.Value().index ||
                        runtime->
                            pendingGeneratedVisuals[
                                index].generation !=
                            rootHandle.Value().generation) {
                        ++index;
                        continue;
                    }
                    for (std::uint32_t move =
                             index + 1U;
                         move <
                             runtime->
                                 pendingGeneratedVisuals.
                                     Size();
                         ++move) {
                        runtime->
                            pendingGeneratedVisuals[
                                move - 1U] =
                            runtime->
                                pendingGeneratedVisuals[
                                    move];
                    }
                    runtime->
                        pendingGeneratedVisuals.
                            PopBack();
                    return {};
                }
            }
            DetachViewUi(
                *runtime,
                &root, {});
            return {};
        }
        if (runtime->deferGeneratedActivation ||
            (runtime->bindings != nullptr &&
             runtime->bindings->IsFlushing())) {
            Base::Result<Aero::VisualHandle>
                handle =
                    runtime->tree->GetHandle(root);
            if (!handle) return handle.GetStatus();
            return runtime->
                pendingGeneratedVisuals.
                    PushBack(handle.Value());
        }
        Base::Result<void> applied =
            ApplyViewUi(*runtime, root);
        if (!applied) {
            DetachViewUi(
                *runtime,
                &root, {});
            return applied.GetStatus();
        }
        Base::Result<void> attached =
            runtime->VisitAndAttach(root);
        if (!attached) {
            DetachViewUi(
                *runtime,
                &root, {});
            return attached.GetStatus();
        }
        Base::Result<std::uint32_t> rebound =
            runtime->bindings->Flush();
        if (!rebound) {
            DetachViewUi(*runtime, &root, {});
            return rebound.GetStatus();
        }
        Base::Result<std::uint32_t> started =
            runtime->storyboards->StartLoadedAnimations(&root);
        if (!started) {
            DetachViewUi(
                *runtime,
                &root, {});
            return started.GetStatus();
        }
        return {};
    }

Base::Result<void>
 ViewState::FlushGeneratedVisuals() noexcept {
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
                    tree->ResolveHandle(handle);
                if (subtreeRoot == nullptr) continue;
                Base::Result<void> applied =
                    ApplyViewUi(
                        *this,
                        *subtreeRoot);
                if (!applied) return applied.GetStatus();
                Base::Result<void> attached =
                    VisitAndAttach(
                        *subtreeRoot);
                if (!attached) return attached.GetStatus();
                Base::Result<std::uint32_t> reboundBeforeTriggers =
                    bindings->Flush();
                if (!reboundBeforeTriggers) {
                    return reboundBeforeTriggers.GetStatus();
                }
                Base::Result<std::uint32_t> started =
                    storyboards->StartLoadedAnimations(subtreeRoot);
                if (!started) return started.GetStatus();
            }
            Base::Result<std::uint32_t> rebound =
                bindings->Flush();
            if (!rebound) return rebound.GetStatus();
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Generated item visual activation exceeded the bounded activation waves");
    }

Base::Result<void> ViewState::AttachItemGenerator(
        Controls::ItemsControl& itemsControl) noexcept {
        if (AeroGuiInternal::
                HasAttachedGenerator(itemsControl)) {
            return {};
        }
        Controls::Panel* host = itemsControl.GetItemsHost();
        if (host == nullptr) return {};

        Base::Result<Controls::ItemContainerGenerator*> created =
            AeroGuiInternal::CreateItemContainerGenerator(
                *tree,
                *layout,
                *values,
                styles,
                renderer,
                templates,
                &ViewState::GeneratedItemSubtreeChanged,
                this);
        if (!created) return created.GetStatus();
        Controls::ItemContainerGenerator* generator = created.Value();
        Base::Result<void> attached;
        if (metadata->Types().IsDerivedFrom(
                host->RuntimeType(),
                Controls::VirtualizingStackPanel::StaticTypeId())) {
            attached = generator->AttachVirtualized(
                itemsControl,
                *static_cast<Controls::VirtualizingStackPanel*>(host));
        } else {
            attached = generator->Attach(itemsControl, *host);
        }
        if (!attached) {
            delete generator;
            return attached.GetStatus();
        }

        Base::Result<void> generatedUiApplied = ApplyViewUi(*this, *host);
        if (!generatedUiApplied) {
            DetachViewUi(*this, host, {});
            static_cast<void>(generator->Detach());
            delete generator;
            return generatedUiApplied.GetStatus();
        }
        Base::Result<void> tracked = itemGenerators.PushBack(generator);
        if (!tracked) {
            static_cast<void>(generator->Detach());
            delete generator;
            return tracked.GetStatus();
        }
        return {};
    }

Base::Result<void> ViewState::AttachPendingItemGenerators(
        Aero::Media::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed = stack.PushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            if (metadata->Types().IsDerivedFrom(
                    node->RuntimeType(),
                    Controls::ItemsControl::StaticTypeId())) {
                Base::Result<void> attached = AttachItemGenerator(
                    *static_cast<Controls::ItemsControl*>(node));
                if (!attached) return attached.GetStatus();
            }
            for (Aero::Media::Visual* child :
                 AeroGuiInternal::RenderChildren(*node)) {
                pushed = stack.PushBack(child);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

void ViewState::DestroyUiEngines() noexcept {
        if (tree != nullptr) {
            tree->SetLayout(nullptr);
            tree->SetBindings(nullptr);
            tree->SetStyles(nullptr);
            tree->SetEvents(nullptr);
            tree->SetInput(nullptr);
            tree->SetAnimations(nullptr);
            tree->SetVisualStates(nullptr);
            tree->SetTemplates(nullptr);
            tree->SetTextLayout(nullptr);
            tree->SetMeshResources(nullptr);
            tree->SetControlBehaviors(nullptr);
            tree->AttachResourceEnvironment({});
            tree->SetNameScope(nullptr, nullptr);
        }
        FreeObject(*allocator, Base::MemoryTag::Ui, overlays);
        FreeObject(*allocator, Base::MemoryTag::Ui, focus);
        FreeObject(*allocator, Base::MemoryTag::Ui, styles);
        delete visualStates;
        visualStates = nullptr;
        FreeObject(*allocator, Base::MemoryTag::Ui, templates);
        FreeObject(*allocator, Base::MemoryTag::Ui, resources);
    }

Base::Result<void> ViewState::VisitAndAttach(
        Aero::Media::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed =
            stack.PushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Meta::TypeId type = node->RuntimeType();
            if (controlBehaviors != nullptr) {
                Base::Result<void> attached = controlBehaviors->Attach(
                    *node, options.textInputMethodHost);
                if (!attached) return attached.GetStatus();
            }
            AttachTextLayout(
                *node,
                text != nullptr
                    ? text->Layout()
                    : nullptr);
            AttachPathResources(*node, GetMeshResources());
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ItemsControl::StaticTypeId())) {
                Base::Result<void> attached = AttachItemGenerator(
                    *static_cast<Controls::ItemsControl*>(node));
                if (!attached) return attached.GetStatus();
            }
            const auto children = AeroGuiInternal::RenderChildren(*node);
            for (std::uint32_t index = 0U;
                 index < children.Size(); ++index) {
                pushed = stack.PushBack(children[index]);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

void ViewState::ClearTextInputHosts(
        Aero::Media::Visual* node) noexcept {
        if (node == nullptr) return;
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            static_cast<void>(
                static_cast<Controls::TextBox*>(node)->
                    SetInputMethodHost(nullptr));
        }
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::PasswordBox::
                    StaticTypeId())) {
            static_cast<void>(
                static_cast<
                    Controls::PasswordBox*>(node)->
                    SetInputMethodHost(nullptr));
        }
        for (Aero::Media::Visual* child :
             AeroGuiInternal::RenderChildren(*node)) {
            ClearTextInputHosts(child);
        }
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

Base::Result<void> SynchronizeFrameResources(ViewState& state) noexcept {
    bool deviceGenerationChanged = false;
    GuiState& guiState = static_cast<GuiState&>(*state.gui);
    const bool fontProviderChanged =
        guiState.fontChangeGeneration != state.seenFontProviderChange;
    if (fontProviderChanged) {
        state.seenFontProviderChange = guiState.fontChangeGeneration;
    }
    if (state.images != nullptr &&
        guiState.textureChangeGeneration != state.seenTextureProviderChange) {
        if (guiState.textureChangesLost) {
            state.images->Invalidate({}, state.GetImageResources());
        } else {
            for (const XamlProviderChangeRecord& change :
                 guiState.textureChanges) {
                if (change.generation <= state.seenTextureProviderChange) {
                    continue;
                }
                state.images->Invalidate(change.uri, state.GetImageResources());
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
                Base::Result<void> invalidated = state.renderer->Invalidate(
                    *rootVisual, Aero::Render::RenderInvalidation::All);
                if (!invalidated) return invalidated.GetStatus();
            }
            state.VisitPaths(rootVisual, state.GetMeshResources(), true);
            if (state.tree != nullptr) {
                state.tree->SetMeshResources(state.GetMeshResources());
            }
            state.deviceGeneration = generation;
        }
    }
    if (state.text != nullptr) {
        Base::Result<bool> synchronized = state.text->SynchronizeBackend(
            *state.device,
            state.publicRenderer.Resources().text,
            deviceGenerationChanged || fontProviderChanged);
        if (!synchronized) return synchronized.GetStatus();
        if (synchronized.Value()) {
            state.VisitTextElements(
                state.RootVisual(), state.text->Layout(), true);
        }
    }
    if (state.images != nullptr) {
        Base::Result<bool> synchronized = state.images->Synchronize(
            state.RootVisual(),
            state.loadedDocument.canonicalUri,
            state.xamlRuntime->Providers(),
            guiState.textureProvider.Get(),
            state.GetImageResources(),
            deviceGenerationChanged);
        if (!synchronized) return synchronized.GetStatus();
        if (synchronized.Value()) {
            Aero::Media::Visual* rootVisual = state.RootVisual();
            if (rootVisual != nullptr) {
                Base::Result<void> invalidated = state.renderer->Invalidate(
                    *rootVisual, Aero::Render::RenderInvalidation::All);
                if (!invalidated) return invalidated.GetStatus();
            }
        }
    }
    return {};
}

} // namespace

Base::Result<std::uint32_t> ExecuteViewFrame(ViewState& state, View& view) noexcept {
    if (!state.initialized) {
        return ViewNotInitialized(
            "View must be initialized before running frames");
    }
    const bool skipAnimationPhase =
        state.animations == nullptr;

    bool skipUnsyncedVisualPhases = false;
    Base::Result<void> resources = SynchronizeFrameResources(state);
    if (!resources) {
        // Missing theme/image/XAML source files must not skip DataBind.
        // Layout/render can dereference unsynchronized image/font backends.
        state.ReportRendererFailure(resources.GetStatus());
        skipUnsyncedVisualPhases = true;
    }

    using Phase = ::Aero::Threading::DispatcherFramePhase;
    const Phase phases[] = {
        Phase::BeginFrame,
        Phase::Input,
        Phase::PropertyChanges,
        Phase::DataBind,
        Phase::Animation,
        Phase::Lifecycle,
        Phase::Layout,
        Phase::RenderCommit,
        Phase::EndFrame};

    ViewFrameResult result;
    for (Phase phase : phases) {
        if (phase == Phase::Animation && skipAnimationPhase) {
            continue;
        }
        if (skipUnsyncedVisualPhases &&
            (phase == Phase::Layout ||
             phase == Phase::RenderCommit)) {
            continue;
        }
        if (phase == Phase::Layout && state.HasAttachedRoot() &&
            state.tree != nullptr) {
            Base::Result<void> completed = state.tree->CompleteVisualEdges({
                state.loadedDocument.visualContent.mountEdges.Data(),
                state.loadedDocument.visualContent.mountEdges.Size()});
            if (!completed) return completed.GetStatus();
        }
        if (phase == Phase::RenderCommit) {
            state.RaiseFrameRendering(view);
            if (state.overlays != nullptr) {
                Base::Result<void> overlaySync =
                    state.overlays->SynchronizeOverlays();
                if (!overlaySync) return overlaySync.GetStatus();
            }
        }

        Base::Result<std::uint32_t> ran = state.dispatcher->RunFramePhase(phase);
        if (!ran) return ran.GetStatus();

        if (phase == Phase::Lifecycle) {
            Base::Result<std::uint32_t> focused = state.focus != nullptr
                ? state.focus->ProcessPendingFocus()
                : Base::Result<std::uint32_t>(0U);
            if (!focused) return focused.GetStatus();
            Base::Result<void> counted = AddFrameCallbacks(
                result, focused.Value(), "View callback count overflow");
            if (!counted) return counted.GetStatus();
        }
        if (phase == Phase::DataBind) {
            Base::Result<void> generatedVisualsFlushed =
                state.FlushGeneratedVisuals();
            if (!generatedVisualsFlushed) {
                return generatedVisualsFlushed.GetStatus();
            }
        }
        if (phase == Phase::Layout && !state.layout->LastFlushStatus().IsOk()) {
            return state.layout->LastFlushStatus();
        }
        if (phase == Phase::Layout &&
            state.layout->Diagnostics().arrangedCount != 0U) {
            if (state.interactivity != nullptr) {
                state.interactivity->NotifyLayoutUpdated();
            }
            Aero::Media::Visual* rootVisual = state.RootVisual();
            if (rootVisual != nullptr && state.renderer != nullptr) {
                Base::Result<void> invalidated = state.renderer->Invalidate(
                    *rootVisual, Aero::Render::RenderInvalidation::All);
                if (!invalidated) return invalidated.GetStatus();
            }
        }
        if (phase == Phase::Animation && state.animations != nullptr) {
            const Base::Status animationStatus =
                state.animations->LastTickStatus();
            if (!animationStatus.IsOk()) {
                state.ReportUpdateFailure(animationStatus);
            }
            if (state.animations->Diagnostics().appliedValueCount != 0U) {
                Aero::Media::Visual* rootVisual = state.RootVisual();
                if (rootVisual != nullptr && state.renderer != nullptr) {
                    Base::Result<void> invalidated = state.renderer->Invalidate(
                        *rootVisual, Aero::Render::RenderInvalidation::All);
                    if (!invalidated) return invalidated.GetStatus();
                }
            }
            if (state.storyboards != nullptr) {
                Base::Result<std::uint32_t> completed =
                    state.storyboards->ProcessStoryboardCompletions();
                if (!completed) return completed.GetStatus();
                Base::Result<void> counted = AddFrameCallbacks(
                    result,
                    completed.Value(),
                    "Frame callback count overflow");
                if (!counted) return counted.GetStatus();
            }
        }
        if (phase == Phase::Lifecycle && state.animations != nullptr) {
            Base::Result<std::uint32_t> initialValues =
                state.animations->ApplyPendingInitialValues();
            if (!initialValues) return initialValues.GetStatus();
            Base::Result<void> counted = AddFrameCallbacks(
                result,
                initialValues.Value(),
                "Initial animation callback count overflow");
            if (!counted) return counted.GetStatus();
        }

        Base::Result<void> counted = AddFrameCallbacks(
            result, ran.Value(), "Frame callback count overflow");
        if (!counted) return counted.GetStatus();

        if (phase == Phase::RenderCommit) {
            const Base::Status committed = state.renderer->LastCommitStatus();
            if (!committed.IsOk()) return committed;
            if (state.animations != nullptr) {
                state.animations->CommitPendingInitialValues();
            }
        }
    }

    if (state.text != nullptr) {
        Base::Result<std::uint32_t> collected = state.text->CollectGarbage();
        if (!collected) return collected.GetStatus();
    }
    result.frameNumber = ++state.frameNumber;
    const Aero::LayoutDiagnostics layoutDiagnostics =
        state.layout->Diagnostics();
    result.layout.passVersion = layoutDiagnostics.passVersion;
    result.layout.measuredCount = layoutDiagnostics.measuredCount;
    result.layout.arrangedCount = layoutDiagnostics.arrangedCount;
    result.layout.pendingMeasureCount = layoutDiagnostics.pendingMeasureCount;
    result.layout.pendingArrangeCount = layoutDiagnostics.pendingArrangeCount;
    const ::Aero::Render::RenderDiagnostics render =
        state.renderer->Diagnostics();
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


namespace {

[[maybe_unused]] Base::Result<std::uint32_t> AdvanceViewAnimations(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept {
    ViewState* state_ = &state;
    if (!state_->mounted || state_->animations == nullptr) {
        return ViewNotInitialized(
            "Animation timing requires a mounted View");
    }
    Base::Result<std::uint32_t> advanced =
        state_->animations->AdvanceBy(
        static_cast<Aero::Media::Animation::AnimationTime>(
            elapsedMilliseconds) * 1000U);
    if (!advanced) return advanced.GetStatus();
    Base::Result<std::uint32_t> completed =
        state_->storyboards->ProcessStoryboardCompletions();
    if (!completed) return completed.GetStatus();
    if (advanced.Value() >
        UINT32_MAX - completed.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Animation action count overflow");
    }
    return advanced.Value() + completed.Value();
}

} // namespace

Base::Result<std::uint32_t> AdvanceViewClocks(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept {
    ViewState* state_ = &state;
    if (!state_->mounted || state_->animations == nullptr) {
        return ViewNotInitialized(
            "View timing requires a mounted animation manager");
    }
    std::uint32_t actionCount = 0U;
    if (state_->controlBehaviors != nullptr) {
        Base::Result<std::uint32_t> controls =
            state_->controlBehaviors->AdvanceTime(
                elapsedMilliseconds);
        if (!controls) return controls.GetStatus();
        actionCount = controls.Value();
    }
    Base::Result<std::uint32_t> toolTips =
        state.overlays != nullptr
            ? state.overlays->AdvanceToolTipTime(
                elapsedMilliseconds)
            : Base::Result<std::uint32_t>(0U);
    if (!toolTips) return toolTips.GetStatus();
    if (actionCount > UINT32_MAX - toolTips.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Control timing action count overflow");
    }
    actionCount += toolTips.Value();
    if (state_->options.automaticAnimationClock) {
        return actionCount;
    }
    Base::Result<std::uint32_t> animations =
        state_->animations->AdvanceBy(
            static_cast<Aero::Media::Animation::AnimationTime>(
                elapsedMilliseconds) * 1000U);
    if (!animations) return animations.GetStatus();
    Base::Result<std::uint32_t> completed =
        state_->storyboards->ProcessStoryboardCompletions();
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


void ViewState::ClearElementEvents(
        Aero::Media::Visual* node) noexcept {
        if (node == nullptr) return;
        for (Aero::Media::Visual* child :
             AeroGuiInternal::RenderChildren(*node)) {
            ClearElementEvents(child);
        }
    }

void ViewState::BeginDestroyInteractions() noexcept {
        if (Aero::Media::Visual* rootVisual = RootVisual()) {
            if (interactivity != nullptr) {
                interactivity->DetachBehaviorsInSubtree(*rootVisual);
            }
        }
        if (overlays != nullptr) {
            overlays->CloseAllOverlays();
            overlays->ClearOverlays();
        }
        if (interactivity != nullptr) {
            interactivity->ClearAnimationEventSubscriptions();
        }
        if (overlays != nullptr) {
            if (overlays->activeToolTip) {
                static_cast<void>(
                    overlays->activeToolTip->SetIsOpen(false));
            }
            overlays->pendingToolTip.Reset();
            overlays->activeToolTip.Reset();
            overlays->toolTipTarget.Reset();
        }
        ClearTextInputHosts(RootVisual());
        ClearElementEvents(RootVisual());
        FreeObject(*allocator, Base::MemoryTag::Ui, controlBehaviors);
        if (tree != nullptr) tree->SetControlBehaviors(nullptr);
    }

void ViewState::FinishDestroyInteractions() noexcept {
        while (!itemGenerators.Empty()) {
            Controls::ItemContainerGenerator*
                generator = itemGenerators.Back();
            itemGenerators.PopBack();
            if (generator != nullptr) {
                static_cast<void>(
                    generator->Detach());
                delete generator;
                generator = nullptr;
            }
        }
        if (input != nullptr) {
            input->SetRoot(nullptr);
        }
    }

void ViewState::DestroyInteractions() noexcept {
        BeginDestroyInteractions();
        FinishDestroyInteractions();
    }

Base::Result<void> ViewState::CreateInteractions() noexcept {
        Aero::Media::Visual* rootVisual = RootVisual();
        if (rootVisual == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        if (input == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "InputRouter is unavailable");
        }
        input->SetRoot(rootVisual);
        Base::Result<void> status;

        if (options.attachControlInteractions || options.attachTextEditing) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, controlBehaviors,
                *allocator, *metadata, *tree, *events, *input,
                visualStates, options.clipboard,
                options.attachControlInteractions,
                options.attachTextEditing);
            if (!status) return status.GetStatus();
            status = controlBehaviors->Initialize();
            if (!status) return status.GetStatus();
            if (tree != nullptr) tree->SetControlBehaviors(controlBehaviors);
        }
        status = VisitAndAttach(*rootVisual);
        if (!status) {
            return status.GetStatus();
        }
        return {};
    }


} // namespace Aero
