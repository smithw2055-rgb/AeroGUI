#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Controls/ControlTemplate.hpp>
#include "gui/controls/State.hpp" 
#include "gui/templates/TemplateState.hpp"
#include "gui/markup/MarkupWriterState.hpp"

#include "render/RenderTree.hpp"


#include <Aero/Controls.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdio>
#include <new>
#include <utility>
#include "gui/controls/ControlBehavior.hpp"


namespace Aero::Controls {
using Aero::Controls::TemplateEngine;
using Aero::Controls::TemplateHandle;
using namespace Aero::Meta;

namespace {

Base::Status InvalidTemplate(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

[[maybe_unused]] bool IsTargetCompatible(
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
        value.Type() == Data::Binding::StaticTypeId();
}

bool MatchesTemplateCondition(
    DependencyObject& source,
    const TemplateTriggerCondition& condition,
    const PropertyValue& current) noexcept {
    if (condition.value.IsNullObject() &&
        condition.property ==
            Primitives::ToggleButton::
                IsCheckedProperty.Handle() &&
        source.PropertyRegistry().Types().IsDerivedFrom(
            source.RuntimeType(),
            Primitives::ToggleButton::StaticTypeId())) {
        return !static_cast<Primitives::ToggleButton&>(
            source).GetIsChecked().GetHasValue();
    }
    return current == condition.value;
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
        !value.IsNullObject() && value.AsObject() &&
        types.IsAssignableFrom(
            target.ValueType(),
            value.AsObject()->RuntimeType())) {
        return PropertyValue::FromObject(
            target.ValueType(),
            Base::Ref<Base::Object>::FromBorrowed(
                *value.AsObject()));
    }
    if (target.ValueType() ==
            Meta::TypeOf<Aero::Length>() &&
        value.Type() == Meta::TypeOf<double>()) {
        Base::Result<double> numeric =
            Meta::ValueCodec<double>::Decode(value);
        if (!numeric) return numeric.GetStatus();
        return Meta::ValueCodec<
            Aero::Length>::Encode(
                Aero::Length::Pixels(
                    numeric.Value()));
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "Template binding value is not assignable to the target property");
}

} // namespace

Base::Result<void> TemplateBuilder::SetRoot(
    Base::Ref<Base::Object> owner,
    ::Aero::Media::Visual& root) noexcept {
    return SetRoot({}, std::move(owner), root);
}

Base::Result<void> TemplateBuilder::SetRoot(
    Base::StringView name,
    Base::Ref<Base::Object> owner,
    ::Aero::Media::Visual& root) noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    if (state.tree == nullptr || state.parent == nullptr ||
        state.rootVisual != nullptr || !owner ||
        owner.Get() != &root || root.AsUIElement() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template root registration is invalid");
    }

    Base::Result<Aero::ElementAttachment> mounted =
        state.tree->AttachElement(*state.parent, root);
    if (!mounted) return mounted.GetStatus();
    Aero::ElementAttachment mount = std::move(mounted).Value();

    Base::Result<void> selected =
        AeroGuiInternal::SetTemplateRoot(*state.parent, root.AsUIElement());
    if (!selected) {
        (void)state.tree->DetachElement(mount);
        return selected.GetStatus();
    }
    if (root.AsFrameworkElement() != nullptr) {
        Base::Result<void> templated =
            AeroGuiInternal::SetTemplatedParent(
                *root.AsFrameworkElement(), state.parent);
        if (!templated) {
            (void)AeroGuiInternal::SetTemplateRoot(*state.parent, nullptr);
            (void)state.tree->DetachElement(mount);
            return templated.GetStatus();
        }
    }
    Base::Result<void> added = AddOwnedPart(
        name, std::move(owner), root, &mount);
    if (!added) {
        if (root.AsFrameworkElement() != nullptr) {
            (void)AeroGuiInternal::SetTemplatedParent(
                *root.AsFrameworkElement(), nullptr);
        }
        (void)AeroGuiInternal::SetTemplateRoot(*state.parent, nullptr);
        (void)state.tree->DetachElement(mount);
        return added.GetStatus();
    }
    state.rootVisual = &root;
    state.rootElement = root.AsUIElement();
    return {};
}

Base::Result<void> TemplateBuilder::AddPart(
    Base::StringView name,
    ::Aero::Media::Visual& parent,
    Base::Ref<Base::Object> owner,
    ::Aero::Media::Visual& part) noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    if (state.tree == nullptr || state.parent == nullptr ||
        state.rootVisual == nullptr ||
        !owner || owner.Get() != &part ||
        (!name.Empty() &&
         FindObject(name) != nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template part registration is invalid");
    }

    Base::Result<Aero::ElementAttachment> mounted = state.tree->AttachElement(parent, part);
    if (!mounted) return mounted.GetStatus();
    Aero::ElementAttachment mount = std::move(mounted).Value();

    if (part.AsFrameworkElement() != nullptr) {
        Base::Result<void> templated =
            AeroGuiInternal::SetTemplatedParent(
                *part.AsFrameworkElement(), state.parent);
        if (!templated) {
            (void)state.tree->DetachElement(mount);
            return templated.GetStatus();
        }
    }
    Base::Result<void> added = AddOwnedPart(
        name, std::move(owner), part, &mount);
    if (!added) {
        if (part.AsFrameworkElement() != nullptr) {
            (void)AeroGuiInternal::SetTemplatedParent(
                *part.AsFrameworkElement(), nullptr);
        }
        (void)state.tree->DetachElement(mount);
        return added.GetStatus();
    }
    return {};
}

Base::Result<bool> TemplateBuilder::ProjectContent(
    ContentControl& owner,
    ContentPresenter& presenter) noexcept {
    return ProjectContentCore(
        owner, presenter, &presenter, nullptr);
}

Base::Result<bool> TemplateBuilder::ProjectContent(
    ContentControl& owner,
    ContentControl& presenter) noexcept {
    return ProjectContentCore(
        owner, presenter, nullptr, &presenter);
}

Control& TemplateBuilder::TemplatedParent() const noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    return *state.parent;
}

::Aero::Media::Visual* TemplateBuilder::RootVisual() const noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    return state.rootVisual;
}

UIElement* TemplateBuilder::RootElement() const noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    return state.rootElement;
}

Aero::BindingEngine&
TemplateBuilder::Bindings() const noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    AERO_ASSERT(state.bindings != nullptr);
    return *state.bindings;
}

