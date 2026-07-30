#include "RuntimePresentationServices.hpp"

#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Style.hpp>
#include "../presentation/RuntimeManagers.hpp"
#include "../controls/RuntimeManagers.hpp"

namespace Aero::Detail {
namespace {

template<class T>
Base::Result<const T*> ResolvePresentationValue(
    Presentation::FrameworkElement& element,
    Core::DependencyPropertyHandle property,
    const Presentation::ResourceEnvironment& resources,
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
        Presentation::ResourceResolver::Lookup(
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

void RuntimePresentationServices::Configure(
    Base::IAllocator& allocator,
    Core::MetadataDomain& metadata,
    Core::EffectiveValueEngine& values,
    Presentation::BindingManager& bindings,
    Presentation::StyleManager& styles,
    Controls::TemplateManager& templates,
    Controls::VisualStateManager& visualStates,
    const Presentation::ResourceEnvironment& resources) noexcept {
    allocator_ = &allocator;
    metadata_ = &metadata;
    values_ = &values;
    bindings_ = &bindings;
    styles_ = &styles;
    templates_ = &templates;
    visualStates_ = &visualStates;
    resources_ = resources;
}

void RuntimePresentationServices::Reset() noexcept {
    allocator_ = nullptr;
    metadata_ = nullptr;
    values_ = nullptr;
    bindings_ = nullptr;
    styles_ = nullptr;
    templates_ = nullptr;
    visualStates_ = nullptr;
    resources_ = {};
}

Base::Result<void> RuntimePresentationServices::Apply(
    Presentation::Visual& root) noexcept {
    if (!IsConfigured() || metadata_ == nullptr || values_ == nullptr ||
        bindings_ == nullptr ||
        styles_ == nullptr || templates_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Runtime presentation services are unavailable");
    }

    Base::Vector<Presentation::Visual*> stack(allocator_);
    Base::Result<void> pushed = stack.TryPushBack(&root);
    if (!pushed) return pushed.GetStatus();
    while (!stack.Empty()) {
        Presentation::Visual* node = stack.Back();
        stack.PopBack();
        if (node == nullptr) continue;

        auto* dependencyObject =
            static_cast<Core::DependencyObject*>(node);
        Base::Result<std::uint32_t> activated =
            bindings_->ActivateDeferred(
                *dependencyObject);
        if (!activated) return activated.GetStatus();

        Presentation::FrameworkElement* element =
            node->AsFrameworkElement();
        if (element != nullptr) {
            Base::Result<const Presentation::Style*> resolved =
                ResolvePresentationValue<Presentation::Style>(
                    *element,
                    Presentation::FrameworkElement::StyleProperty,
                    resources_,
                    "FrameworkElement Style value is not a Style");
            if (!resolved) return resolved.GetStatus();
            const Presentation::Style* style = resolved.Value();
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
            control.AttachTemplateManager(*templates_);
            Base::Result<const Controls::ControlTemplate*> resolved =
                ResolvePresentationValue<Controls::ControlTemplate>(
                    control,
                    Controls::Control::TemplateProperty,
                    resources_,
                    "Control Template value is not a ControlTemplate");
            if (!resolved) return resolved.GetStatus();
            const Controls::ControlTemplate* controlTemplate =
                resolved.Value();
            if (controlTemplate != nullptr) {
                const Controls::TemplateHandle existing =
                    templates_->AppliedHandle(control);
                if (!existing.IsValid() ||
                    templates_->AppliedTemplate(existing) != controlTemplate) {
                    Base::Result<Controls::TemplateHandle> applied =
                        templates_->Apply(control, *controlTemplate);
                    if (!applied) return applied.GetStatus();
                }
            }
        }

        for (Presentation::Visual* child : node->VisualChildren()) {
            pushed = stack.TryPushBack(child);
            if (!pushed) return pushed.GetStatus();
        }
    }
    Base::Result<std::uint32_t> appliedValues = values_->Flush();
    return appliedValues
        ? Base::Result<void>()
        : Base::Result<void>(appliedValues.GetStatus());
}

void RuntimePresentationServices::Detach(
    Presentation::Visual* root,
    Base::Span<Presentation::Visual* const> declarationNodes) noexcept {
    if (!IsConfigured() || values_ == nullptr) return;

    Base::Vector<Presentation::Visual*> reachable(allocator_);
    if (root != nullptr) {
        (void)reachable.TryPushBack(root);
        for (std::uint32_t index = 0U; index < reachable.Size(); ++index) {
            Presentation::Visual* node = reachable[index];
            if (node == nullptr) continue;
            for (Presentation::Visual* child : node->VisualChildren()) {
                if (child != nullptr) (void)reachable.TryPushBack(child);
            }
        }
    }

    for (Presentation::Visual* node : reachable) {
        if (node == nullptr) continue;
        Presentation::FrameworkElement* element = node->AsFrameworkElement();
        if (bindings_ != nullptr) {
            (void)bindings_->DetachObject(*node);
        }
        if (element != nullptr && styles_ != nullptr) {
            (void)styles_->DetachObject(*element);
        }
    }
    for (std::uint32_t index = reachable.Size(); index > 0U; --index) {
        Presentation::Visual* node = reachable[index - 1U];
        if (node == nullptr || metadata_ == nullptr ||
            !metadata_->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::Control::StaticTypeId())) {
            continue;
        }
        auto& control = *static_cast<Controls::Control*>(node);
        if (visualStates_ != nullptr) (void)visualStates_->Clear(control);
        if (templates_ != nullptr) (void)templates_->Clear(control);
    }
    for (Presentation::Visual* node : declarationNodes) {
        if (node != nullptr) (void)values_->DetachObject(*node);
    }
}

} // namespace Aero::Detail
