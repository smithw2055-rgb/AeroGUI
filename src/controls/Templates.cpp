#include <Aero/Controls/Templates.hpp>

#include <utility>

namespace Aero::Controls {

namespace {

Base::Status InvalidTemplate(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

bool IsTargetCompatible(
    const TypeRegistry& types,
    TypeId derived,
    TypeId expectedBase) noexcept {
    return derived == expectedBase ||
        types.IsDerivedFrom(derived, expectedBase);
}

} // namespace

Base::Result<void> TemplateBuildContext::SetRoot(
    Base::Ref<Base::Object> owner,
    Visual& root) noexcept {
    if (tree_ == nullptr || parent_ == nullptr ||
        rootVisual_ != nullptr || !owner ||
        owner.Get() != &root ||
        root.AsUIElement() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template root registration is invalid");
    }
    Base::Result<void> logical =
        tree_->AttachLogical(*parent_, root);
    if (!logical) return logical.GetStatus();
    Base::Result<void> visual =
        tree_->AttachVisual(*parent_, root);
    if (!visual) {
        (void)tree_->DetachLogical(*parent_, root);
        return visual.GetStatus();
    }
    Base::Result<void> selected =
        parent_->SetTemplateChild(root.AsUIElement());
    if (!selected) {
        (void)tree_->DetachVisual(*parent_, root);
        (void)tree_->DetachLogical(*parent_, root);
        return selected.GetStatus();
    }
    if (root.AsFrameworkElement() != nullptr) {
        Base::Result<void> templated =
            root.AsFrameworkElement()->SetTemplatedParent(parent_);
        if (!templated) {
            (void)parent_->SetTemplateChild(nullptr);
            (void)tree_->DetachVisual(*parent_, root);
            (void)tree_->DetachLogical(*parent_, root);
            return templated.GetStatus();
        }
    }
    rootVisual_ = &root;
    rootElement_ = root.AsUIElement();
    Base::Result<void> added =
        AddOwnedPart({}, std::move(owner), root);
    if (!added) {
        Rollback();
        return added.GetStatus();
    }
    return {};
}

Base::Result<void> TemplateBuildContext::AddPart(
    Base::StringView name,
    Visual& parent,
    Base::Ref<Base::Object> owner,
    Visual& part) noexcept {
    if (tree_ == nullptr || parent_ == nullptr ||
        rootVisual_ == nullptr || name.Empty() ||
        !owner || owner.Get() != &part ||
        FindObject(name) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template part registration is invalid");
    }
    Base::Result<void> logical =
        tree_->AttachLogical(parent, part);
    if (!logical) return logical.GetStatus();
    Base::Result<void> visual =
        tree_->AttachVisual(parent, part);
    if (!visual) {
        (void)tree_->DetachLogical(parent, part);
        return visual.GetStatus();
    }
    if (part.AsFrameworkElement() != nullptr) {
        Base::Result<void> templated =
            part.AsFrameworkElement()->SetTemplatedParent(parent_);
        if (!templated) {
            (void)tree_->DetachVisual(parent, part);
            (void)tree_->DetachLogical(parent, part);
            return templated.GetStatus();
        }
    }
    Base::Result<void> added =
        AddOwnedPart(name, std::move(owner), part);
    if (!added) {
        if (part.AsFrameworkElement() != nullptr) {
            (void)part.AsFrameworkElement()->SetTemplatedParent(
                nullptr);
        }
        (void)tree_->DetachVisual(parent, part);
        (void)tree_->DetachLogical(parent, part);
        return added.GetStatus();
    }
    return {};
}

DependencyObject* TemplateBuildContext::FindObject(
    Base::StringView name) const noexcept {
    for (const TemplatePart& part : parts_) {
        if (part.name.View() == name) return part.object;
    }
    return nullptr;
}

Base::Result<void> TemplateBuildContext::AddOwnedPart(
    Base::StringView name,
    Base::Ref<Base::Object> owner,
    Visual& visual) noexcept {
    TemplatePart part;
    Base::Result<void> assigned = part.name.TryAssign(name);
    if (!assigned) return assigned.GetStatus();
    part.owner = std::move(owner);
    part.visual = &visual;
    part.object = &visual;
    part.frameworkElement = visual.AsFrameworkElement();
    return parts_.TryPushBack(std::move(part));
}

void TemplateBuildContext::Rollback() noexcept {
    for (TemplatePart& part : parts_) {
        if (part.frameworkElement != nullptr) {
            (void)part.frameworkElement->SetTemplatedParent(nullptr);
        }
    }
    if (parent_ != nullptr) {
        (void)parent_->SetTemplateChild(nullptr);
    }
    if (tree_ != nullptr && rootVisual_ != nullptr) {
        (void)tree_->DetachNode(*rootVisual_);
    }
    parts_.Clear();
    rootVisual_ = nullptr;
    rootElement_ = nullptr;
}

Base::Result<void> FrameworkTemplate::TryAddTemplateBinding(
    Base::StringView targetName,
    DependencyPropertyHandle sourceProperty,
    DependencyPropertyHandle targetProperty) noexcept {
    if (sealed_) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (!sourceProperty.IsValid() ||
        !targetProperty.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TemplateBinding requires source and target properties");
    }
    TemplateBindingPlan binding;
    Base::Result<void> assigned =
        binding.targetName.TryAssign(targetName);
    if (!assigned) return assigned.GetStatus();
    binding.sourceProperty = sourceProperty;
    binding.targetProperty = targetProperty;
    return bindings_.TryPushBack(std::move(binding));
}

Base::Result<void> FrameworkTemplate::TryAddPropertyTrigger(
    TemplatePropertyTrigger trigger) noexcept {
    if (sealed_) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (!trigger.property.IsValid() ||
        trigger.value.IsUnset() ||
        trigger.setters.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template property trigger is incomplete");
    }
    return triggers_.TryPushBack(std::move(trigger));
}

Base::Result<void> FrameworkTemplate::TryAddVisualStateGroup(
    VisualStateGroup group) noexcept {
    if (sealed_) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (group.name.Empty() || group.states.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Visual state group requires a name and at least one state");
    }
    for (const VisualStateGroup& existing : visualStateGroups_) {
        if (existing.name.View() == group.name.View()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Visual state group name is duplicated");
        }
    }
    for (std::uint32_t stateIndex = 0U;
        stateIndex < group.states.Size(); ++stateIndex) {
        const VisualState& state = group.states[stateIndex];
        if (state.name.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Visual state requires a name");
        }
        for (std::uint32_t previous = 0U;
            previous < stateIndex; ++previous) {
            if (group.states[previous].name.View() ==
                state.name.View()) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Visual state name is duplicated in its group");
            }
        }
        for (std::uint32_t setterIndex = 0U;
            setterIndex < state.setters.Size(); ++setterIndex) {
            const VisualStateSetter& setter =
                state.setters[setterIndex];
            if (!setter.property.IsValid() ||
                setter.value.IsUnset()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Visual state setter is incomplete");
            }
            for (std::uint32_t previous = 0U;
                previous < setterIndex; ++previous) {
                const VisualStateSetter& candidate =
                    state.setters[previous];
                if (candidate.property == setter.property &&
                    candidate.targetName.View() ==
                        setter.targetName.View()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        "Visual state setter target is duplicated");
                }
            }
        }
    }
    return visualStateGroups_.TryPushBack(std::move(group));
}