Base::Result<bool>
TemplateBuilder::ProjectContentCore(
    ContentControl& owner,
    ::Aero::Media::Visual& presenterVisual,
    ContentPresenter* presenter,
    ContentControl* contentHost) noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    if (state.tree == nullptr || state.parent == nullptr ||
        &owner != state.parent || state.rootVisual == nullptr ||
        (presenter == nullptr &&
         contentHost == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template content projection owner is invalid");
    }
    UIElement* content = AeroGuiInternal::ContentControlContent(owner);
    if (content == nullptr) return false;

    bool presenterIsPart = false;
    for (const Aero::Controls::TemplatePart& part : state.parts) {
        presenterIsPart =
            presenterIsPart ||
            part.visual == &presenterVisual;
    }
    if (!presenterIsPart ||
        (content->GetLogicalParent() != nullptr &&
         content->GetLogicalParent() != &owner) ||
        (content->GetVisualParent() != nullptr &&
         content->GetVisualParent() != &owner)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Template content cannot be projected");
    }

    Aero::Controls::TemplateContentProjection projection;
    projection.owner = &owner;
    projection.presenter = presenter;
    projection.contentHost = contentHost;
    projection.content = content;
    projection.originalVisualParent = content->GetVisualParent();

    auto restore = [&]() noexcept {
        (void)state.tree->DetachVisual(
            projection.projectedMount);
        if (presenter != nullptr) {
            (void)presenter->SetContent(nullptr);
        } else {
            (void)contentHost->SetContent(nullptr);
        }
        if (projection.detachedOriginalVisual &&
            projection.originalVisualParent != nullptr) {
            (void)state.tree->AttachVisualChild(
                *projection.originalVisualParent, *content);
        }
        if (projection.attachedLogical) {
            (void)state.tree->DetachLogical(owner, *content);
        }
    };

    if (content->GetLogicalParent() == nullptr) {
        Base::Result<void> logical = state.tree->AttachLogical(owner, *content);
        if (!logical) return logical.GetStatus();
        projection.attachedLogical = true;
    }
    if (projection.originalVisualParent != nullptr) {
        Aero::VisualAttachment original;
        original.visualParent = projection.originalVisualParent;
        original.child = content;
        original.visualAttached = true;
        original.layoutAttached = state.layout != nullptr &&
            projection.originalVisualParent->AsUIElement() != nullptr;
        original.renderAttached = state.renderer != nullptr &&
            projection.originalVisualParent->AsFrameworkElement() != nullptr &&
            content->AsFrameworkElement() != nullptr;
        Base::Result<void> detached = state.tree->DetachVisual(original);
        if (!detached) {
            if (projection.attachedLogical) {
                (void)state.tree->DetachLogical(owner, *content);
            }
            return detached.GetStatus();
        }
        projection.detachedOriginalVisual = true;
    }

    Base::Result<Aero::VisualAttachment> projected =
        state.tree->AttachVisualChild(
            presenterVisual, *content);
    if (!projected) {
        restore();
        return projected.GetStatus();
    }
    projection.projectedMount = std::move(projected).Value();

    if (presenter != nullptr) {
        presenter->SetContent(content);
    } else {
        contentHost->SetContent(content);
    }
    Base::Result<void> tracked =
        state.projections.PushBack(std::move(projection));
    if (!tracked) {
        restore();
        return tracked.GetStatus();
    }
    return true;
}

Base::Result<void> TemplateBuilder::PopulateItemsPresenter(
    ItemsPresenter& presenter,
    const ItemsPanelTemplate* itemsPanel) noexcept {
    if (presenter.GetItemsHost() != nullptr) return {};

    Base::Ref<Base::Object> owner;
    if (itemsPanel != nullptr) {
        Base::Result<Base::Ref<Base::Object>> created =
            ItemsPanelTemplateRuntime::Instantiate(*itemsPanel);
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
    presenter.SetItemsHost(owner, panel);

    Base::Result<void> mounted =
        AddPart({}, presenter, owner, panel);
    if (!mounted) {
        presenter.SetChild(nullptr);
        return mounted.GetStatus();
    }
    return {};
}

Base::Result<void>
TemplateBuilder::PopulateContentPresenter(
    ContentPresenter& presenter) noexcept {
    if (presenter.GetContent() != nullptr ||
        presenter.GetContentSource().Empty()) {
        return {};
    }
    Base::Result<Base::Ref<TextBlock>> created =
        Base::MakeRef<TextBlock>();
    if (!created) return created.GetStatus();
    Base::Ref<Base::Object> owner(
        created.Value());
    presenter.SetOwnedContent(owner, *created.Value());
    Base::Result<void> mounted =
        AddPart(
            {}, presenter, owner,
            *created.Value());
    if (!mounted) {
        presenter.SetContent(nullptr);
        return mounted.GetStatus();
    }
    return {};
}

DependencyObject* TemplateBuilder::FindObject(
    Base::StringView name) const noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    for (const Aero::Controls::TemplatePart& part : state.parts) {
        if (part.name.View() == name) return part.object;
    }
    return nullptr;
}


Base::Result<void> TemplateBuilder::AddObjectPart(
    Base::StringView name,
    Base::Ref<Base::Object> owner,
    DependencyObject& object) noexcept {
    auto& state = *static_cast<
        Aero::Controls::TemplateBuildState*>(state_);
    if (name.Empty() || !owner || owner.Get() != &object ||
        FindObject(name) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Named template object registration is invalid");
    }
    Aero::Controls::TemplatePart part;
    Base::Result<void> assigned = part.name.Assign(name);
    if (!assigned) return assigned.GetStatus();
    part.owner = std::move(owner);
    part.object = &object;
    return state.parts.PushBack(std::move(part));
}

Base::Result<void> TemplateBuilder::AddOwnedPart(
    Base::StringView name,
    Base::Ref<Base::Object> owner,
    ::Aero::Media::Visual& visual,
    void* mountState) noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    const auto& mount = *static_cast<const Aero::ElementAttachment*>(mountState);
    Aero::Controls::TemplatePart part;
    Base::Result<void> assigned = part.name.Assign(name);
    if (!assigned) return assigned.GetStatus();
    part.owner = std::move(owner);
    part.visual = &visual;
    part.object = &visual;
    part.frameworkElement = visual.AsFrameworkElement();
    part.mount = mount;
    return state.parts.PushBack(std::move(part));
}

