#include <Aero/Controls/Templates.hpp>

#include "presentation/RenderingInternal.hpp"

#include "../presentation/ResourceAssignment.hpp"

#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <cstdio>
#include <utility>

namespace Aero::Controls {

Base::Result<void> Control::RaiseRoutedEvent(
    RoutedEventHandle event,
    RoutedEventArgs* args) noexcept {
    if (routedEvents_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Control is not attached to routed-event services");
    }
    return routedEvents_->RaiseEvent(
        *this, event, args);
}

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

bool IsDeferredBindingSetterValue(
    const PropertyValue& value) noexcept {
    return value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Presentation::BindingSpec::StaticTypeId();
}

Base::Result<PropertyValue> ConvertTemplateBindingValue(
    const TypeRegistry& types,
    const DependencyProperty& target,
    const PropertyValue& value) noexcept {
    if (target.AcceptsAnyValue() ||
        value.Type() == target.ValueType()) {
        return value;
    }
    if (value.Kind() == PropertyValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject() &&
        types.IsDerivedFrom(
            value.Type(),
            target.ValueType())) {
        return value;
    }
    if (target.ValueType() ==
            Core::TypeOf<Presentation::Length>() &&
        value.Type() == Core::TypeOf<double>()) {
        Base::Result<double> numeric =
            Core::ValueCodec<double>::Decode(value);
        if (!numeric) return numeric.GetStatus();
        return Core::ValueCodec<
            Presentation::Length>::Encode(
                Presentation::Length::Pixels(
                    numeric.Value()));
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        target.Name().Data());
}

} // namespace

Base::Result<void> TemplateBuildContext::SetRoot(
    Base::Ref<Base::Object> owner,
    Visual& root) noexcept {
    return SetRoot({}, std::move(owner), root);
}

Base::Result<void> TemplateBuildContext::SetRoot(
    Base::StringView name,
    Base::Ref<Base::Object> owner,
    Visual& root) noexcept {
    if (tree_ == nullptr || parent_ == nullptr ||
        rootVisual_ != nullptr || !owner ||
        owner.Get() != &root || root.AsUIElement() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template root registration is invalid");
    }

    Base::Result<MountEdgeState> mounted =
        mounts_.Attach(*parent_, root);
    if (!mounted) return mounted.GetStatus();
    MountEdgeState mount = std::move(mounted).Value();

    Base::Result<void> selected =
        parent_->SetTemplateChild(root.AsUIElement());
    if (!selected) {
        (void)mounts_.Detach(mount);
        return selected.GetStatus();
    }
    if (root.AsFrameworkElement() != nullptr) {
        Base::Result<void> templated =
            root.AsFrameworkElement()->SetTemplatedParent(parent_);
        if (!templated) {
            (void)parent_->SetTemplateChild(nullptr);
            (void)mounts_.Detach(mount);
            return templated.GetStatus();
        }
    }
    Base::Result<void> added = AddOwnedPart(
        name, std::move(owner), root, mount);
    if (!added) {
        if (root.AsFrameworkElement() != nullptr) {
            (void)root.AsFrameworkElement()->SetTemplatedParent(nullptr);
        }
        (void)parent_->SetTemplateChild(nullptr);
        (void)mounts_.Detach(mount);
        return added.GetStatus();
    }
    rootVisual_ = &root;
    rootElement_ = root.AsUIElement();
    return {};
}