Base::Result<void> FrameworkTemplate::Seal(
    const DependencyPropertyRegistry& properties) noexcept {
    if (sealed_) return {};
    if (!properties.IsFrozen() ||
        targetType_ == InvalidTypeId ||
        factory_ == nullptr ||
        properties.Types().FindType(targetType_) == nullptr) {
        return InvalidTemplate(
            "FrameworkTemplate requires a factory and registered target type");
    }
    for (const TemplateBindingPlan& binding : bindings_) {
        const DependencyProperty* source =
            properties.Find(binding.sourceProperty);
        const DependencyProperty* target =
            properties.Find(binding.targetProperty);
        if (source == nullptr || target == nullptr ||
            source->MetadataFor(targetType_) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplateBinding property was not found");
        }
    }
    for (const TemplatePropertyTrigger& trigger : triggers_) {
        const DependencyProperty* condition =
            properties.Find(trigger.property);
        if (condition == nullptr ||
            condition->MetadataFor(targetType_) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Template trigger condition does not apply to TargetType");
        }
        Base::Result<void> valid = properties.ValidateValueFor(
            trigger.property, targetType_, trigger.value);
        if (!valid) return valid.GetStatus();
        for (const TemplateTriggerSetter& setter :
             trigger.setters) {
            if (!setter.property.IsValid() ||
                setter.value.IsUnset() ||
                properties.Find(setter.property) == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Template trigger setter is invalid");
            }
        }
    }
    for (std::uint32_t groupIndex = 0U;
        groupIndex < visualStateGroups_.Size(); ++groupIndex) {
        const VisualStateGroup& group =
            visualStateGroups_[groupIndex];
        for (const VisualState& state : group.states) {
            for (const VisualStateSetter& setter : state.setters) {
                const DependencyProperty* property =
                    properties.Find(setter.property);
                if (property == nullptr || property->IsReadOnly()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidArgument,
                        "Visual state setter property is invalid");
                }
                Base::Result<void> valid =
                    properties.ValidateValueFor(
                        setter.property,
                        property->RegisteredOwnerType(),
                        setter.value);
                if (!valid) return valid.GetStatus();
                for (std::uint32_t otherIndex = 0U;
                    otherIndex < groupIndex; ++otherIndex) {
                    for (const VisualState& otherState :
                        visualStateGroups_[otherIndex].states) {
                        for (const VisualStateSetter& otherSetter :
                            otherState.setters) {
                            if (otherSetter.property ==
                                    setter.property &&
                                otherSetter.targetName.View() ==
                                    setter.targetName.View()) {
                                return Base::Status::Failure(
                                    Base::ErrorCode::InvalidState,
                                    "Visual state groups cannot target "
                                    "the same property");
                            }
                        }
                    }
                }
            }
        }
    }
    sealed_ = true;
    return {};
}