void TemplateBuilder::Rollback() noexcept {
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    for (std::uint32_t index = state.projections.Size();
         index > 0U; --index) {
        Aero::Controls::TemplateContentProjection& projection = state.projections[index - 1U];
        if ((projection.presenter == nullptr &&
             projection.contentHost == nullptr) ||
            projection.content == nullptr) {
            continue;
        }
        (void)state.tree->DetachVisual(
            projection.projectedMount);
        if (projection.presenter != nullptr) {
            (void)projection.presenter->SetContent(nullptr);
        } else {
            (void)projection.contentHost->SetContent(nullptr);
        }
        if (projection.detachedOriginalVisual &&
            projection.originalVisualParent != nullptr) {
            (void)state.tree->AttachVisualChild(
                *projection.originalVisualParent, *projection.content);
        }
        if (projection.attachedLogical && projection.owner != nullptr) {
            (void)state.tree->DetachLogical(
                *projection.owner, *projection.content);
        }
    }
    state.projections.Clear();

    for (std::uint32_t index = state.parts.Size(); index > 0U; --index) {
        Aero::Controls::TemplatePart& part = state.parts[index - 1U];
        if (part.frameworkElement != nullptr) {
            (void)AeroGuiInternal::SetTemplatedParent(*part.frameworkElement, nullptr);
        }
        if (part.mount.IsAttached()) {
            (void)state.tree->DetachElement(part.mount);
        }
    }
    if (state.parent != nullptr) (void)AeroGuiInternal::SetTemplateRoot(*state.parent, nullptr);
    state.parts.Clear();
    state.rootVisual = nullptr;
    state.rootElement = nullptr;
}

Base::Result<void> TemplateProgram::Configure(
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

Base::Result<void> TemplateProgram::SetBaseUri(
    const Base::ResourceUri& value) noexcept {
    if (sealed) {
        return InvalidTemplate(
            "Cannot modify a sealed TemplateProgram");
    }
    baseUri = value;
    return {};
}

Base::Result<void> TemplateProgram::AddNamespace(
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
        entry.prefix.Assign(prefix);
    if (!assigned) return assigned.GetStatus();
    assigned = entry.uri.Assign(uri);
    if (!assigned) return assigned.GetStatus();
    return namespaces.PushBack(std::move(entry));
}

Base::Result<void> TemplateProgram::Seal() noexcept {
    if (sealed) return {};
    if (factory == nullptr) {
        return InvalidTemplate(
            "TemplateProgram requires an execution factory");
    }
    sealed = true;
    return {};
}

Base::Result<void> TemplateProgram::FreezeRuntimePlan(
    TypeId valueTargetType,
    Base::Vector<TemplateBindingPlan>&& valueBindings,
    Base::Vector<TemplateMetadataBindingPlan>&&
        valueMetadataBindings,
    Base::Vector<TemplateDynamicResourcePlan>&&
        valueDynamicResources,
    Base::Vector<TemplatePropertyTrigger>&& valueTriggers,
    Base::Vector<VisualStateGroupPlan>&& valueVisualStateGroups) noexcept {
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
    dynamicResources =
        std::move(valueDynamicResources);
    triggers = std::move(valueTriggers);
    visualStateGroups = std::move(valueVisualStateGroups);
    sealed = true;
    return {};
}


} // namespace Aero::Controls

namespace Aero {

FrameworkTemplate::FrameworkTemplate() noexcept
    : state_(new (std::nothrow) Controls::FrameworkTemplateState()) {
    if (state_ == nullptr) {
        Base::ReportOutOfMemory(sizeof(Controls::FrameworkTemplateState), alignof(Controls::FrameworkTemplateState), Base::MemoryTag::Ui);
    }
}

FrameworkTemplate::~FrameworkTemplate() noexcept {
    delete static_cast<Controls::FrameworkTemplateState*>(state_);
    state_ = nullptr;
}

Meta::TypeId FrameworkTemplate::GetTargetType() const noexcept {
    const Controls::FrameworkTemplateState* state = static_cast<const Controls::FrameworkTemplateState*>(state_);
    if (state == nullptr) return Meta::InvalidTypeId;
    return state->sealed ? state->program.targetType : state->targetType;
}

bool FrameworkTemplate::GetIsSealed() const noexcept {
    const Controls::FrameworkTemplateState* state = static_cast<const Controls::FrameworkTemplateState*>(state_);
    return state != nullptr && state->sealed;
}

ResourceDictionary& FrameworkTemplate::GetResources() noexcept {
    auto* state = static_cast<Controls::FrameworkTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

const ResourceDictionary& FrameworkTemplate::GetResources() const noexcept {
    const auto* state = static_cast<const Controls::FrameworkTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

Controls::FrameworkTemplateState* FrameworkTemplateRuntime::State(
    FrameworkTemplate& value) noexcept {
    return static_cast<Controls::FrameworkTemplateState*>(value.state_);
}

const Controls::FrameworkTemplateState* FrameworkTemplateRuntime::State(
    const FrameworkTemplate& value) noexcept {
    return static_cast<const Controls::FrameworkTemplateState*>(value.state_);
}

} // namespace Aero

namespace Aero::Controls {

using namespace ::Aero;
using namespace ::Aero::Meta;
using namespace ::Aero::Controls;
using namespace ::Aero::Controls;

::Aero::Controls::FrameworkTemplateState* TemplatePrivate::State(FrameworkTemplate& value) noexcept {
    return ::Aero::FrameworkTemplateRuntime::State(value);
}

const ::Aero::Controls::FrameworkTemplateState* TemplatePrivate::State(const FrameworkTemplate& value) noexcept {
    return ::Aero::FrameworkTemplateRuntime::State(value);
}

Base::Result<void> TemplatePrivate::SetTargetType(
    FrameworkTemplate& templateValue,
    TypeId value) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (state->sealed) return InvalidTemplate("Cannot modify a sealed FrameworkTemplate");
    if (value == InvalidTypeId) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "FrameworkTemplate TargetType is invalid");
    state->targetType = value;
    return {};
}

Base::Result<void> TemplatePrivate::ConfigureFactory(
    FrameworkTemplate& templateValue,
    TemplateFactoryCallback factory,
    void* factoryContext,
    Base::Ref<Base::Object> factoryOwner) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (state->sealed) return InvalidTemplate("Cannot modify a sealed FrameworkTemplate");
    return state->program.Configure(factory, factoryContext, std::move(factoryOwner));
}

Base::Result<void> TemplatePrivate::AddTemplateBinding(
    FrameworkTemplate& templateValue,
    Base::StringView targetName,
    DependencyPropertyHandle sourceProperty,
    DependencyPropertyHandle targetProperty) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (state->sealed) return InvalidTemplate("Cannot modify a sealed FrameworkTemplate");
    if (!sourceProperty.IsValid() || !targetProperty.IsValid()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "TemplateBinding requires source and target properties");
    TemplateBindingPlan binding;
    Base::Result<void> assigned = binding.targetName.Assign(targetName);
    if (!assigned) return assigned.GetStatus();
    binding.sourceProperty = sourceProperty;
    binding.targetProperty = targetProperty;
    return state->bindings.PushBack(std::move(binding));
}

