#include "RuntimeUiServices.hpp"
#include "../controls/TemplateRuntime.hpp"

#include <Aero/Controls/Base.hpp>
#include <Aero/Styling.hpp>
#include <Aero/FrameworkElement.hpp>
#include "../ui/RuntimeManagers.hpp"
#include "../controls/RuntimeManagers.hpp"
#include "../input/InputService.hpp"

namespace Aero::Detail {

void UiRuntimeAccess::SetEventRouter(Aero::UIElement& element, EventRouter* router) noexcept {
    element.eventRouter_ = router;
}

void UiRuntimeAccess::SetCommandRouter(Aero::UIElement& element, InputService* service) noexcept {
    element.commandRouter_ = service;
}

namespace {

template<class T>
Base::Result<const T*> ResolveUiValue(
    Aero::FrameworkElement& element,
    Core::DependencyPropertyHandle property,
    const Aero::ResourceEnvironment& resources,
    const char* incompatibleMessage) noexcept {
    Base::Result<Core::Value> explicitValue = element.GetValue(property);
    if (!explicitValue) return explicitValue.GetStatus();
    if (explicitValue.Value().Kind() == Core::ValueKind::Object &&
        !explicitValue.Value().IsNullObject() &&
        explicitValue.Value().AsObject()) {
        Base::Object* object = explicitValue.Value().AsObject().Get();
        if (object->RuntimeType() != T::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                incompatibleMessage);
        }
        return static_cast<const T*>(object);
    }

    Base::Result<Core::Value> implicit =
        Aero::ResourceResolver::Lookup(
            &element,
            element.RuntimeType(),
            nullptr,
            resources);
    if (!implicit) {
        return implicit.GetStatus().code == Base::ErrorCode::NotFound
            ? Base::Result<const T*>(static_cast<const T*>(nullptr))
            : Base::Result<const T*>(implicit.GetStatus());
    }
    if (implicit.Value().Kind() != Core::ValueKind::Object ||
        implicit.Value().IsNullObject() ||
        !implicit.Value().AsObject() ||
        implicit.Value().AsObject()->RuntimeType() != T::StaticTypeId()) {
        return static_cast<const T*>(nullptr);
    }
    return static_cast<const T*>(implicit.Value().AsObject().Get());
}

} // namespace

void RuntimeUiServices::Configure(
    Base::IAllocator& allocator,
    Core::MetadataDomain& metadata,
    Core::EffectiveValueEngine& values,
    Aero::Detail::BindingManager& bindings,
    Aero::Detail::EventRouter& events,
    Aero::Detail::InputService& input,
    Aero::Detail::StyleManager& styles,
    Controls::TemplateManager& templates,
    Controls::VisualStateManager& visualStates,
    const Aero::ResourceEnvironment& resources) noexcept {
    allocator_ = &allocator;
    metadata_ = &metadata;
    values_ = &values;
    bindings_ = &bindings;
    events_ = &events;
    input_ = &input;
    styles_ = &styles;
    templates_ = &templates;
    visualStates_ = &visualStates;
    resources_ = resources;
}

void RuntimeUiServices::Reset() noexcept {
    allocator_ = nullptr;
    metadata_ = nullptr;
    values_ = nullptr;
    bindings_ = nullptr;
    events_ = nullptr;
    input_ = nullptr;
    styles_ = nullptr;
    templates_ = nullptr;
    visualStates_ = nullptr;
    resources_ = {};
}