TemplateManager::~TemplateManager() noexcept {
    while (!instances_.Empty()) {
        if (!ClearAt(instances_.Size() - 1U)) {
            break;
        }
    }
}

Base::Result<TemplateHandle> TemplateManager::Apply(
    Control& control,
    const ControlTemplate& plan) noexcept {
    if (tree_ == nullptr || values_ == nullptr ||
        properties_ == nullptr || !plan.IsSealed() ||
        control.OwningTree() != tree_ ||
        !IsTargetCompatible(
            properties_->Types(),
            control.RuntimeType(),
            plan.TargetType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ControlTemplate cannot be applied to this control");
    }
    const std::uint32_t existing = FindInstance(control);
    if (existing != UINT32_MAX) {
        Base::Result<void> cleared = ClearAt(existing);
        if (!cleared) return cleared.GetStatus();
    }
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Template handle sequence is exhausted");
    }

    TemplateBuildContext context(*tree_, control);
    Base::Result<void> built =
        plan.Factory()(context, plan.FactoryContext());
    if (!built || context.RootVisual() == nullptr) {
        context.Rollback();
        return built
            ? InvalidTemplate(
                "ControlTemplate factory did not set a root")
            : built.GetStatus();
    }

    Instance instance;
    instance.handle.value = nextHandle_++;
    instance.parent = &control;
    instance.plan = &plan;
    instance.rootVisual = context.rootVisual_;
    instance.rootElement = context.rootElement_;
    instance.parts = std::move(context.parts_);
    context.rootVisual_ = nullptr;
    context.rootElement_ = nullptr;
    Base::Result<void> tracked =
        instances_.TryPushBack(std::move(instance));
    if (!tracked) {
        --nextHandle_;
        context.parts_ = std::move(instance.parts);
        context.rootVisual_ = instance.rootVisual;
        context.rootElement_ = instance.rootElement;
        context.Rollback();
        return tracked.GetStatus();
    }
    Instance& stored = instances_.Back();
    Base::Result<void> subscribed = Subscribe(stored);
    if (!subscribed) {
        const Base::Status status = subscribed.GetStatus();
        (void)ClearAt(instances_.Size() - 1U);
        return status;
    }
    Base::Result<void> bindings = ApplyBindings(stored);
    if (!bindings) {
        const Base::Status status = bindings.GetStatus();
        (void)ClearAt(instances_.Size() - 1U);
        return status;
    }
    Base::Result<void> triggers = EvaluateTriggers(stored);
    if (!triggers) {
        const Base::Status status = triggers.GetStatus();
        (void)ClearAt(instances_.Size() - 1U);
        return status;
    }
    return stored.handle;
}

