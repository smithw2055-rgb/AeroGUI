#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/HierarchicalDataTemplate.hpp>
#include <Aero/Controls/ItemsPanelTemplate.hpp>
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
    if (value.Kind() != ValueKind::Object ||
        value.IsNullObject()) {
        return false;
    }
    if (value.Type() == Data::Binding::StaticTypeId()) {
        return true;
    }
    return value.AsObject() &&
        value.AsObject()->RuntimeType() ==
            Data::Binding::StaticTypeId();
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
    const Registry* metadata,
    const DependencyProperty& target,
    const PropertyValue& value) noexcept {
    if (value.IsUnset()) {
        return PropertyValue{};
    }
    if (target.AcceptsAnyValue() ||
        value.Type() == target.ValueType()) {
        return value;
    }
    if (value.Kind() == PropertyValueKind::Object &&
        value.IsNullObject()) {
        if (target.ValueType() == Meta::TypeOf<Base::String>()) {
            return Meta::ValueCodec<Base::String>::Encode(Base::String{});
        }
        return PropertyValue::NullObject(target.ValueType());
    }
    if (value.Kind() == PropertyValueKind::Object &&
        !value.IsNullObject() && value.AsObject()) {
        const TypeId objectType = value.AsObject()->RuntimeType();
        if (types.IsAssignableFrom(target.ValueType(), objectType) ||
            types.IsDerivedFrom(objectType, target.ValueType()) ||
            objectType == target.ValueType()) {
            return PropertyValue::FromObject(
                target.ValueType(),
                Base::Ref<Base::Object>::FromBorrowed(
                    *value.AsObject()));
        }
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
    if (target.ValueType() == Meta::TypeOf<double>() &&
        value.Type() == Meta::TypeOf<Aero::Length>()) {
        Base::Result<Aero::Length> length =
            Meta::ValueCodec<Aero::Length>::Decode(value);
        if (!length) return length.GetStatus();
        return Meta::ValueCodec<double>::Encode(
            length.Value().isAuto ? 0.0 : length.Value().value);
    }
    if (value.Kind() == PropertyValueKind::String &&
        metadata != nullptr) {
        Base::Result<PropertyValue> converted =
            metadata->TryConvertText(
                target.ValueType(), value.AsString());
        if (converted) {
            return converted;
        }
    }
    if (target.ValueType() == Meta::TypeOf<Base::String>()) {
        if (value.Kind() == PropertyValueKind::String) {
            Base::String text;
            Base::Result<void> assigned = text.Assign(value.AsString());
            if (!assigned) return assigned.GetStatus();
            return Meta::ValueCodec<Base::String>::Encode(text);
        }
        return Meta::ValueCodec<Base::String>::Encode(Base::String{});
    }
    // WPF TemplateBinding conversion failures leave the target at its
    // default; they do not abort template apply.
    return PropertyValue{};
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
        owner.Get() != &root || ::Aero::TryCast<::Aero::UIElement>(&(root)) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Template root registration is invalid");
    }

    Base::Result<Aero::ElementAttachment> mounted =
        state.tree->AttachElement(*state.parent, root);
    if (!mounted) return mounted.GetStatus();
    Aero::ElementAttachment mount = std::move(mounted).Value();

    Base::Result<void> selected =
        AeroGuiInternal::SetTemplateRoot(*state.parent, ::Aero::TryCast<::Aero::UIElement>(&(root)));
    if (!selected) {
        (void)state.tree->DetachElement(mount);
        return selected.GetStatus();
    }
    if (::Aero::TryCast<::Aero::FrameworkElement>(&(root)) != nullptr) {
        Base::Result<void> templated =
            AeroGuiInternal::SetTemplatedParent(
                *::Aero::TryCast<::Aero::FrameworkElement>(&root), state.parent);
        if (!templated) {
            (void)AeroGuiInternal::SetTemplateRoot(*state.parent, nullptr);
            (void)state.tree->DetachElement(mount);
            return templated.GetStatus();
        }
    }
    Base::Result<void> added = AddOwnedPart(
        name, std::move(owner), root, &mount);
    if (!added) {
        if (::Aero::TryCast<::Aero::FrameworkElement>(&(root)) != nullptr) {
            (void)AeroGuiInternal::SetTemplatedParent(
                *::Aero::TryCast<::Aero::FrameworkElement>(&root), nullptr);
        }
        (void)AeroGuiInternal::SetTemplateRoot(*state.parent, nullptr);
        (void)state.tree->DetachElement(mount);
        return added.GetStatus();
    }
    state.rootVisual = &root;
    state.rootElement = ::Aero::TryCast<::Aero::UIElement>(&(root));
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

    if (::Aero::TryCast<::Aero::FrameworkElement>(&(part)) != nullptr) {
        Base::Result<void> templated =
            AeroGuiInternal::SetTemplatedParent(
                *::Aero::TryCast<::Aero::FrameworkElement>(&part), state.parent);
        if (!templated) {
            (void)state.tree->DetachElement(mount);
            return templated.GetStatus();
        }
    }
    Base::Result<void> added = AddOwnedPart(
        name, std::move(owner), part, &mount);
    if (!added) {
        if (::Aero::TryCast<::Aero::FrameworkElement>(&(part)) != nullptr) {
            (void)AeroGuiInternal::SetTemplatedParent(
                *::Aero::TryCast<::Aero::FrameworkElement>(&part), nullptr);
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

    const ::Aero::Media::Visual* ancestor = &presenterVisual;
    while (ancestor != nullptr) {
        if (ancestor == content) {
            // Content is the templated visual tree (or otherwise already
            // contains this presenter). Projecting it would cycle.
            return false;
        }
        ancestor = ancestor->GetVisualParent();
    }

    {
        const ::Aero::Media::Visual* hosted = content->GetVisualParent();
        while (hosted != nullptr) {
            if (hosted == &presenterVisual) {
                return true;
            }
            hosted = hosted->GetVisualParent();
        }
    }
    bool presenterInTemplate = false;
    for (const Aero::Controls::TemplatePart& part : state.parts) {
        presenterInTemplate =
            presenterInTemplate ||
            part.visual == &presenterVisual;
    }
    if (!presenterInTemplate) {
        const ::Aero::Media::Visual* walk = &presenterVisual;
        while (walk != nullptr) {
            if (walk == state.rootVisual || walk == state.parent) {
                presenterInTemplate = true;
                break;
            }
            walk = walk->GetVisualParent();
        }
    }
    if (!presenterInTemplate) {
        // Presenter is not part of this template. Skip rather than abort
        // ApplyViewUi (fragment content may already be hosted).
        return false;
    }
    // Content may already be a visual child of the owner (SetContent /
    // fragment attachEdges) or of another host (ScrollViewer.Content still
    // parented to the templated ScrollViewer). The projection path below
    // detaches and reparents onto the presenter — do not refuse just
    // because the current parent is not the owner.

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
            ::Aero::TryCast<::Aero::UIElement>(projection.originalVisualParent) != nullptr;
        original.renderAttached = state.renderer != nullptr &&
            ::Aero::TryCast<::Aero::FrameworkElement>(projection.originalVisualParent) != nullptr &&
            ::Aero::TryCast<::Aero::FrameworkElement>(content) != nullptr;
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
    auto& state = *static_cast<Aero::Controls::TemplateBuildState*>(state_);
    if (presenter.GetContentSource() == Base::StringView("Header") &&
        state.parent != nullptr) {
        const Value* header = nullptr;
        Value stored;
        if (state.parent->PropertyRegistry().Types().IsDerivedFrom(
                state.parent->RuntimeType(),
                HeaderedItemsControl::StaticTypeId())) {
            stored = static_cast<HeaderedItemsControl*>(state.parent)
                ->GetHeader();
            header = &stored;
        } else if (state.parent->PropertyRegistry().Types().IsDerivedFrom(
                       state.parent->RuntimeType(),
                       HeaderedContentControl::StaticTypeId())) {
            stored = static_cast<HeaderedContentControl*>(state.parent)
                ->GetHeader();
            header = &stored;
        }
        if (header != nullptr &&
            header->Kind() == ValueKind::Object &&
            !header->IsNullObject() &&
            header->AsObject() &&
            state.parent->PropertyRegistry().Types().IsDerivedFrom(
                header->AsObject()->RuntimeType(),
                UIElement::StaticTypeId())) {
            // Gallery SampleTemplate StackPanel already lives on Header.
            // A dummy TextBlock would occupy PART_Header's only layout slot
            // and prevent HostUiElement from attaching sibling leaf headers.
            return {};
        }
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
    part.frameworkElement = ::Aero::TryCast<::Aero::FrameworkElement>(&(visual));
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



Base::Result<void> DeferredObjectProgram::Configure(
    DeferredObjectFactory valueFactory,
    void* valueContext) noexcept {
    return Configure(valueFactory, valueContext, {});
}

Base::Result<void> DeferredObjectProgram::Configure(
    DeferredObjectFactory valueFactory,
    void* valueContext,
    Base::Ref<Base::Object> valueOwner) noexcept {
    if (sealed) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is sealed");
    }
    if (valueFactory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Deferred object factory is null");
    }
    factory = valueFactory;
    context = valueContext;
    factoryOwner = std::move(valueOwner);
    return {};
}

Base::Result<void> DeferredObjectProgram::SetBaseUri(
    const Base::ResourceUri& value) noexcept {
    if (sealed) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is sealed");
    }
    baseUri = value;
    return {};
}

Base::Result<void> DeferredObjectProgram::Seal() noexcept {
    if (factory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program has no factory");
    }
    sealed = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>> DeferredObjectProgram::Instantiate(
    const Base::Ref<Base::Object>& payload,
    Aero::BindingEngine* bindings) const noexcept {
    if (factory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Deferred object program is not ready");
    }
    Base::Result<Base::Ref<Base::Object>> result =
        factory(payload, context, bindings);
    if (!result || result.Value()) return result;
    return Base::Status::Failure(Base::ErrorCode::InvalidState,
        "Deferred object factory returned null");
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


using namespace Controls;

DataTemplate::DataTemplate() noexcept
    : state_(new (std::nothrow) Controls::DataTemplateState()) {
    if (state_ == nullptr) {
        Base::ReportOutOfMemory(sizeof(Controls::DataTemplateState), alignof(Controls::DataTemplateState), Base::MemoryTag::Ui);
    }
}

DataTemplate::~DataTemplate() noexcept {
    delete static_cast<Controls::DataTemplateState*>(state_);
    state_ = nullptr;
}

TypeId DataTemplate::GetDataType() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr ? state->dataType : InvalidTypeId;
}

void DataTemplate::SetDataType(TypeId value) noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state == nullptr) return;
    if (state->program.sealed || value == InvalidTypeId) {
        return;
    }
    state->dataType = value;
}

ResourceKey DataTemplate::GetImplicitKey() const noexcept {
    return ResourceKey::FromType(GetDataType());
}

Ref<Base::Object> HierarchicalDataTemplate::GetItemsSource() const noexcept {
    const Controls::DataTemplateState* state =
        static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr ? state->hierarchicalItemsSource
                            : Base::Ref<Base::Object>{};
}

void HierarchicalDataTemplate::SetItemsSource(
    Base::Ref<Base::Object> value) noexcept {
    Controls::DataTemplateState* state =
        static_cast<Controls::DataTemplateState*>(state_);
    if (state != nullptr) state->hierarchicalItemsSource = std::move(value);
}

Ref<Base::Object> HierarchicalDataTemplate::GetItemTemplate() const noexcept {
    const Controls::DataTemplateState* state =
        static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr ? state->hierarchicalItemTemplate
                            : Base::Ref<Base::Object>{};
}

void HierarchicalDataTemplate::SetItemTemplate(
    Base::Ref<Base::Object> value) noexcept {
    Controls::DataTemplateState* state =
        static_cast<Controls::DataTemplateState*>(state_);
    if (state != nullptr) state->hierarchicalItemTemplate = std::move(value);
}

ResourceDictionary& DataTemplate::GetResources() noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

const ResourceDictionary& DataTemplate::GetResources() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

bool DataTemplate::GetIsSealed() const noexcept {
    const Controls::DataTemplateState* state = static_cast<const Controls::DataTemplateState*>(state_);
    return state != nullptr && state->program.sealed;
}

::Aero::Controls::DataTemplateState* DataTemplateRuntime::State(DataTemplate& value) noexcept {
    return static_cast<Controls::DataTemplateState*>(value.state_);
}

const ::Aero::Controls::DataTemplateState* DataTemplateRuntime::State(const DataTemplate& value) noexcept {
    return static_cast<const Controls::DataTemplateState*>(value.state_);
}

Base::Result<void> DataTemplateRuntime::Configure(DataTemplate& value, Controls::DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> owner) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->program.Configure(factory, context, std::move(owner));
}