Base::Result<void> TemplatePrivate::AddTemplatedParentBinding(
    FrameworkTemplate& templateValue,
    Base::StringView targetName,
    Base::StringView path,
    Base::StringView stringFormat,
    DependencyPropertyHandle targetProperty,
    Data::BindingMode mode,
    Meta::UpdateSourceTrigger updateSourceTrigger) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (state->sealed) return InvalidTemplate("Cannot modify a sealed FrameworkTemplate");
    if (path.Empty() || !targetProperty.IsValid()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "TemplatedParent Binding requires a path and target property");
    TemplateMetadataBindingPlan binding;
    Base::Result<void> assigned = binding.targetName.Assign(targetName);
    if (!assigned) return assigned.GetStatus();
    assigned = binding.path.Assign(path);
    if (!assigned) return assigned.GetStatus();
    assigned = binding.stringFormat.Assign(stringFormat);
    if (!assigned) return assigned.GetStatus();
    binding.targetProperty = targetProperty;
    binding.mode = mode;
    binding.updateSourceTrigger = updateSourceTrigger;
    return state->metadataBindings.PushBack(std::move(binding));
}

Base::Result<void> TemplatePrivate::AddDynamicResource(
    FrameworkTemplate& templateValue,
    Base::StringView targetName,
    Base::StringView key,
    DependencyPropertyHandle targetProperty) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "FrameworkTemplate state allocation failed");
    }
    if (state->sealed) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (targetName.Empty() || key.Empty() || !targetProperty.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template DynamicResource declaration is incomplete");
    }
    TemplateDynamicResourcePlan resource;
    Base::Result<void> assigned = resource.targetName.Assign(targetName);
    if (assigned) assigned = resource.key.Assign(key);
    if (!assigned) return assigned.GetStatus();
    resource.targetProperty = targetProperty;
    return state->dynamicResources.PushBack(std::move(resource));
}

Base::Result<void> TemplatePrivate::SetAuthoredVisualTree(
    ControlTemplate& templateValue,
    const Base::Ref<Base::Object>& value) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ControlTemplate state allocation failed");
    if (state->sealed || !value) return InvalidTemplate("ControlTemplate authored visual tree is invalid");
    state->authoredVisualTree = value;
    return {};
}

Base::Result<void> TemplatePrivate::AddAuthoredVisualStateGroup(
    ControlTemplate& templateValue,
    const Base::Ref<Base::Object>& value) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ControlTemplate state allocation failed");
    if (state->sealed || !value) return InvalidTemplate("ControlTemplate authored visual state group is invalid");
    return state->authoredVisualStateGroups.PushBack(value);
}

void TemplatePrivate::ClearAuthoredVisualTree(ControlTemplate& value) noexcept {
    FrameworkTemplateState* state = State(value);
    if (state != nullptr) state->authoredVisualTree.Reset();
}

void TemplatePrivate::ClearAuthoredVisualStateGroups(ControlTemplate& value) noexcept {
    FrameworkTemplateState* state = State(value);
    if (state != nullptr) state->authoredVisualStateGroups.Clear();
}

void TemplatePrivate::ClearAuthoredTriggers(FrameworkTemplate& value) noexcept {
    FrameworkTemplateState* state = State(value);
    if (state != nullptr) state->authoredTriggers.Clear();
}

Base::Result<void> TemplatePrivate::AddPropertyTrigger(
    FrameworkTemplate& templateValue,
    TemplatePropertyTrigger trigger) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (state->sealed) return InvalidTemplate("Cannot modify a sealed FrameworkTemplate");
    if (trigger.conditions.Empty()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Template property trigger is incomplete");
    return state->triggers.PushBack(std::move(trigger));
}

Base::Result<void> TemplatePrivate::AddVisualStateGroup(
    FrameworkTemplate& templateValue,
    VisualStateGroupPlan group) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (state->sealed) {
        return InvalidTemplate(
            "Cannot modify a sealed FrameworkTemplate");
    }
    if (group.name.Empty() || group.states.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Visual state group requires a name and at least one state");
    }
    for (const VisualStateGroupPlan& existing : state->visualStateGroups) {
        if (existing.name.View() == group.name.View()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Visual state group name is duplicated");
        }
    }
    for (std::uint32_t stateIndex = 0U;
        stateIndex < group.states.Size(); ++stateIndex) {
        const VisualStatePlan& state = group.states[stateIndex];
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
            const VisualStateSetterPlan& setter =
                state.setters[setterIndex];
            if (!setter.property.IsValid() ||
                setter.value.IsUnset()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Visual state setter is incomplete");
            }
            for (std::uint32_t previous = 0U;
                previous < setterIndex; ++previous) {
                const VisualStateSetterPlan& candidate =
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
        const VisualTransitionPlan& transition =
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
                for (const VisualStatePlan& state :
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
            const VisualTransitionPlan& candidate =
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
    return state->visualStateGroups.PushBack(std::move(group));
}


Base::Result<void> TemplatePrivate::RegisterAuthoredName(
    ControlTemplate& templateValue, Base::StringView name, Base::Object& object) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ControlTemplate state allocation failed");
    return state->authoredNames.Register(name, object);
}

Base::Result<Base::String> TemplatePrivate::EnsureAuthoredName(
    ControlTemplate& templateValue,
    Base::Object& object) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ControlTemplate state allocation failed");
    Base::StringView existing =
        state->authoredNames.NameOf(object);
    if (!existing.Empty()) {
        Base::String result;
        Base::Result<void> assigned =
            result.Assign(existing);
        return assigned
            ? Base::Result<Base::String>(
                  std::move(result))
            : Base::Result<Base::String>(
                  assigned.GetStatus());
    }
    for (;;) {
        if (state->generatedNameSequence ==
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
            state->generatedNameSequence++);
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
        if (state->authoredNames.Find(generated) !=
            nullptr) {
            continue;
        }
        Base::Result<void> registered =
            state->authoredNames.Register(
                generated, object);
        if (!registered) {
            return registered.GetStatus();
        }
        Base::String result;
        Base::Result<void> assigned =
            result.Assign(generated);
        return assigned
            ? Base::Result<Base::String>(
                  std::move(result))
            : Base::Result<Base::String>(
                  assigned.GetStatus());
    }
}


Base::Result<void> TemplatePrivate::AddAuthoredTrigger(
    FrameworkTemplate& templateValue, Base::Ref<Base::Object> trigger) noexcept {
    FrameworkTemplateState* state = State(templateValue);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (!trigger || state->sealed) return Base::Status::Failure(Base::ErrorCode::InvalidState, "Template Trigger cannot be added after sealing");
    return state->authoredTriggers.PushBack(std::move(trigger));
}