Base::Result<bool> TemplateManager::Clear(
    TemplateHandle handle) noexcept {
    const std::uint32_t index = FindInstance(handle);
    if (index == UINT32_MAX) return false;
    Base::Result<void> cleared = ClearAt(index);
    if (!cleared) return cleared.GetStatus();
    return true;
}

Base::Result<bool> TemplateManager::Clear(
    Control& control) noexcept {
    const std::uint32_t index = FindInstance(control);
    if (index == UINT32_MAX) return false;
    Base::Result<void> cleared = ClearAt(index);
    if (!cleared) return cleared.GetStatus();
    return true;
}

DependencyObject* TemplateManager::FindName(
    TemplateHandle handle,
    Base::StringView name) const noexcept {
    const std::uint32_t index = FindInstance(handle);
    return index != UINT32_MAX
        ? FindTarget(instances_[index], name)
        : nullptr;
}

TemplateHandle TemplateManager::AppliedHandle(
    const Control& control) const noexcept {
    const std::uint32_t index = FindInstance(control);
    return index != UINT32_MAX
        ? instances_[index].handle : TemplateHandle{};
}

const ControlTemplate* TemplateManager::AppliedTemplate(
    TemplateHandle handle) const noexcept {
    const std::uint32_t index = FindInstance(handle);
    return index != UINT32_MAX
        ? instances_[index].plan : nullptr;
}

std::uint32_t TemplateManager::FindInstance(
    TemplateHandle handle) const noexcept {
    for (std::uint32_t index = 0U;
         index < instances_.Size();
         ++index) {
        if (instances_[index].handle.value == handle.value) {
            return index;
        }
    }
    return UINT32_MAX;
}

std::uint32_t TemplateManager::FindInstance(
    const Control& control) const noexcept {
    for (std::uint32_t index = 0U;
         index < instances_.Size();
         ++index) {
        if (instances_[index].parent == &control) return index;
    }
    return UINT32_MAX;
}

DependencyObject* TemplateManager::FindTarget(
    const Instance& instance,
    Base::StringView name) const noexcept {
    for (const TemplatePart& part : instance.parts) {
        if (part.name.View() == name) return part.object;
    }
    return nullptr;
}

Base::Result<void> TemplateManager::Subscribe(
    Instance& instance) noexcept {
    for (std::uint32_t index = 0U;
         index < instance.plan->Bindings().Size();
         ++index) {
        const DependencyPropertyHandle property =
            instance.plan->Bindings()[index].sourceProperty;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                instance.plan->Bindings()[previous].sourceProperty !=
                    property;
        }
        if (first) {
            Base::Result<void> subscribed =
                instance.parent->TryAddValueChangedHandler(
                    property, propertyChangedHandler_);
            if (!subscribed) return subscribed.GetStatus();
        }
    }
    for (std::uint32_t index = 0U;
         index < instance.plan->Triggers().Size();
         ++index) {
        const DependencyPropertyHandle property =
            instance.plan->Triggers()[index].property;
        bool first = true;
        for (const TemplateBindingPlan& binding :
             instance.plan->Bindings()) {
            first = first &&
                binding.sourceProperty != property;
        }
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                instance.plan->Triggers()[previous].property !=
                    property;
        }
        if (first) {
            Base::Result<void> subscribed =
                instance.parent->TryAddValueChangedHandler(
                    property, propertyChangedHandler_);
            if (!subscribed) return subscribed.GetStatus();
        }
    }
    return {};
}

void TemplateManager::Unsubscribe(
    Instance& instance) noexcept {
    for (std::uint32_t index = 0U;
         index < instance.plan->Bindings().Size();
         ++index) {
        const DependencyPropertyHandle property =
            instance.plan->Bindings()[index].sourceProperty;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                instance.plan->Bindings()[previous].sourceProperty !=
                    property;
        }
        if (first) {
            (void)instance.parent->RemoveValueChangedHandler(
                property, propertyChangedHandler_);
        }
    }
    for (std::uint32_t index = 0U;
         index < instance.plan->Triggers().Size();
         ++index) {
        const DependencyPropertyHandle property =
            instance.plan->Triggers()[index].property;
        bool first = true;
        for (const TemplateBindingPlan& binding :
             instance.plan->Bindings()) {
            first = first &&
                binding.sourceProperty != property;
        }
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                instance.plan->Triggers()[previous].property !=
                    property;
        }
        if (first) {
            (void)instance.parent->RemoveValueChangedHandler(
                property, propertyChangedHandler_);
        }
    }
}