Base::Result<void> DataTemplateRuntime::SetBaseUri(DataTemplate& value, const Base::ResourceUri& uri) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->program.SetBaseUri(uri);
}

const Base::ResourceUri& DataTemplateRuntime::BaseUri(const DataTemplate& value) noexcept {
    static Base::ResourceUri empty;
    const Controls::DataTemplateState* state = State(value);
    return state != nullptr ? state->program.baseUri : empty;
}

Base::Result<void> DataTemplateRuntime::SetAuthoredVisualTree(DataTemplate& value, const Base::Ref<Base::Object>& tree) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    if (state->program.sealed || !tree) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate VisualTree assignment is invalid");
    state->authoredVisualTree = tree;
    return {};
}

void DataTemplateRuntime::ClearAuthoredVisualTree(DataTemplate& value) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state != nullptr) state->authoredVisualTree.Reset();
}

Base::Result<void> DataTemplateRuntime::AddAuthoredTrigger(DataTemplate& value, Base::Ref<Aero::TriggerBase> trigger) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    if (!trigger || state->program.factory != nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate Trigger cannot be added after sealing");
    return state->authoredTriggers.PushBack(std::move(trigger));
}

void DataTemplateRuntime::ClearAuthoredTriggers(DataTemplate& value) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state != nullptr) state->authoredTriggers.Clear();
}