const Base::Ref<Base::Object>& TemplatePrivate::AuthoredVisualTree(const ControlTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? state->authoredVisualTree : empty;
}

Base::Span<const Base::Ref<Base::Object>> TemplatePrivate::AuthoredVisualStateGroups(const ControlTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? Base::Span<const Base::Ref<Base::Object>>(state->authoredVisualStateGroups.Data(), state->authoredVisualStateGroups.Size()) : Base::Span<const Base::Ref<Base::Object>>{};
}

const NameScope& TemplatePrivate::AuthoredNames(const ControlTemplate& value) noexcept {
    static NameScope empty;
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? state->authoredNames : empty;
}

void TemplatePrivate::ClearAuthoredNames(ControlTemplate& value) noexcept {
    FrameworkTemplateState* state = State(value);
    if (state != nullptr) state->authoredNames.Clear();
}

Base::Span<const Base::Ref<Base::Object>> TemplatePrivate::AuthoredTriggers(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? Base::Span<const Base::Ref<Base::Object>>(state->authoredTriggers.Data(), state->authoredTriggers.Size()) : Base::Span<const Base::Ref<Base::Object>>{};
}

TemplateFactoryCallback TemplatePrivate::Factory(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? state->program.factory : nullptr;
}

void* TemplatePrivate::FactoryContext(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? state->program.factoryContext : nullptr;
}

const Base::Ref<Base::Object>& TemplatePrivate::FactoryOwner(const FrameworkTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? state->program.factoryOwner : empty;
}

const Base::ResourceUri& TemplatePrivate::BaseUri(const FrameworkTemplate& value) noexcept {
    static Base::ResourceUri empty;
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? state->program.baseUri : empty;
}

Base::Result<void> TemplatePrivate::SetBaseUri(FrameworkTemplate& value, const Base::ResourceUri& uri) noexcept {
    FrameworkTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    return state->program.SetBaseUri(uri);
}

Base::Result<void> TemplatePrivate::AddNamespace(FrameworkTemplate& value, Base::StringView prefix, Base::StringView uri) noexcept {
    FrameworkTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    return state->program.AddNamespace(prefix, uri);
}

Base::Span<const TemplateNamespace> TemplatePrivate::Namespaces(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    return state != nullptr ? Base::Span<const TemplateNamespace>(state->program.namespaces.Data(), state->program.namespaces.Size()) : Base::Span<const TemplateNamespace>{};
}

Base::Span<const TemplateBindingPlan> TemplatePrivate::Bindings(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    if (state == nullptr) return {};
    const auto& values = state->sealed ? state->program.bindings : state->bindings;
    return {values.Data(), values.Size()};
}

Base::Span<const TemplateMetadataBindingPlan> TemplatePrivate::MetadataBindings(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    if (state == nullptr) return {};
    const auto& values = state->sealed ? state->program.metadataBindings : state->metadataBindings;
    return {values.Data(), values.Size()};
}

Base::Span<const TemplateDynamicResourcePlan> TemplatePrivate::DynamicResources(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    if (state == nullptr) return {};
    const auto& values = state->sealed
        ? state->program.dynamicResources
        : state->dynamicResources;
    return {values.Data(), values.Size()};
}

Base::Span<const TemplatePropertyTrigger> TemplatePrivate::Triggers(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    if (state == nullptr) return {};
    const auto& values = state->sealed ? state->program.triggers : state->triggers;
    return {values.Data(), values.Size()};
}

Base::Span<const VisualStateGroupPlan> TemplatePrivate::VisualStateGroups(const FrameworkTemplate& value) noexcept {
    const FrameworkTemplateState* state = State(value);
    if (state == nullptr) return {};
    const auto& values = state->sealed ? state->program.visualStateGroups : state->visualStateGroups;
    return {values.Data(), values.Size()};
}