Base::Result<void> TemplateBuildContext::AddPart(
    Base::StringView name,
    Visual& parent,
    Base::Ref<Base::Object> owner,
    Visual& part) noexcept {
    if (tree_ == nullptr || parent_ == nullptr ||
        rootVisual_ == nullptr ||
        !owner || owner.Get() != &part ||
        (!name.Empty() &&
         FindObject(name) != nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template part registration is invalid");
    }

    Base::Result<MountEdgeState> mounted = mounts_.Attach(parent, part);
    if (!mounted) return mounted.GetStatus();
    MountEdgeState mount = std::move(mounted).Value();

    if (part.AsFrameworkElement() != nullptr) {
        Base::Result<void> templated =
            part.AsFrameworkElement()->SetTemplatedParent(parent_);
        if (!templated) {
            (void)mounts_.Detach(mount);
            return templated.GetStatus();
        }
    }
    Base::Result<void> added = AddOwnedPart(
        name, std::move(owner), part, mount);
    if (!added) {
        if (part.AsFrameworkElement() != nullptr) {
            (void)part.AsFrameworkElement()->SetTemplatedParent(nullptr);
        }
        (void)mounts_.Detach(mount);
        return added.GetStatus();
    }
    return {};
}

Base::Result<bool> TemplateBuildContext::ProjectContent(
    ContentControl& owner,
    ContentPresenter& presenter) noexcept {
    return ProjectContentCore(
        owner, presenter, &presenter, nullptr);
}

Base::Result<bool> TemplateBuildContext::ProjectContent(
    ContentControl& owner,
    ContentControl& presenter) noexcept {
    return ProjectContentCore(
        owner, presenter, nullptr, &presenter);
}

Base::Result<bool>
TemplateBuildContext::ProjectContentCore(
    ContentControl& owner,
    Visual& presenterVisual,
    ContentPresenter* presenter,
    ContentControl* contentHost) noexcept {
    if (tree_ == nullptr || parent_ == nullptr ||
        &owner != parent_ || rootVisual_ == nullptr ||
        (presenter == nullptr &&
         contentHost == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template content projection owner is invalid");
    }
    UIElement* content = owner.Content();
    if (content == nullptr) return false;

    bool presenterIsPart = false;
    for (const TemplatePart& part : parts_) {
        presenterIsPart =
            presenterIsPart ||
            part.visual == &presenterVisual;
    }
    if (!presenterIsPart ||
        (content->LogicalParent() != nullptr &&
         content->LogicalParent() != &owner) ||
        (content->VisualParent() != nullptr &&
         content->VisualParent() != &owner)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Template content cannot be projected");
    }

    TemplateContentProjection projection;
    projection.owner = &owner;
    projection.presenter = presenter;
    projection.contentHost = contentHost;
    projection.content = content;
    projection.originalVisualParent = content->VisualParent();

    auto restore = [&]() noexcept {
        (void)mounts_.DetachPresentation(
            projection.projectedMount);
        if (presenter != nullptr) {
            (void)presenter->SetContent(nullptr);
        } else {
            (void)contentHost->SetContent(nullptr);
        }
        if (projection.detachedOriginalPresentation &&
            projection.originalVisualParent != nullptr) {
            (void)mounts_.AttachPresentation(
                *projection.originalVisualParent, *content);
        }
        if (projection.attachedLogical) {
            (void)tree_->DetachLogical(owner, *content);
        }
    };

    if (content->LogicalParent() == nullptr) {
        Base::Result<void> logical = tree_->AttachLogical(owner, *content);
        if (!logical) return logical.GetStatus();
        projection.attachedLogical = true;
    }
    if (projection.originalVisualParent != nullptr) {
        PresentationMountState original;
        original.visualParent = projection.originalVisualParent;
        original.child = content;
        original.visualAttached = true;
        original.layoutAttached = layout_ != nullptr &&
            projection.originalVisualParent->AsUIElement() != nullptr;
        original.renderAttached = renderer_ != nullptr &&
            projection.originalVisualParent->AsFrameworkElement() != nullptr &&
            content->AsFrameworkElement() != nullptr;
        Base::Result<void> detached = mounts_.DetachPresentation(original);
        if (!detached) {
            if (projection.attachedLogical) {
                (void)tree_->DetachLogical(owner, *content);
            }
            return detached.GetStatus();
        }
        projection.detachedOriginalPresentation = true;
    }

    Base::Result<PresentationMountState> projected =
        mounts_.AttachPresentation(
            presenterVisual, *content);
    if (!projected) {
        restore();
        return projected.GetStatus();
    }
    projection.projectedMount = std::move(projected).Value();

    Base::Result<void> selected =
        presenter != nullptr
        ? presenter->SetContent(content)
        : contentHost->SetContent(content);
    if (!selected) {
        restore();
        return selected.GetStatus();
    }
    Base::Result<void> tracked =
        projections_.TryPushBack(std::move(projection));
    if (!tracked) {
        restore();
        return tracked.GetStatus();
    }
    return true;
}

Base::Result<void> TemplateBuildContext::PopulateItemsPresenter(
    ItemsPresenter& presenter,
    const ItemsPanelTemplate* itemsPanel) noexcept {
    if (presenter.ItemsHost() != nullptr) return {};

    Base::Ref<Base::Object> owner;
    if (itemsPanel != nullptr) {
        Base::Result<Base::Ref<Base::Object>> created =
            itemsPanel->Instantiate();
        if (!created) return created.GetStatus();
        owner = std::move(created).Value();
    } else {
        Base::Result<Base::Ref<StackPanel>> created =
            Base::MakeRef<StackPanel>();
        if (!created) return created.GetStatus();
        owner = Base::Ref<Base::Object>(
            std::move(created).Value());
    }
    if (!owner ||
        !presenter.PropertyRegistry().Types().IsDerivedFrom(
            owner->RuntimeType(), Panel::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ItemsPanelTemplate root must be a Panel");
    }
    auto& panel = *static_cast<Panel*>(owner.Get());
    Base::Result<void> selected =
        presenter.SetItemsHost(owner, panel);
    if (!selected) return selected.GetStatus();

    Base::Result<void> mounted =
        AddPart({}, presenter, owner, panel);
    if (!mounted) {
        static_cast<void>(presenter.SetChild(nullptr));
        return mounted.GetStatus();
    }
    return {};
}

Base::Result<void>
TemplateBuildContext::PopulateContentPresenter(
    ContentPresenter& presenter) noexcept {
    if (presenter.Content() != nullptr ||
        presenter.ContentSource().Empty()) {
        return {};
    }
    Base::Result<Base::Ref<TextBlock>> created =
        Base::MakeRef<TextBlock>();
    if (!created) return created.GetStatus();
    Base::Ref<Base::Object> owner(
        created.Value());
    Base::Result<void> selected =
        presenter.SetOwnedContent(
            owner, *created.Value());
    if (!selected) return selected.GetStatus();
    Base::Result<void> mounted =
        AddPart(
            {}, presenter, owner,
            *created.Value());
    if (!mounted) {
        static_cast<void>(
            presenter.SetContent(nullptr));
        return mounted.GetStatus();
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
    Visual& visual,
    MountEdgeState mount) noexcept {
    TemplatePart part;
    Base::Result<void> assigned = part.name.TryAssign(name);
    if (!assigned) return assigned.GetStatus();
    part.owner = std::move(owner);
    part.visual = &visual;
    part.object = &visual;
    part.frameworkElement = visual.AsFrameworkElement();
    part.mount = mount;
    return parts_.TryPushBack(std::move(part));
}

void TemplateBuildContext::Rollback() noexcept {
    for (std::uint32_t index = projections_.Size();
         index > 0U; --index) {
        TemplateContentProjection& projection = projections_[index - 1U];
        if ((projection.presenter == nullptr &&
             projection.contentHost == nullptr) ||
            projection.content == nullptr) {
            continue;
        }
        (void)mounts_.DetachPresentation(
            projection.projectedMount);
        if (projection.presenter != nullptr) {
            (void)projection.presenter->SetContent(nullptr);
        } else {
            (void)projection.contentHost->SetContent(nullptr);
        }
        if (projection.detachedOriginalPresentation &&
            projection.originalVisualParent != nullptr) {
            (void)mounts_.AttachPresentation(
                *projection.originalVisualParent, *projection.content);
        }
        if (projection.attachedLogical && projection.owner != nullptr) {
            (void)tree_->DetachLogical(
                *projection.owner, *projection.content);
        }
    }
    projections_.Clear();

    for (std::uint32_t index = parts_.Size(); index > 0U; --index) {
        TemplatePart& part = parts_[index - 1U];
        if (part.frameworkElement != nullptr) {
            (void)part.frameworkElement->SetTemplatedParent(nullptr);
        }
        (void)mounts_.Detach(part.mount);
    }
    if (parent_ != nullptr) (void)parent_->SetTemplateChild(nullptr);
    parts_.Clear();
    rootVisual_ = nullptr;
    rootElement_ = nullptr;
}

Base::Result<void> FrameworkTemplate::Impl::Configure(
    TemplateFactoryCallback valueFactory,
    void* valueFactoryContext,
    Base::Ref<Base::Object> valueFactoryOwner) noexcept {
    if (sealed) {
        return InvalidTemplate(
            "Cannot modify a sealed TemplateProgram");
    }
    if (valueFactory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TemplateProgram requires an execution factory");
    }
    factory = valueFactory;
    factoryContext = valueFactoryContext;
    factoryOwner = std::move(valueFactoryOwner);
    return {};
}

Base::Result<void> FrameworkTemplate::Impl::SetBaseUri(
    const Base::ResourceUri& value) noexcept {
    if (sealed) {
        return InvalidTemplate(
            "Cannot modify a sealed TemplateProgram");
    }
    baseUri = value;
    return {};
}

Base::Result<void> FrameworkTemplate::Impl::TryAddNamespace(
    Base::StringView prefix,
    Base::StringView uri) noexcept {
    if (sealed) {
        return InvalidTemplate(
            "Cannot modify a sealed TemplateProgram");
    }
    if (uri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template namespace URI is empty");
    }
    for (const TemplateNamespace& existing :
         namespaces) {
        if (existing.prefix.View() == prefix) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Template namespace prefix is duplicated");
        }
    }
    TemplateNamespace entry;
    Base::Result<void> assigned =
        entry.prefix.TryAssign(prefix);
    if (!assigned) return assigned.GetStatus();
    assigned = entry.uri.TryAssign(uri);
    if (!assigned) return assigned.GetStatus();
    return namespaces.TryPushBack(std::move(entry));
}

Base::Result<void> FrameworkTemplate::Impl::Seal() noexcept {
    if (sealed) return {};
    if (factory == nullptr) {
        return InvalidTemplate(
            "TemplateProgram requires an execution factory");
    }
    sealed = true;
    return {};
}

Base::Result<void> FrameworkTemplate::Impl::FreezeRuntimePlan(
    TypeId valueTargetType,
    Base::Vector<TemplateBindingPlan>&& valueBindings,
    Base::Vector<TemplateMetadataBindingPlan>&&
        valueMetadataBindings,
    Base::Vector<TemplatePropertyTrigger>&& valueTriggers,
    Base::Vector<VisualStateGroup>&& valueVisualStateGroups) noexcept {
    if (sealed) {
        return InvalidTemplate(
            "TemplateProgram runtime plan is already frozen");
    }
    if (factory == nullptr ||
        valueTargetType == InvalidTypeId) {
        return InvalidTemplate(
            "TemplateProgram runtime plan is incomplete");
    }
    targetType = valueTargetType;
    bindings = std::move(valueBindings);
    metadataBindings =
        std::move(valueMetadataBindings);
    triggers = std::move(valueTriggers);
    visualStateGroups = std::move(valueVisualStateGroups);
    sealed = true;
    return {};
}

Base::Result<void> FrameworkTemplate::TrySetTargetType(
    TypeId value) noexcept {
    if (sealed_) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (value == InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkTemplate TargetType is invalid");
    }
    targetType_ = value;
    return {};
}

Base::Result<void> FrameworkTemplate::ConfigureFactory(
    TemplateFactoryCallback factory,
    void* factoryContext,
    Base::Ref<Base::Object> factoryOwner) noexcept {
    if (sealed_) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    return program_.Configure(
        factory,
        factoryContext,
        std::move(factoryOwner));
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

Base::Result<void>
FrameworkTemplate::TryAddTemplatedParentBinding(
    Base::StringView targetName,
    Base::StringView path,
    Base::StringView stringFormat,
    DependencyPropertyHandle targetProperty,
    Presentation::BindingMode mode,
    Core::UpdateSourceTrigger updateSourceTrigger) noexcept {
    if (sealed_) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (path.Empty() || !targetProperty.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TemplatedParent Binding requires a path and target property");
    }
    TemplateMetadataBindingPlan binding;
    Base::Result<void> assigned =
        binding.targetName.TryAssign(targetName);
    if (!assigned) return assigned.GetStatus();
    assigned = binding.path.TryAssign(path);
    if (!assigned) return assigned.GetStatus();
    assigned = binding.stringFormat.TryAssign(
        stringFormat);
    if (!assigned) return assigned.GetStatus();
    binding.targetProperty = targetProperty;
    binding.mode = mode;
    binding.updateSourceTrigger = updateSourceTrigger;
    return metadataBindings_.TryPushBack(
        std::move(binding));
}

Base::Result<void> ControlTemplate::SetAuthoredVisualTree(
    const Base::Ref<Base::Object>& value) noexcept {
    if (IsSealed() || !value) {
        return InvalidTemplate(
            "ControlTemplate authored visual tree is invalid");
    }
    authoredVisualTree_ = value;
    return {};
}

Base::Result<void>
ControlTemplate::TryAddAuthoredVisualStateGroup(
    const Base::Ref<Base::Object>& value) noexcept {
    if (IsSealed() || !value) {
        return InvalidTemplate(
            "ControlTemplate authored visual state group is invalid");
    }
    return authoredVisualStateGroups_.TryPushBack(
        value);
}

Base::Result<void> FrameworkTemplate::TryAddPropertyTrigger(
    TemplatePropertyTrigger trigger) noexcept {
    if (sealed_) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (trigger.conditions.Empty()) {
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
    for (std::uint32_t transitionIndex = 0U;
         transitionIndex < group.transitions.Size();
         ++transitionIndex) {
        const VisualTransition& transition =
            group.transitions[transitionIndex];
        if (transition.generatedDurationMicroseconds == 0U &&
            !transition.storyboard) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Visual transition requires a duration or Storyboard");
        }
        const auto stateExists =
            [&](Base::StringView name) noexcept {
                if (name.Empty()) return true;
                for (const VisualState& state :
                     group.states) {
                    if (state.name.View() == name) {
                        return true;
                    }
                }
                return false;
            };
        if (!stateExists(transition.from.View()) ||
            !stateExists(transition.to.View())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Visual transition state name was not found");
        }
        for (std::uint32_t previous = 0U;
             previous < transitionIndex;
             ++previous) {
            const VisualTransition& candidate =
                group.transitions[previous];
            if (candidate.from.View() ==
                    transition.from.View() &&
                candidate.to.View() ==
                    transition.to.View()) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Visual transition route is duplicated");
            }
        }
    }
    return visualStateGroups_.TryPushBack(std::move(group));
}

Base::Result<Base::String>
ControlTemplate::EnsureAuthoredName(
    Base::Object& object) noexcept {
    Base::StringView existing =
        authoredNames_.NameOf(object);
    if (!existing.Empty()) {
        Base::String result;
        Base::Result<void> assigned =
            result.TryAssign(existing);
        return assigned
            ? Base::Result<Base::String>(
                  std::move(result))
            : Base::Result<Base::String>(
                  assigned.GetStatus());
    }
    for (;;) {
        if (generatedNameSequence_ ==
            UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "ControlTemplate generated name sequence is exhausted");
        }
        char raw[64]{};
        const int written = std::snprintf(
            raw,
            sizeof(raw),
            "AeroTemplate%u",
            generatedNameSequence_++);
        if (written <= 0 ||
            static_cast<std::size_t>(written) >=
                sizeof(raw)) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "ControlTemplate generated name is too long");
        }
        const Base::StringView generated(
            raw,
            static_cast<std::uint32_t>(
                written));
        if (authoredNames_.Find(generated) !=
            nullptr) {
            continue;
        }
        Base::Result<void> registered =
            authoredNames_.TryRegister(
                generated, object);
        if (!registered) {
            return registered.GetStatus();
        }
        Base::String result;
        Base::Result<void> assigned =
            result.TryAssign(generated);
        return assigned
            ? Base::Result<Base::String>(
                  std::move(result))
            : Base::Result<Base::String>(
                  assigned.GetStatus());
    }
}