Base::Span<const Base::Ref<Aero::TriggerBase>> DataTemplateRuntime::AuthoredTriggers(const DataTemplate& value) noexcept {
    const Controls::DataTemplateState* state = State(value);
    return state != nullptr ? Base::Span<const Base::Ref<Aero::TriggerBase>>(state->authoredTriggers.Data(), state->authoredTriggers.Size()) : Base::Span<const Base::Ref<Aero::TriggerBase>>{};
}

Base::Result<void> DataTemplateRuntime::RegisterAuthoredName(DataTemplate& value, Base::StringView name, Base::Object& object) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    return state->authoredNames.Register(name, object);
}

void DataTemplateRuntime::ClearAuthoredNames(DataTemplate& value) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state != nullptr) state->authoredNames.Clear();
}

const Aero::NameScope& DataTemplateRuntime::AuthoredNames(const DataTemplate& value) noexcept {
    static Aero::NameScope empty;
    const Controls::DataTemplateState* state = State(value);
    return state != nullptr ? state->authoredNames : empty;
}

const Base::Ref<Base::Object>& DataTemplateRuntime::AuthoredVisualTree(const DataTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const Controls::DataTemplateState* state = State(value);
    return state != nullptr ? state->authoredVisualTree : empty;
}

Base::Result<void> DataTemplateRuntime::Seal(DataTemplate& value) noexcept {
    Controls::DataTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "DataTemplate state allocation failed");
    Base::Result<void> program = state->program.Seal();
    if (!program) return program.GetStatus();
    return state->resources.Seal();
}