Base::Result<void> RuntimeUiServices::Apply(
    Aero::Visual& root) noexcept {
    if (!IsConfigured() || metadata_ == nullptr || values_ == nullptr ||
        bindings_ == nullptr || events_ == nullptr || input_ == nullptr ||
        styles_ == nullptr || templates_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Runtime UI services are unavailable");
    }

    Base::Vector<Aero::Visual*> stack(allocator_);
    Base::Result<void> pushed = stack.TryPushBack(&root);
    if (!pushed) return pushed.GetStatus();
    while (!stack.Empty()) {
        Aero::Visual* node = stack.Back();
        stack.PopBack();
        if (node == nullptr) continue;

        if (Aero::UIElement* element = node->AsUIElement()) {
            UiRuntimeAccess::SetEventRouter(*element, events_);
            UiRuntimeAccess::SetCommandRouter(*element, input_);
        }

        auto* dependencyObject =
            static_cast<Core::DependencyObject*>(node);
        Base::Result<std::uint32_t> activated =
            bindings_->ActivateDeferred(
                *dependencyObject);
        if (!activated) return activated.GetStatus();

        Aero::FrameworkElement* element =
            node->AsFrameworkElement();
        if (element != nullptr) {
            Base::Result<const Aero::Style*> resolved =
                ResolveUiValue<Aero::Style>(
                    *element,
                    Aero::FrameworkElement::StyleProperty,
                    resources_,
                    "FrameworkElement Style value is not a Style");
            if (!resolved) return resolved.GetStatus();
            const Aero::Style* style = resolved.Value();
            if (style != nullptr) {
                if (!style->IsSealed()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Implicit Style is not sealed");
                }
                if (styles_->AppliedStyle(*element) != style) {
                    Base::Result<void> applied =
                        styles_->Apply(*element, *style);
                    if (!applied) return applied.GetStatus();
                }
            }
        }

        Base::Result<std::uint32_t> styleValues = values_->Flush();
        if (!styleValues) return styleValues.GetStatus();

        if (metadata_->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::Control::StaticTypeId())) {
            auto& control = *static_cast<Controls::Control*>(node);
            control.AttachTemplateRuntime(templates_);
            Base::Result<const Controls::ControlTemplate*> resolved =
                ResolveUiValue<Controls::ControlTemplate>(
                    control,
                    Controls::Control::TemplateProperty,
                    resources_,
                    "Control Template value is not a ControlTemplate");
            if (!resolved) return resolved.GetStatus();
            const Controls::ControlTemplate* controlTemplate =
                resolved.Value();
            if (controlTemplate != nullptr) {
                const Controls::Detail::TemplateHandle existing =
                    templates_->AppliedHandle(control);
                if (!existing.IsValid() ||
                    templates_->AppliedTemplate(existing) != controlTemplate) {
                    Base::Result<Controls::Detail::TemplateHandle> applied =
                        templates_->Apply(control, *controlTemplate);
                    if (!applied) return applied.GetStatus();
                }
            }
        }

        for (Aero::Visual* child : Aero::Detail::VisualAccess::VisualChildren(*node)) {
            pushed = stack.TryPushBack(child);
            if (!pushed) return pushed.GetStatus();
        }
    }
    Base::Result<std::uint32_t> appliedValues = values_->Flush();
    return appliedValues
        ? Base::Result<void>()
        : Base::Result<void>(appliedValues.GetStatus());
}

void RuntimeUiServices::Detach(
    Aero::Visual* root,
    Base::Span<Aero::Visual* const> declarationNodes) noexcept {
    if (!IsConfigured() || values_ == nullptr) return;

    Base::Vector<Aero::Visual*> reachable(allocator_);
    if (root != nullptr) {
        (void)reachable.TryPushBack(root);
        for (std::uint32_t index = 0U; index < reachable.Size(); ++index) {
            Aero::Visual* node = reachable[index];
            if (node == nullptr) continue;
            for (Aero::Visual* child : Aero::Detail::VisualAccess::VisualChildren(*node)) {
                if (child != nullptr) (void)reachable.TryPushBack(child);
            }
        }
    }

    for (Aero::Visual* node : reachable) {
        if (node == nullptr) continue;
        if (Aero::UIElement* ui = node->AsUIElement()) {
            UiRuntimeAccess::SetEventRouter(*ui, nullptr);
            UiRuntimeAccess::SetCommandRouter(*ui, nullptr);
        }
        Aero::FrameworkElement* element = node->AsFrameworkElement();
        if (bindings_ != nullptr) {
            (void)bindings_->DetachObject(*node);
        }
        if (element != nullptr && styles_ != nullptr) {
            (void)styles_->DetachObject(*element);
        }
    }
    for (std::uint32_t index = reachable.Size(); index > 0U; --index) {
        Aero::Visual* node = reachable[index - 1U];
        if (node == nullptr || metadata_ == nullptr ||
            !metadata_->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::Control::StaticTypeId())) {
            continue;
        }
        auto& control = *static_cast<Controls::Control*>(node);
        if (visualStates_ != nullptr) (void)Controls::Detail::VisualStateManagerAccess::Clear(*visualStates_, control);
        if (templates_ != nullptr) (void)templates_->Clear(control);
    }
    for (Aero::Visual* node : declarationNodes) {
        if (node != nullptr) (void)values_->DetachObject(*node);
    }
}

} // namespace Aero::Detail