Base::Result<void> TemplateManager::ApplyBindings(
    Instance& instance,
    DependencyPropertyHandle changed) noexcept {
    for (const TemplateBindingPlan& binding :
         instance.plan->Bindings()) {
        if (changed.IsValid() &&
            binding.sourceProperty != changed) {
            continue;
        }
        DependencyObject* target =
            FindTarget(instance, binding.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplateBinding target name was not found");
        }
        Base::Result<PropertyValue> value =
            instance.parent->GetValue(binding.sourceProperty);
        if (!value) return value.GetStatus();
        Base::Result<void> applied =
            values_->SetTemplateValue(
                *target, binding.targetProperty, value.Value());
        if (!applied) return applied.GetStatus();
    }
    return {};
}

Base::Result<void> TemplateManager::EvaluateTriggers(
    Instance& instance) noexcept {
    for (const TemplatePropertyTrigger& trigger :
         instance.plan->Triggers()) {
        for (const TemplateTriggerSetter& setter :
             trigger.setters) {
            DependencyObject* target =
                FindTarget(instance, setter.targetName.View());
            if (target == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template trigger target name was not found");
            }
            Base::Result<void> cleared =
                values_->ClearTriggerValue(
                    *target, setter.property);
            if (!cleared) return cleared.GetStatus();
        }
    }
    for (const TemplatePropertyTrigger& trigger :
         instance.plan->Triggers()) {
        Base::Result<PropertyValue> current =
            instance.parent->GetValue(trigger.property);
        if (!current) return current.GetStatus();
        if (current.Value() != trigger.value) continue;
        for (const TemplateTriggerSetter& setter :
             trigger.setters) {
            DependencyObject* target =
                FindTarget(instance, setter.targetName.View());
            Base::Result<void> applied =
                values_->SetTriggerValue(
                    *target, setter.property, setter.value);
            if (!applied) return applied.GetStatus();
        }
    }
    return {};
}

Base::Result<void> TemplateManager::ClearProviders(
    Instance& instance) noexcept {
    for (const TemplateBindingPlan& binding :
         instance.plan->Bindings()) {
        DependencyObject* target =
            FindTarget(instance, binding.targetName.View());
        if (target != nullptr) {
            Base::Result<void> cleared =
                values_->ClearTemplateValue(
                    *target, binding.targetProperty);
            if (!cleared) return cleared.GetStatus();
        }
    }
    for (const TemplatePropertyTrigger& trigger :
         instance.plan->Triggers()) {
        for (const TemplateTriggerSetter& setter :
             trigger.setters) {
            DependencyObject* target =
                FindTarget(instance, setter.targetName.View());
            if (target != nullptr) {
                Base::Result<void> cleared =
                    values_->ClearTriggerValue(
                        *target, setter.property);
                if (!cleared) return cleared.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void> TemplateManager::ClearAt(
    std::uint32_t index) noexcept {
    Instance& instance = instances_[index];
    Unsubscribe(instance);
    Base::Result<void> providers =
        ClearProviders(instance);
    if (!providers) return providers.GetStatus();
    for (TemplatePart& part : instance.parts) {
        if (part.frameworkElement != nullptr) {
            Base::Result<void> cleared =
                part.frameworkElement->SetTemplatedParent(nullptr);
            if (!cleared) return cleared.GetStatus();
        }
    }
    Base::Result<void> child =
        instance.parent->SetTemplateChild(nullptr);
    if (!child) return child.GetStatus();
    Base::Result<void> detached =
        tree_->DetachNode(*instance.rootVisual);
    if (!detached) return detached.GetStatus();
    if (index + 1U != instances_.Size()) {
        instances_[index] =
            std::move(instances_[instances_.Size() - 1U]);
    }
    instances_.PopBack();
    return {};
}

void TemplateManager::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    for (Instance& instance : instances_) {
        if (instance.parent != &object) continue;
        (void)ApplyBindings(instance, args.property);
        for (const TemplatePropertyTrigger& trigger :
             instance.plan->Triggers()) {
            if (trigger.property == args.property) {
                (void)EvaluateTriggers(instance);
                break;
            }
        }
        return;
    }
}

} // namespace Aero::Controls