Base::Result<void> TemplatePrivate::Seal(
    FrameworkTemplate& templateValue,
    const DependencyPropertyRegistry& properties) noexcept {
    FrameworkTemplateState* templateState = State(templateValue);
    if (templateState == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "FrameworkTemplate state allocation failed");
    if (templateState->sealed) return {};
    if (!properties.IsFrozen() ||
        templateState->targetType == InvalidTypeId ||
        templateState->program.factory == nullptr ||
        properties.Types().FindType(templateState->targetType) == nullptr) {
        return InvalidTemplate(
            "FrameworkTemplate requires a factory and registered target type");
    }
    for (const TemplateBindingPlan& binding : templateState->bindings) {
        const DependencyProperty* source =
            properties.Find(binding.sourceProperty);
        const DependencyProperty* target =
            properties.Find(binding.targetProperty);
        if (source == nullptr || target == nullptr ||
            source->MetadataFor(templateState->targetType) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplateBinding property was not found");
        }
    }
    for (const TemplateMetadataBindingPlan& binding :
         templateState->metadataBindings) {
        if (binding.path.Empty() ||
            properties.Find(binding.targetProperty) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplatedParent Binding target property was not found");
        }
    }
    for (const TemplateDynamicResourcePlan& resource :
         templateState->dynamicResources) {
        if (resource.targetName.Empty() || resource.key.Empty() ||
            properties.Find(resource.targetProperty) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Template DynamicResource target property was not found");
        }
    }
    for (const TemplatePropertyTrigger& trigger : templateState->triggers) {
        for (const TemplateTriggerCondition& triggerCondition :
             trigger.conditions) {
            const DependencyProperty* condition =
                properties.Find(triggerCondition.property);
            if (condition == nullptr ||
                (triggerCondition.sourceName.Empty() &&
                 condition->MetadataFor(templateState->targetType) == nullptr)) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template trigger condition does not apply to TargetType");
            }
            if (triggerCondition.sourceName.Empty()) {
                Base::Result<void> valid = properties.ValidateValueFor(
                    triggerCondition.property, templateState->targetType, triggerCondition.value);
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
        groupIndex < templateState->visualStateGroups.Size(); ++groupIndex) {
        const VisualStateGroupPlan& group =
            templateState->visualStateGroups[groupIndex];
        for (const VisualStatePlan& state : group.states) {
            for (const VisualStateSetterPlan& setter : state.setters) {
                const DependencyProperty* property =
                    properties.Find(setter.property);
                if (property == nullptr || property->GetIsReadOnly()) {
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
                    for (const VisualStatePlan& otherState :
                        templateState->visualStateGroups[otherIndex].states) {
                        for (const VisualStateSetterPlan& otherSetter :
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
        templateState->program.FreezeRuntimePlan(
            templateState->targetType,
            std::move(templateState->bindings),
            std::move(templateState->metadataBindings),
            std::move(templateState->dynamicResources),
            std::move(templateState->triggers),
            std::move(templateState->visualStateGroups));
    if (!programSealed) {
        return programSealed.GetStatus();
    }
    Base::Result<void> resourcesSealed =
        templateState->resources.Seal();
    if (!resourcesSealed) {
        return resourcesSealed.GetStatus();
    }
    templateState->sealed = true;
    return {};
}

} // namespace Aero::Controls

namespace Aero::Controls {


bool Control::ApplyTemplate() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return false;
    if (AeroGuiInternal::IsTemplateApplied(*this)) return false;
    auto* templateRuntime = static_cast<TemplateEngine*>(
        AeroGuiInternal::TemplateRuntime(*this));
    if (templateRuntime == nullptr) {
        return false;
    }
    const Base::Ref<ControlTemplate> value =
        GetValue(TemplateProperty);
    if (!value) return false;
    Base::Result<TemplateHandle> applied =
        templateRuntime->Apply(*this, *value);
    if (!applied) return false;
    // TemplateEngine has now completed its instance transaction and installed
    // the handle/name scope. Derived controls may safely resolve PART_* and
    // materialize item containers without re-entering the transaction.
    OnApplyTemplate();
    return true;
}

DependencyObject* Control::GetTemplateChild(
    Base::StringView name) const noexcept {
    auto* templateRuntime = static_cast<TemplateEngine*>(
        AeroGuiInternal::TemplateRuntime(*this));
    if (templateRuntime == nullptr ||
        templateHandleValue_ == 0U ||
        name.Empty()) {
        return nullptr;
    }
    return templateRuntime->FindName(
        TemplateHandle{templateHandleValue_}, name);
}

DependencyObject* Control::GetTemplateChild(
    TypeId type) const noexcept {
    auto* templateRuntime = static_cast<TemplateEngine*>(
        AeroGuiInternal::TemplateRuntime(*this));
    if (templateRuntime == nullptr ||
        templateHandleValue_ == 0U ||
        type == InvalidTypeId) {
        return nullptr;
    }
    return templateRuntime->FindPart(
        TemplateHandle{templateHandleValue_}, type);
}

} // namespace Aero::Controls

namespace Aero {

void FrameworkTemplate::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    Controls::FrameworkTemplateState* state =
        static_cast<Controls::FrameworkTemplateState*>(state_);
    if (state == nullptr) return;
    (void)Aero::AssignResourceDictionary(
        state->resources,
        std::move(value),
        "FrameworkTemplate Resources is already assigned");
}

} // namespace Aero

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;

TemplateEngine::~TemplateEngine() noexcept {
    while (!instances_.Empty()) {
        if (!ClearAt(instances_.Size() - 1U)) {
            break;
        }
    }
}

Base::Result<TemplateHandle> TemplateEngine::Apply(
    Control& control,
    const ControlTemplate& plan) noexcept {
    if (tree_ == nullptr || values_ == nullptr ||
        properties_ == nullptr || !plan.GetIsSealed() ||
        control.GetTree() != tree_ ||
        !IsTargetCompatible(
            properties_->Types(),
            control.RuntimeType(),
            plan.GetTargetType())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ControlTemplate cannot be applied to this control");
    }
    AeroGuiInternal::AttachTemplateEngine(
        control, this);
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

    Aero::Controls::TemplateBuildState buildState(
        *tree_, control, layout_, renderer_, bindings_);
    TemplateBuilder context(&buildState);
    Base::Result<void> built =
        Aero::Controls::TemplatePrivate::Factory(plan)(context, Aero::Controls::TemplatePrivate::FactoryContext(plan));
    if (!built || context.RootVisual() == nullptr) {
        context.Rollback();
        return built
            ? InvalidTemplate(
                "ControlTemplate factory did not set a root")
            : built.GetStatus();
    }

    {
        const std::uint32_t authoredPartCount =
            buildState.parts.Size();
        for (std::uint32_t index = 0U;
             index < authoredPartCount;
             ++index) {
            Aero::Controls::TemplatePart& part =
                buildState.parts[index];
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
            buildState.parts.Size();
        for (std::uint32_t index = 0U;
             index < authoredPartCount;
             ++index) {
            Aero::Controls::TemplatePart& part = buildState.parts[index];
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
                    itemsControl.GetItemsPanel());
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
    instance.rootVisual = buildState.rootVisual;
    instance.rootElement = buildState.rootElement;
    instance.parts = std::move(buildState.parts);
    instance.projections =
        std::move(buildState.projections);
    for (const Aero::Controls::TemplatePart& part :
         instance.parts) {
        if (!part.name.Empty() && part.owner) {
            Base::Result<void> named =
                instance.names.Register(
                    part.name.View(),
                    *part.owner);
            if (!named) {
                buildState.parts =
                    std::move(instance.parts);
                buildState.projections =
                    std::move(instance.projections);
                buildState.rootVisual =
                    instance.rootVisual;
                buildState.rootElement =
                    instance.rootElement;
                context.Rollback();
                return named.GetStatus();
            }
        }
    }
    buildState.rootVisual = nullptr;
    buildState.rootElement = nullptr;
    Base::Result<void> tracked =
        instances_.PushBack(std::move(instance));
    if (!tracked) {
        --nextHandle_;
        buildState.parts = std::move(instance.parts);
        buildState.projections =
            std::move(instance.projections);
        buildState.rootVisual = instance.rootVisual;
        buildState.rootElement = instance.rootElement;
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
    bindings = AttachDynamicResources(stored);
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
    AeroGuiInternal::NotifyTemplateApplied(
        control, stored.handle.value);
    return stored.handle;
}

Base::Result<bool> TemplateEngine::Clear(
    TemplateHandle handle) noexcept {
    const std::uint32_t index = FindInstance(handle);
    if (index == UINT32_MAX) return false;
    Base::Result<void> cleared = ClearAt(index);
    if (!cleared) return cleared.GetStatus();
    return true;
}

Base::Result<bool> TemplateEngine::Clear(
    Control& control) noexcept {
    const std::uint32_t index = FindInstance(control);
    if (index == UINT32_MAX) return false;
    Base::Result<void> cleared = ClearAt(index);
    if (!cleared) return cleared.GetStatus();
    return true;
}

DependencyObject* TemplateEngine::FindName(
    TemplateHandle handle,
    Base::StringView name) const noexcept {
    const std::uint32_t index = FindInstance(handle);
    if (index == UINT32_MAX) return nullptr;
    Base::Object* found =
        instances_[index].names.Find(name);
    if (found != nullptr) {
        for (const Aero::Controls::TemplatePart& part :
             instances_[index].parts) {
            if (part.owner.Get() == found) {
                return part.object;
            }
        }
    }
    return FindTarget(instances_[index], name);
}

DependencyObject* TemplateEngine::FindPart(
    TemplateHandle handle,
    TypeId type) const noexcept {
    const std::uint32_t index = FindInstance(handle);
    if (index == UINT32_MAX ||
        type == InvalidTypeId) {
        return nullptr;
    }
    // A direct panel declared with IsItemsHost is the XAML contract for an
    // ItemsControl without an ItemsPresenter. Prefer it over structural
    // panels such as the template root when a caller asks for a Panel.
    if (type == Panel::StaticTypeId()) {
        for (const Aero::Controls::TemplatePart& part :
             instances_[index].parts) {
            if (part.object == nullptr ||
                !properties_->Types().IsDerivedFrom(
                    part.object->RuntimeType(), Panel::StaticTypeId())) {
                continue;
            }
            auto& panel = *static_cast<Panel*>(part.object);
            if (panel.GetValueOr(Panel::IsItemsHostProperty, false)) {
                return part.object;
            }
        }
    }
    for (const Aero::Controls::TemplatePart& part :
        instances_[index].parts) {
        if (part.object != nullptr &&
            properties_->Types().IsDerivedFrom(
                part.object->RuntimeType(), type)) {
            return part.object;
        }
    }
    return nullptr;
}

TemplateHandle TemplateEngine::AppliedHandle(
    const Control& control) const noexcept {
    const std::uint32_t index = FindInstance(control);
    return index != UINT32_MAX
        ? instances_[index].handle : TemplateHandle{};
}

const ControlTemplate* TemplateEngine::AppliedTemplate(
    TemplateHandle handle) const noexcept {
    const std::uint32_t index = FindInstance(handle);
    return index != UINT32_MAX
        ? instances_[index].plan : nullptr;
}

std::uint32_t TemplateEngine::FindInstance(
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

std::uint32_t TemplateEngine::FindInstance(
    const Control& control) const noexcept {
    for (std::uint32_t index = 0U;
         index < instances_.Size();
         ++index) {
        if (instances_[index].parent == &control) return index;
    }
    return UINT32_MAX;
}

DependencyObject* TemplateEngine::FindTarget(
    const Instance& instance,
    Base::StringView name) const noexcept {
    if (name.Empty()) return instance.parent;
    for (const Aero::Controls::TemplatePart& part : instance.parts) {
        if (part.name.View() == name) return part.object;
    }
    return nullptr;
}

Base::Result<void> TemplateEngine::Subscribe(
    Instance& instance) noexcept {
    for (std::uint32_t index = 0U;
         index < Aero::Controls::TemplatePrivate::Bindings(*instance.plan).Size();
         ++index) {
        const DependencyPropertyHandle property =
            Aero::Controls::TemplatePrivate::Bindings(*instance.plan)[index].sourceProperty;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                Aero::Controls::TemplatePrivate::Bindings(*instance.plan)[previous].sourceProperty !=
                    property;
        }
        if (first) {
            Base::Result<void> subscribed =
                instance.parent->AddValueChangedHandlerChecked(
                    property, propertyChangedHandler_);
            if (!subscribed) return subscribed.GetStatus();
        }
    }
    for (std::uint32_t index = 0U;
         index < Aero::Controls::TemplatePrivate::Triggers(*instance.plan).Size();
         ++index) {
        for (const TemplateTriggerCondition& condition :
             Aero::Controls::TemplatePrivate::Triggers(*instance.plan)[index].conditions) {
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
                source->AddValueChangedHandlerChecked(
                    condition.property,
                    propertyChangedHandler_);
            if (!subscribed) return subscribed.GetStatus();
        }
    }
    return {};
}

void TemplateEngine::Unsubscribe(
    Instance& instance) noexcept {
    for (std::uint32_t index = 0U;
         index < Aero::Controls::TemplatePrivate::Bindings(*instance.plan).Size();
         ++index) {
        const DependencyPropertyHandle property =
            Aero::Controls::TemplatePrivate::Bindings(*instance.plan)[index].sourceProperty;
        bool first = true;
        for (std::uint32_t previous = 0U;
             previous < index;
             ++previous) {
            first = first &&
                Aero::Controls::TemplatePrivate::Bindings(*instance.plan)[previous].sourceProperty !=
                    property;
        }
        if (first) {
            (void)instance.parent->RemoveValueChangedHandler(
                property, propertyChangedHandler_);
        }
    }
    for (std::uint32_t index = 0U;
         index < Aero::Controls::TemplatePrivate::Triggers(*instance.plan).Size();
         ++index) {
        for (const TemplateTriggerCondition& condition :
             Aero::Controls::TemplatePrivate::Triggers(*instance.plan)[index].conditions) {
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

Base::Result<void> TemplateEngine::ApplyBindings(
    Instance& instance,
    DependencyPropertyHandle changed) noexcept {
    for (const TemplateBindingPlan& binding :
         Aero::Controls::TemplatePrivate::Bindings(*instance.plan)) {
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

Base::Result<void> TemplateEngine::AttachMetadataBindings(
    Instance& instance) noexcept {
    if (Aero::Controls::TemplatePrivate::MetadataBindings(*instance.plan).Empty()) {
        return {};
    }
    if (metadata_ == nullptr || bindings_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "TemplatedParent Binding services are unavailable");
    }
    Base::Result<void> reserved =
        instance.metadataBindings.Reserve(
            Aero::Controls::TemplatePrivate::MetadataBindings(*instance.plan).Size());
    if (!reserved) return reserved.GetStatus();
    for (const TemplateMetadataBindingPlan& binding :
         Aero::Controls::TemplatePrivate::MetadataBindings(*instance.plan)) {
        DependencyObject* target =
            FindTarget(instance, binding.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "TemplatedParent Binding target name was not found");
        }
        Data::MetadataBindingDescriptor descriptor;
        descriptor.metadata = metadata_;
        descriptor.source = instance.parent;
        Base::StringView bindingPath = binding.path.View();
        constexpr Base::StringView parentPrefix("TemplatedParent.");
        if (bindingPath.SizeBytes() > parentPrefix.SizeBytes() &&
            bindingPath.Substr(0U, parentPrefix.SizeBytes()) ==
                parentPrefix &&
            properties_->Types().IsDerivedFrom(
                instance.parent->RuntimeType(),
                FrameworkElement::StaticTypeId())) {
            auto* frameworkParent = static_cast<FrameworkElement*>(
                instance.parent);
            DependencyObject* outerParent =
                frameworkParent->GetTemplatedParent();
            if (outerParent != nullptr) {
                descriptor.source = outerParent;
                bindingPath = bindingPath.Substr(
                    parentPrefix.SizeBytes(),
                    bindingPath.SizeBytes() - parentPrefix.SizeBytes());
            }
        }
        descriptor.target = target;
        descriptor.targetProperty = binding.targetProperty;
        descriptor.path = bindingPath;
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        Base::Result<Data::BindingHandle> attached =
            bindings_->Attach(descriptor);
        if (!attached) return attached.GetStatus();
        Base::Result<void> tracked =
            instance.metadataBindings.PushBack(
                attached.Value());
        if (!tracked) {
            (void)bindings_->Detach(attached.Value());
            return tracked.GetStatus();
        }
    }
    return {};
}

void TemplateEngine::DetachMetadataBindings(
    Instance& instance) noexcept {
    if (bindings_ != nullptr) {
        for (Data::BindingHandle handle :
             instance.metadataBindings) {
            (void)bindings_->Detach(handle);
        }
    }
    instance.metadataBindings.Clear();
}

Base::Result<void> TemplateEngine::AttachDynamicResources(
    Instance& instance) noexcept {
    const auto declarations =
        Aero::Controls::TemplatePrivate::DynamicResources(*instance.plan);
    if (declarations.Empty()) return {};
    if (effectiveValues_ == nullptr || resources_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Template DynamicResource services are unavailable");
    }
    Base::Result<void> reserved =
        instance.dynamicResourceTargets.Reserve(declarations.Size());
    if (!reserved) return reserved.GetStatus();
    const Aero::ResourceDictionary* templateResources[] = {
        &instance.plan->GetResources()};
    for (const TemplateDynamicResourcePlan& declaration : declarations) {
        DependencyObject* target =
            FindTarget(instance, declaration.targetName.View());
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Template DynamicResource target name was not found");
        }
        bool tracked = false;
        for (DependencyObject* existing :
             instance.dynamicResourceTargets) {
            if (existing == target) {
                tracked = true;
                break;
            }
        }
        if (!tracked) {
            reserved = instance.dynamicResourceTargets.PushBack(target);
            if (!reserved) return reserved.GetStatus();
        }
        Base::Result<void> attached = Markup::DynamicResource::Attach(
            *effectiveValues_,
            {templateResources, 1U},
            resources_,
            *target,
            declaration.targetProperty,
            declaration.key.View());
        if (!attached) return attached.GetStatus();
    }
    return {};
}

void TemplateEngine::DetachDynamicResources(
    Instance& instance) noexcept {
    if (effectiveValues_ != nullptr) {
        for (DependencyObject* target :
             instance.dynamicResourceTargets) {
            if (target != nullptr) {
                static_cast<void>(effectiveValues_->DetachObject(*target));
            }
        }
    }
    instance.dynamicResourceTargets.Clear();
}

Base::Result<void> TemplateEngine::EvaluateTriggers(
    Instance& instance) noexcept {
    for (const TemplatePropertyTrigger& trigger :
         Aero::Controls::TemplatePrivate::Triggers(*instance.plan)) {
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
         Aero::Controls::TemplatePrivate::Triggers(*instance.plan)) {
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
            if (!MatchesTemplateCondition(
                    *source,
                    triggerCondition,
                    current.Value())) {
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

Base::Result<void> TemplateEngine::ClearProviders(
    Instance& instance) noexcept {
    for (const TemplateBindingPlan& binding :
         Aero::Controls::TemplatePrivate::Bindings(*instance.plan)) {
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
         Aero::Controls::TemplatePrivate::Triggers(*instance.plan)) {
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

Base::Result<void> TemplateEngine::ClearAt(
    std::uint32_t index) noexcept {
    Instance& instance = instances_[index];
    AeroGuiInternal::NotifyTemplateDetached(
        *instance.parent);
    DetachMetadataBindings(instance);
    DetachDynamicResources(instance);
    Unsubscribe(instance);
    Base::Result<void> providers = ClearProviders(instance);
    if (!providers) return providers.GetStatus();

    for (Aero::Controls::TemplatePart& part : instance.parts) {
        if (part.frameworkElement != nullptr) {
            Base::Result<void> cleared =
                AeroGuiInternal::SetTemplatedParent(*part.frameworkElement, nullptr);
            if (!cleared) return cleared.GetStatus();
        }
    }
    Base::Result<void> child = AeroGuiInternal::SetTemplateRoot(*instance.parent, nullptr);
    if (!child) return child.GetStatus();

    for (std::uint32_t projectionIndex = instance.projections.Size();
         projectionIndex > 0U; --projectionIndex) {
        Aero::Controls::TemplateContentProjection& projection =
            instance.projections[projectionIndex - 1U];
        if ((projection.presenter == nullptr &&
             projection.contentHost == nullptr) ||
            projection.content == nullptr) {
            continue;
        }
        Base::Result<void> projectedDetached =
            tree_->DetachVisual(projection.projectedMount);
        if (!projectedDetached) return projectedDetached.GetStatus();
        if (projection.presenter != nullptr) {
            projection.presenter->SetContent(nullptr);
        } else {
            projection.contentHost->SetContent(nullptr);
        }
        if (projection.detachedOriginalVisual &&
            projection.originalVisualParent != nullptr) {
            Base::Result<Aero::VisualAttachment> restored =
                tree_->AttachVisualChild(
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
        auto& mount =
            instance.parts[partIndex - 1U].mount;
        if (mount.IsAttached()) {
            Base::Result<void> detached =
                tree_->DetachElement(mount);
            if (!detached) return detached.GetStatus();
        }
    }
    for (Aero::Controls::TemplatePart& part : instance.parts) {
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

void TemplateEngine::OnPropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    for (Instance& instance : instances_) {
        const bool parentChanged = instance.parent == &object;
        if (parentChanged) {
            (void)ApplyBindings(instance, args.GetProperty());
        }
        bool triggerChanged = false;
        for (const TemplatePropertyTrigger& trigger :
             Aero::Controls::TemplatePrivate::Triggers(*instance.plan)) {
            for (const TemplateTriggerCondition& triggerCondition :
                 trigger.conditions) {
                DependencyObject* source = FindTarget(
                    instance, triggerCondition.sourceName.View());
                if (source == &object &&
                    triggerCondition.property == args.GetProperty()) {
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

} // namespace Aero::Controls