Base::Result<void> FrameworkTemplate::Seal(
    const DependencyPropertyRegistry& properties) noexcept {
    if (sealed_) return {};
    if (!properties.IsFrozen() ||
        targetType_ == InvalidTypeId ||
        program_.Factory() == nullptr ||
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
    for (const TemplateMetadataBindingPlan& binding :
         metadataBindings_) {
        if (binding.path.Empty() ||
            properties.Find(binding.targetProperty) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplatedParent Binding target property was not found");
        }
    }
    for (const TemplatePropertyTrigger& trigger : triggers_) {
        for (const TemplateTriggerCondition& triggerCondition :
             trigger.conditions) {
            const DependencyProperty* condition =
                properties.Find(triggerCondition.property);
            if (condition == nullptr ||
                (triggerCondition.sourceName.Empty() &&
                 condition->MetadataFor(targetType_) == nullptr)) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template trigger condition does not apply to TargetType");
            }
            if (triggerCondition.sourceName.Empty()) {
                Base::Result<void> valid = properties.ValidateValueFor(
                    triggerCondition.property, targetType_, triggerCondition.value);
                if (!valid) return valid.GetStatus();
            }
        }
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
    Base::Result<void> programSealed =
        program_.FreezeRuntimePlan(
            targetType_,
            std::move(bindings_),
            std::move(metadataBindings_),
            std::move(triggers_),
            std::move(visualStateGroups_));
    if (!programSealed) {
        return programSealed.GetStatus();
    }
    Base::Result<void> resourcesSealed =
        resources_.Seal();
    if (!resourcesSealed) {
        return resourcesSealed.GetStatus();
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

Base::Result<bool> Control::ApplyTemplate() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (IsTemplateApplied()) return false;
    if (templateManager_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Control is not attached to a template manager");
    }
    Base::Result<Base::Ref<ControlTemplate>> value =
        GetValue(TemplateProperty);
    if (!value) return value.GetStatus();
    if (!value.Value()) return false;
    Base::Result<TemplateHandle> applied =
        templateManager_->Apply(*this, *value.Value());
    if (!applied) return applied.GetStatus();
    return true;
}

DependencyObject* Control::GetTemplateChild(
    Base::StringView name) const noexcept {
    if (templateManager_ == nullptr ||
        templateHandleValue_ == 0U ||
        name.Empty()) {
        return nullptr;
    }
    return templateManager_->FindName(
        TemplateHandle{templateHandleValue_}, name);
}

DependencyObject* Control::GetTemplateChild(
    TypeId type) const noexcept {
    if (templateManager_ == nullptr ||
        templateHandleValue_ == 0U ||
        type == InvalidTypeId) {
        return nullptr;
    }
    return templateManager_->FindPart(
        TemplateHandle{templateHandleValue_}, type);
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
    control.AttachTemplateManager(*this);
    const std::uint32_t existing = FindInstance(control);
    if (existing != UINT32_MAX) {
        if (instances_[existing].plan == &plan) {
            return instances_[existing].handle;
        }
        Base::Result<void> cleared = ClearAt(existing);
        if (!cleared) return cleared.GetStatus();
    }
    if (nextHandle_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Template handle sequence is exhausted");
    }

    TemplateBuildContext context(
        *tree_, control, layout_, renderer_);
    Base::Result<void> built =
        plan.Factory()(context, plan.FactoryContext());
    if (!built || context.RootVisual() == nullptr) {
        context.Rollback();
        return built
            ? InvalidTemplate(
                "ControlTemplate factory did not set a root")
            : built.GetStatus();
    }

    {
        const std::uint32_t authoredPartCount =
            context.parts_.Size();
        for (std::uint32_t index = 0U;
             index < authoredPartCount;
             ++index) {
            TemplatePart& part =
                context.parts_[index];
            if (part.object == nullptr ||
                !properties_->Types().IsDerivedFrom(
                    part.object->RuntimeType(),
                    ContentPresenter::StaticTypeId())) {
                continue;
            }
            Base::Result<void> populated =
                context.PopulateContentPresenter(
                    *static_cast<ContentPresenter*>(
                        part.object));
            if (!populated) {
                context.Rollback();
                return populated.GetStatus();
            }
        }
    }

    if (properties_->Types().IsDerivedFrom(
            control.RuntimeType(),
            ItemsControl::StaticTypeId())) {
        auto& itemsControl =
            static_cast<ItemsControl&>(control);
        const std::uint32_t authoredPartCount =
            context.parts_.Size();
        for (std::uint32_t index = 0U;
             index < authoredPartCount;
             ++index) {
            TemplatePart& part = context.parts_[index];
            if (part.object == nullptr ||
                !properties_->Types().IsDerivedFrom(
                    part.object->RuntimeType(),
                    ItemsPresenter::StaticTypeId())) {
                continue;
            }
            Base::Result<void> populated =
                context.PopulateItemsPresenter(
                    *static_cast<ItemsPresenter*>(
                        part.object),
                    itemsControl.ItemsPanel());
            if (!populated) {
                context.Rollback();
                return populated.GetStatus();
            }
        }
    }

    Instance instance;
    instance.handle.value = nextHandle_++;
    instance.parent = &control;
    instance.plan = &plan;
    instance.rootVisual = context.rootVisual_;
    instance.rootElement = context.rootElement_;
    instance.parts = std::move(context.parts_);
    instance.projections =
        std::move(context.projections_);
    for (const TemplatePart& part :
         instance.parts) {
        if (!part.name.Empty() && part.owner) {
            Base::Result<void> named =
                instance.names.TryRegister(
                    part.name.View(),
                    *part.owner);
            if (!named) {
                context.parts_ =
                    std::move(instance.parts);
                context.projections_ =
                    std::move(instance.projections);
                context.rootVisual_ =
                    instance.rootVisual;
                context.rootElement_ =
                    instance.rootElement;
                context.Rollback();
                return named.GetStatus();
            }
        }
    }
    context.rootVisual_ = nullptr;
    context.rootElement_ = nullptr;
    Base::Result<void> tracked =
        instances_.TryPushBack(std::move(instance));
    if (!tracked) {
        --nextHandle_;
        context.parts_ = std::move(instance.parts);
        context.projections_ =
            std::move(instance.projections);
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
    bindings = AttachMetadataBindings(stored);
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
    Base::Result<void> notified =
        control.NotifyTemplateApplied(stored.handle.value);
    if (!notified) {
        const Base::Status status = notified.GetStatus();
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
    if (index == UINT32_MAX) return nullptr;
    Base::Object* found =
        instances_[index].names.Find(name);
    if (found != nullptr) {
        for (const TemplatePart& part :
             instances_[index].parts) {
            if (part.owner.Get() == found) {
                return part.object;
            }
        }
    }
    return FindTarget(instances_[index], name);
}

DependencyObject* TemplateManager::FindPart(
    TemplateHandle handle,
    TypeId type) const noexcept {
    const std::uint32_t index = FindInstance(handle);
    if (index == UINT32_MAX ||
        type == InvalidTypeId) {
        return nullptr;
    }
    for (const TemplatePart& part :
         instances_[index].parts) {
        if (part.object != nullptr &&
            properties_->Types().IsDerivedFrom(
                part.object->RuntimeType(), type)) {
            return part.object;
        }
    }
    return nullptr;
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
    if (name.Empty()) return instance.parent;
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
        for (const TemplateTriggerCondition& condition :
             instance.plan->Triggers()[index].conditions) {
            DependencyObject* source =
                FindTarget(
                    instance,
                    condition.sourceName.View());
            if (source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template trigger source name was not found");
            }
            Base::Result<void> subscribed =
                source->TryAddValueChangedHandler(
                    condition.property,
                    propertyChangedHandler_);
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
        for (const TemplateTriggerCondition& condition :
             instance.plan->Triggers()[index].conditions) {
            DependencyObject* source =
                FindTarget(
                    instance,
                    condition.sourceName.View());
            if (source != nullptr) {
                (void)source->RemoveValueChangedHandler(
                    condition.property,
                    propertyChangedHandler_);
            }
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
            thread_local char message[384];
            const Base::StringView targetName =
                binding.targetName.View();
            std::snprintf(
                message,
                sizeof(message),
                "TemplateBinding target '%.*s' was not found; template has %u parts",
                static_cast<int>(
                    targetName.SizeBytes()),
                targetName.Data(),
                instance.parts.Size());
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                message);
        }
        Base::Result<PropertyValue> value =
            instance.parent->GetValue(binding.sourceProperty);
        if (!value) return value.GetStatus();
        const DependencyProperty* targetProperty =
            properties_->Find(binding.targetProperty);
        if (targetProperty == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplateBinding target property was not found");
        }
        Base::Result<PropertyValue> converted =
            ConvertTemplateBindingValue(
                properties_->Types(),
                *targetProperty, value.Value());
        if (!converted) return converted.GetStatus();
        Base::Result<void> applied =
            values_->SetTemplateValue(
                *target,
                binding.targetProperty,
                converted.Value());
        if (!applied) return applied.GetStatus();
    }
    return {};
}

Base::Result<void> TemplateManager::AttachMetadataBindings(
    Instance& instance) noexcept {
    if (instance.plan->MetadataBindings().Empty()) {
        return {};
    }
    if (metadata_ == nullptr || bindings_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TemplatedParent Binding services are unavailable");
    }
    Base::Result<void> reserved =
        instance.metadataBindings.TryReserve(
            instance.plan->MetadataBindings().Size());
    if (!reserved) return reserved.GetStatus();
    for (const TemplateMetadataBindingPlan& binding :
         instance.plan->MetadataBindings()) {
        DependencyObject* target =
            FindTarget(instance, binding.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplatedParent Binding target name was not found");
        }
        Presentation::MetadataBindingDescriptor descriptor;
        descriptor.metadata = metadata_;
        descriptor.source = instance.parent;
        descriptor.target = target;
        descriptor.targetProperty = binding.targetProperty;
        descriptor.path = binding.path.View();
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        Base::Result<Presentation::BindingHandle> attached =
            bindings_->Attach(descriptor);
        if (!attached) return attached.GetStatus();
        Base::Result<void> tracked =
            instance.metadataBindings.TryPushBack(
                attached.Value());
        if (!tracked) {
            (void)bindings_->Detach(attached.Value());
            return tracked.GetStatus();
        }
    }
    return {};
}

void TemplateManager::DetachMetadataBindings(
    Instance& instance) noexcept {
    if (bindings_ != nullptr) {
        for (Presentation::BindingHandle handle :
             instance.metadataBindings) {
            (void)bindings_->Detach(handle);
        }
    }
    instance.metadataBindings.Clear();
}

Base::Result<void> TemplateManager::EvaluateTriggers(
    Instance& instance) noexcept {
    for (const TemplatePropertyTrigger& trigger :
         instance.plan->Triggers()) {
        for (const TemplateTriggerSetter& setter :
             trigger.setters) {
            if (IsDeferredBindingSetterValue(setter.value)) {
                continue;
            }
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
        bool active = true;
        for (const TemplateTriggerCondition& triggerCondition :
             trigger.conditions) {
            DependencyObject* source = FindTarget(
                instance, triggerCondition.sourceName.View());
            if (source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template trigger source name was not found");
            }
            Base::Result<PropertyValue> current =
                source->GetValue(triggerCondition.property);
            if (!current) return current.GetStatus();
            if (current.Value() != triggerCondition.value) {
                active = false;
                break;
            }
        }
        if (!active) continue;
        for (const TemplateTriggerSetter& setter :
             trigger.setters) {
            if (IsDeferredBindingSetterValue(setter.value)) {
                continue;
            }
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
            if (IsDeferredBindingSetterValue(setter.value)) {
                continue;
            }
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
    instance.parent->NotifyTemplateDetached();
    DetachMetadataBindings(instance);
    Unsubscribe(instance);
    Base::Result<void> providers = ClearProviders(instance);
    if (!providers) return providers.GetStatus();

    for (TemplatePart& part : instance.parts) {
        if (part.frameworkElement != nullptr) {
            Base::Result<void> cleared =
                part.frameworkElement->SetTemplatedParent(nullptr);
            if (!cleared) return cleared.GetStatus();
        }
    }
    Base::Result<void> child = instance.parent->SetTemplateChild(nullptr);
    if (!child) return child.GetStatus();

    for (std::uint32_t projectionIndex = instance.projections.Size();
         projectionIndex > 0U; --projectionIndex) {
        TemplateContentProjection& projection =
            instance.projections[projectionIndex - 1U];
        if ((projection.presenter == nullptr &&
             projection.contentHost == nullptr) ||
            projection.content == nullptr) {
            continue;
        }
        Base::Result<void> projectedDetached =
            mounts_.DetachPresentation(projection.projectedMount);
        if (!projectedDetached) return projectedDetached.GetStatus();
        Base::Result<void> presenterCleared =
            projection.presenter != nullptr
            ? projection.presenter->SetContent(nullptr)
            : projection.contentHost->SetContent(nullptr);
        if (!presenterCleared) return presenterCleared.GetStatus();
        if (projection.detachedOriginalPresentation &&
            projection.originalVisualParent != nullptr) {
            Base::Result<PresentationMountState> restored =
                mounts_.AttachPresentation(
                    *projection.originalVisualParent, *projection.content);
            if (!restored) return restored.GetStatus();
        }
        if (projection.attachedLogical && projection.owner != nullptr) {
            Base::Result<void> logicalDetached = tree_->DetachLogical(
                *projection.owner, *projection.content);
            if (!logicalDetached) return logicalDetached.GetStatus();
        }
    }

    for (std::uint32_t partIndex = instance.parts.Size();
         partIndex > 0U; --partIndex) {
        Base::Result<void> detached =
            mounts_.Detach(instance.parts[partIndex - 1U].mount);
        if (!detached) return detached.GetStatus();
    }
    for (TemplatePart& part : instance.parts) {
        if (part.object == nullptr) continue;
        Base::Result<void> untracked = values_->DetachObject(*part.object);
        if (!untracked) return untracked.GetStatus();
    }
    if (index + 1U != instances_.Size()) {
        instances_[index] = std::move(instances_[instances_.Size() - 1U]);
    }
    instances_.PopBack();
    return {};
}

void TemplateManager::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    for (Instance& instance : instances_) {
        const bool parentChanged = instance.parent == &object;
        if (parentChanged) {
            (void)ApplyBindings(instance, args.property);
        }
        bool triggerChanged = false;
        for (const TemplatePropertyTrigger& trigger :
             instance.plan->Triggers()) {
            for (const TemplateTriggerCondition& triggerCondition :
                 trigger.conditions) {
                DependencyObject* source = FindTarget(
                    instance, triggerCondition.sourceName.View());
                if (source == &object &&
                    triggerCondition.property == args.property) {
                    triggerChanged = true;
                    break;
                }
            }
            if (triggerChanged) break;
        }
        if (triggerChanged) {
            (void)EvaluateTriggers(instance);
        }
        if (parentChanged) {
            return;
        }
    }
}

Base::Result<void> FrameworkTemplate::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    return Presentation::Detail::AssignResourceDictionary(
        resources_,
        std::move(value),
        "FrameworkTemplate Resources is already assigned");
}

Base::Result<void> FrameworkTemplate::TryAddAuthoredTrigger(
    Base::Ref<Base::Object> trigger) noexcept {
    if (!trigger || sealed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Template Trigger cannot be added after sealing");
    }
    return authoredTriggers_.TryPushBack(
        std::move(trigger));
}

} // namespace Aero::Controls