Base::Result<Base::Ref<Base::Object>> DataTemplateRuntime::Instantiate(
    const DataTemplate& value, const Base::Ref<Base::Object>& item,
    Aero::BindingEngine* bindings) noexcept {
    const Controls::DataTemplateState* state = State(value);
    if (state == nullptr || state->program.factory == nullptr || !item) return Base::Status::Failure(Base::ErrorCode::InvalidState, "DataTemplate is not ready");
    return state->program.Instantiate(item, bindings);
}

} // namespace Aero

namespace Aero::Controls {

ItemsPanelTemplate::ItemsPanelTemplate() noexcept
    : state_(new (std::nothrow) Controls::ItemsPanelTemplateState()) {
    if (state_ == nullptr) {
        Base::ReportOutOfMemory(sizeof(Controls::ItemsPanelTemplateState), alignof(Controls::ItemsPanelTemplateState), Base::MemoryTag::Ui);
    }
}

ItemsPanelTemplate::~ItemsPanelTemplate() noexcept {
    delete static_cast<Controls::ItemsPanelTemplateState*>(state_);
    state_ = nullptr;
}

ResourceDictionary& ItemsPanelTemplate::GetResources() noexcept {
    Controls::ItemsPanelTemplateState* state = static_cast<Controls::ItemsPanelTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

const ResourceDictionary& ItemsPanelTemplate::GetResources() const noexcept {
    const Controls::ItemsPanelTemplateState* state = static_cast<const Controls::ItemsPanelTemplateState*>(state_);
    if (state != nullptr) return state->resources;
    static ResourceDictionary fallback;
    return fallback;
}

bool ItemsPanelTemplate::GetIsSealed() const noexcept {
    const Controls::ItemsPanelTemplateState* state = static_cast<const Controls::ItemsPanelTemplateState*>(state_);
    return state != nullptr && state->program.sealed;
}

void ItemsPanelTemplate::SetResources(Base::Ref<ResourceDictionary> value) noexcept {
    Controls::ItemsPanelTemplateState* state = static_cast<Controls::ItemsPanelTemplateState*>(state_);
    if (state == nullptr) return;
    (void)Aero::AssignResourceDictionary(state->resources, std::move(value), "ItemsPanelTemplate Resources is already assigned");
}

ItemsPanelTemplateState* ItemsPanelTemplateRuntime::State(ItemsPanelTemplate& value) noexcept {
    return static_cast<Controls::ItemsPanelTemplateState*>(value.state_);
}

const ItemsPanelTemplateState* ItemsPanelTemplateRuntime::State(const ItemsPanelTemplate& value) noexcept {
    return static_cast<const Controls::ItemsPanelTemplateState*>(value.state_);
}

Base::Result<void> ItemsPanelTemplateRuntime::Configure(ItemsPanelTemplate& value, DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> owner) noexcept {
    Controls::ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    return state->program.Configure(factory, context, std::move(owner));
}

Base::Result<void> ItemsPanelTemplateRuntime::SetBaseUri(ItemsPanelTemplate& value, const Base::ResourceUri& uri) noexcept {
    Controls::ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    return state->program.SetBaseUri(uri);
}

const Base::ResourceUri& ItemsPanelTemplateRuntime::BaseUri(const ItemsPanelTemplate& value) noexcept {
    static Base::ResourceUri empty;
    const Controls::ItemsPanelTemplateState* state = State(value);
    return state != nullptr ? state->program.baseUri : empty;
}

Base::Result<void> ItemsPanelTemplateRuntime::SetAuthoredVisualTree(ItemsPanelTemplate& value, const Base::Ref<Base::Object>& tree) noexcept {
    Controls::ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    if (state->program.sealed || !tree) return Base::Status::Failure(Base::ErrorCode::InvalidState, "ItemsPanelTemplate VisualTree assignment is invalid");
    state->authoredVisualTree = tree;
    return {};
}

void ItemsPanelTemplateRuntime::ClearAuthoredVisualTree(ItemsPanelTemplate& value) noexcept {
    Controls::ItemsPanelTemplateState* state = State(value);
    if (state != nullptr) state->authoredVisualTree.Reset();
}

const Base::Ref<Base::Object>& ItemsPanelTemplateRuntime::AuthoredVisualTree(const ItemsPanelTemplate& value) noexcept {
    static Base::Ref<Base::Object> empty;
    const Controls::ItemsPanelTemplateState* state = State(value);
    return state != nullptr ? state->authoredVisualTree : empty;
}

Base::Result<void> ItemsPanelTemplateRuntime::Seal(ItemsPanelTemplate& value) noexcept {
    Controls::ItemsPanelTemplateState* state = State(value);
    if (state == nullptr) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "ItemsPanelTemplate state allocation failed");
    Base::Result<void> program = state->program.Seal();
    if (!program) return program.GetStatus();
    return state->resources.Seal();
}

Base::Result<Base::Ref<Base::Object>> ItemsPanelTemplateRuntime::Instantiate(const ItemsPanelTemplate& value) noexcept {
    const Controls::ItemsPanelTemplateState* state = State(value);
    if (state == nullptr || state->program.factory == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "ItemsPanelTemplate is not ready");
    return state->program.Instantiate();
}

::Aero::Controls::FrameworkTemplateState* TemplatePrivate::State(FrameworkTemplate& value) noexcept {
    return ::Aero::FrameworkTemplateRuntime::State(value);
}

const ::Aero::Controls::FrameworkTemplateState* TemplatePrivate::State(const FrameworkTemplate& value) noexcept {
    return ::Aero::FrameworkTemplateRuntime::State(value);
}

DataTemplateState* TemplatePrivate::State(DataTemplate& value) noexcept {
    return DataTemplateRuntime::State(value);
}

const DataTemplateState* TemplatePrivate::State(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::State(value);
}

ItemsPanelTemplateState* TemplatePrivate::State(
    ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::State(value);
}

const ItemsPanelTemplateState* TemplatePrivate::State(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::State(value);
}

Base::Result<void> TemplatePrivate::Configure(
    DataTemplate& value,
    DeferredObjectFactory factory,
    void* context,
    Base::Ref<Base::Object> owner) noexcept {
    return DataTemplateRuntime::Configure(
        value, factory, context, std::move(owner));
}

Base::Result<void> TemplatePrivate::Configure(
    ItemsPanelTemplate& value,
    DeferredObjectFactory factory,
    void* context,
    Base::Ref<Base::Object> owner) noexcept {
    return ItemsPanelTemplateRuntime::Configure(
        value, factory, context, std::move(owner));
}

Base::Result<void> TemplatePrivate::SetBaseUri(
    DataTemplate& value,
    const Base::ResourceUri& uri) noexcept {
    return DataTemplateRuntime::SetBaseUri(value, uri);
}

Base::Result<void> TemplatePrivate::SetBaseUri(
    ItemsPanelTemplate& value,
    const Base::ResourceUri& uri) noexcept {
    return ItemsPanelTemplateRuntime::SetBaseUri(value, uri);
}

const Base::ResourceUri& TemplatePrivate::BaseUri(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::BaseUri(value);
}

const Base::ResourceUri& TemplatePrivate::BaseUri(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::BaseUri(value);
}

Base::Result<void> TemplatePrivate::SetAuthoredVisualTree(
    DataTemplate& value,
    const Base::Ref<Base::Object>& tree) noexcept {
    return DataTemplateRuntime::SetAuthoredVisualTree(value, tree);
}

Base::Result<void> TemplatePrivate::SetAuthoredVisualTree(
    ItemsPanelTemplate& value,
    const Base::Ref<Base::Object>& tree) noexcept {
    return ItemsPanelTemplateRuntime::SetAuthoredVisualTree(value, tree);
}

void TemplatePrivate::ClearAuthoredVisualTree(
    DataTemplate& value) noexcept {
    DataTemplateRuntime::ClearAuthoredVisualTree(value);
}

void TemplatePrivate::ClearAuthoredVisualTree(
    ItemsPanelTemplate& value) noexcept {
    ItemsPanelTemplateRuntime::ClearAuthoredVisualTree(value);
}

Base::Result<void> TemplatePrivate::AddAuthoredTrigger(
    DataTemplate& value,
    Base::Ref<Aero::TriggerBase> trigger) noexcept {
    return DataTemplateRuntime::AddAuthoredTrigger(
        value, std::move(trigger));
}

void TemplatePrivate::ClearAuthoredTriggers(
    DataTemplate& value) noexcept {
    DataTemplateRuntime::ClearAuthoredTriggers(value);
}

Base::Span<const Base::Ref<Aero::TriggerBase>>
TemplatePrivate::AuthoredTriggers(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::AuthoredTriggers(value);
}

Base::Result<void> TemplatePrivate::RegisterAuthoredName(
    DataTemplate& value,
    Base::StringView name,
    Base::Object& object) noexcept {
    return DataTemplateRuntime::RegisterAuthoredName(
        value, name, object);
}

void TemplatePrivate::ClearAuthoredNames(
    DataTemplate& value) noexcept {
    DataTemplateRuntime::ClearAuthoredNames(value);
}

const Aero::NameScope& TemplatePrivate::AuthoredNames(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::AuthoredNames(value);
}

const Base::Ref<Base::Object>& TemplatePrivate::AuthoredVisualTree(
    const DataTemplate& value) noexcept {
    return DataTemplateRuntime::AuthoredVisualTree(value);
}

const Base::Ref<Base::Object>& TemplatePrivate::AuthoredVisualTree(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::AuthoredVisualTree(value);
}

Base::Result<void> TemplatePrivate::Seal(
    DataTemplate& value) noexcept {
    return DataTemplateRuntime::Seal(value);
}

Base::Result<void> TemplatePrivate::Seal(
    ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::Seal(value);
}

Base::Result<Base::Ref<Base::Object>> TemplatePrivate::Instantiate(
    const DataTemplate& value,
    const Base::Ref<Base::Object>& item,
    Aero::BindingEngine* bindings) noexcept {
    return DataTemplateRuntime::Instantiate(value, item, bindings);
}

Base::Result<Base::Ref<Base::Object>> TemplatePrivate::Instantiate(
    const ItemsPanelTemplate& value) noexcept {
    return ItemsPanelTemplateRuntime::Instantiate(value);
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
    Meta::UpdateSourceTrigger updateSourceTrigger,
    const Base::Ref<Data::IValueConverter>& converter,
    const Meta::PropertyValue& converterParameter) noexcept {
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
    binding.converter = converter;
    binding.converterParameter = converterParameter;
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
        // WPF allows GeneratedDuration="0" (instant) with no Storyboard.
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
    if (!value) {
        OnApplyTemplate();
        return GetTemplateRoot() != nullptr;
    }
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

void DataTemplate::SetResources(Base::Ref<ResourceDictionary> value) noexcept {
    Controls::DataTemplateState* state = static_cast<Controls::DataTemplateState*>(state_);
    if (state == nullptr) return;
    (void)Aero::AssignResourceDictionary(state->resources, std::move(value), "DataTemplate Resources is already assigned");
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
    const std::uint32_t newIndex = instances_.Size();
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
    static_cast<void>(controlToInstance_.Insert(stored.parent, newIndex));
    static_cast<void>(handleToInstance_.Insert(stored.handle.value, newIndex));
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
    if (!handle.IsValid()) return UINT32_MAX;
    const std::uint32_t* found = handleToInstance_.Find(handle.value);
    return found != nullptr ? *found : UINT32_MAX;
}

std::uint32_t TemplateEngine::FindInstance(
    const Control& control) const noexcept {
    const std::uint32_t* found = controlToInstance_.Find(&control);
    return found != nullptr ? *found : UINT32_MAX;
}

DependencyObject* TemplateEngine::FindTarget(
    const Instance& instance,
    Base::StringView name) const noexcept {
    if (name.Empty()) return instance.parent;
    for (const Aero::Controls::TemplatePart& part : instance.parts) {
        if (part.name.View() == name) return part.object;
    }
    Base::Object* named = instance.names.Find(name);
    if (named != nullptr &&
        properties_ != nullptr &&
        properties_->Types().IsDerivedFrom(
            named->RuntimeType(),
            DependencyObject::StaticTypeId())) {
        return static_cast<DependencyObject*>(named);
    }
    return nullptr;
}

bool TemplateEngine::HasTemplateBinding(
    DependencyObject& target,
    DependencyPropertyHandle property) const noexcept {
    for (const Instance& instance : instances_) {
        if (instance.plan == nullptr) continue;
        for (const TemplateBindingPlan& binding :
             Aero::Controls::TemplatePrivate::Bindings(*instance.plan)) {
            if (binding.targetProperty != property) continue;
            DependencyObject* found =
                FindTarget(instance, binding.targetName.View());
            if (found == &target) return true;
        }
    }
    return false;
}

Base::Result<void> TemplateEngine::RefreshTemplateBinding(
    DependencyObject& target,
    DependencyPropertyHandle property) noexcept {
    for (Instance& instance : instances_) {
        if (instance.plan == nullptr) continue;
        bool matches = false;
        for (const TemplateBindingPlan& binding :
             Aero::Controls::TemplatePrivate::Bindings(*instance.plan)) {
            if (binding.targetProperty != property) continue;
            DependencyObject* found =
                FindTarget(instance, binding.targetName.View());
            if (found == &target) {
                matches = true;
                break;
            }
        }
        if (matches) {
            return ApplyBindings(instance, property);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "TemplateBinding was not found");
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
                metadata_,
                *targetProperty, value.Value());
        if (!converted) return converted.GetStatus();
        if (converted.Value().IsUnset()) {
            continue;
        }
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
        descriptor.converterResource = binding.converter;
        descriptor.converterParameter = binding.converterParameter;
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
    Base::Result<std::uint32_t> flushed = values_->Flush();
    if (!flushed) return flushed.GetStatus();
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
        if (part.object != nullptr) {
            Base::Result<void> untracked = values_->DetachObject(*part.object);
            if (!untracked) return untracked.GetStatus();
        }
        if (part.visual != nullptr && tree_ != nullptr) {
            tree_->InvalidateNodeHandle(*part.visual);
        }
    }
    controlToInstance_.Erase(instance.parent);
    handleToInstance_.Erase(instance.handle.value);
    if (index + 1U != instances_.Size()) {
        instances_[index] = std::move(instances_[instances_.Size() - 1U]);
        static_cast<void>(controlToInstance_.Set(instances_[index].parent, index));
        static_cast<void>(handleToInstance_.Set(instances_[index].handle.value, index));
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
            // Triggers write parent properties (Button.Background). Re-apply
            // TemplateBindings so the template root Border picks up the new
            // value in the same change; otherwise hover/press visuals lag
            // until a later Background notification.
            (void)ApplyBindings(instance, DependencyPropertyHandle{});
        }
        if (parentChanged) {
            return;
        }
    }
}

} // namespace Aero::Controls
