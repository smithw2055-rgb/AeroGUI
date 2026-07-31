#include <Aero/Markup/Schema.hpp>
#include "../ui/RuntimeManagers.hpp"

// Markup-specific metadata declarations.

#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Style.hpp>

#include "TemplateCompiler.hpp"
#include "StaticResourceObject.hpp"

namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;


class DynamicResourceExtensionToken final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        DynamicResourceExtensionToken,
        Base::Object,
        "urn:aero",
        "DynamicResource")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class StaticExtensionToken final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        StaticExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Static")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TypeExtensionToken final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TypeExtensionToken,
        Base::Object,
        "http://schemas.microsoft.com/winfx/2006/xaml",
        "Type")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class TemplateBindingExtensionToken final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TemplateBindingExtensionToken,
        Base::Object,
        "urn:aero",
        "TemplateBinding")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

Base::Result<void> AddGroupState(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            XamlVisualStateObject::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualStateGroup States expects VisualState");
    }
    return static_cast<XamlVisualStateGroupObject&>(object).AddState(value);
}

Base::Result<void> ClearGroupStates(
    Base::Object& object,
    void*) noexcept {
    static_cast<XamlVisualStateGroupObject&>(object).ClearStates();
    return {};
}

Base::Result<void> AddGroupTransition(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            XamlVisualTransitionObject::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualStateGroup Transitions expects VisualTransition");
    }
    return static_cast<XamlVisualStateGroupObject&>(
        object).AddTransition(value);
}

Base::Result<void> ClearGroupTransitions(
    Base::Object& object,
    void*) noexcept {
    static_cast<XamlVisualStateGroupObject&>(
        object).ClearTransitions();
    return {};
}

Base::Result<void> AddElementVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            XamlVisualStateGroupObject::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualStateGroups expects VisualStateGroup");
    }
    auto& target = static_cast<Core::DependencyObject&>(object);
    Base::Ref<Base::Object> valueStore = target.GetValueOr(
        XamlVisualStateManagerObject::
            VisualStateGroupStoreProperty,
        Base::Ref<Base::Object>{});
    if (!valueStore) {
        Base::Result<Base::Ref<XamlVisualStateGroupStore>> created =
            Base::MakeRef<XamlVisualStateGroupStore>();
        if (!created) return created.GetStatus();
        valueStore = Base::Ref<Base::Object>(
            std::move(created).Value());
        Base::Result<void> stored = target.SetValue(
            XamlVisualStateManagerObject::
                VisualStateGroupStoreProperty,
            valueStore);
        if (!stored) return stored.GetStatus();
    }
    if (valueStore->RuntimeType() !=
            XamlVisualStateGroupStore::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "VisualStateGroups store has an invalid value");
    }
    return static_cast<XamlVisualStateGroupStore&>(
        *valueStore).Add(value);
}

Base::Result<void> ClearElementVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    return static_cast<Core::DependencyObject&>(object).SetValue(
        XamlVisualStateManagerObject::
            VisualStateGroupStoreProperty,
        Base::Ref<Base::Object>{});
}

Base::Result<void> AddStateContent(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState content is null");
    }
    auto& state =
        static_cast<XamlVisualStateObject&>(object);
    if (value->RuntimeType() == Setter::StaticTypeId()) {
        return state.AddSetter(value);
    }
    if (value->RuntimeType() ==
        Media::Animation::Storyboard::StaticTypeId()) {
        return state.SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(value.Get())));
    }
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "VisualState content expects Setter or Storyboard");
}

Base::Result<void> ClearStateContent(
    Base::Object& object,
    void*) noexcept {
    auto& state =
        static_cast<XamlVisualStateObject&>(object);
    state.ClearSetters();
    static_cast<void>(state.SetStoryboard({}));
    return {};
}

Base::Result<void> SetTransitionStoryboard(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            Media::Animation::Storyboard::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualTransition content expects Storyboard");
    }
    return static_cast<XamlVisualTransitionObject&>(
        object).SetStoryboard(
            Base::Ref<Media::Animation::Storyboard>::FromBorrowed(
                *static_cast<Media::Animation::Storyboard*>(
                    value.Get())));
}

Base::Result<void> ClearTransitionStoryboard(
    Base::Object& object,
    void*) noexcept {
    return static_cast<XamlVisualTransitionObject&>(
        object).SetStoryboard({});
}

} // namespace

Base::Result<void> PopulateMarkupMetadata(
    MetadataContext& context) noexcept {
    Base::Result<void> status =
        Describe<DynamicResourceExtensionToken>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Describe<StaticExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Describe<TypeExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Describe<TemplateBindingExtensionToken>(
        context,
        TypeFlags::MarkupExtension |
            TypeFlags::Sealed).Result();
    if (!status) return status.GetStatus();
    status = Describe<StaticResourceObject>(context)
        .Property(
            StaticResourceObject::ResourceKeyProperty,
            PropertyOptions(Base::String{}))
        .Factory()
        .Result();
    if (!status) return status.GetStatus();

    auto visualStateManager =
        Describe<XamlVisualStateManagerObject>(
            context, TypeFlags::Abstract);
    visualStateManager
        .Property(
            XamlVisualStateManagerObject::
                VisualStateGroupStoreProperty,
            PropertyOptions(Base::Ref<Base::Object>{}))
        .Collection<Base::Object>(
            "VisualStateGroups",
            &AddElementVisualStateGroup,
            &ClearElementVisualStateGroups,
            PropertyFlags::Structural |
                PropertyFlags::Attached)
        .Factory();
    status = visualStateManager.Result();
    if (!status) return status.GetStatus();

    auto stateGroup =
        Describe<XamlVisualStateGroupObject>(context);
    stateGroup
        .Property(
            "Name",
            &XamlVisualStateGroupObject::Name,
            &XamlVisualStateGroupObject::SetName)
        .Content<XamlVisualStateObject>(
            "States",
            ContentKind::Collection,
            &AddGroupState,
            &ClearGroupStates)
        .Collection<XamlVisualTransitionObject>(
            "Transitions",
            &AddGroupTransition,
            &ClearGroupTransitions)
        .Factory();
    status = stateGroup.Result();
    if (!status) return status.GetStatus();

    auto state = Describe<XamlVisualStateObject>(context);
    state
        .Property(
            "Name",
            &XamlVisualStateObject::Name,
            &XamlVisualStateObject::SetName)
        .Content<Base::Object>(
            "Content",
            ContentKind::Collection,
            &AddStateContent,
            &ClearStateContent)
        .Factory();
    status = state.Result();
    if (!status) return status.GetStatus();

    auto transition =
        Describe<XamlVisualTransitionObject>(context);
    transition
        .Property(
            "From",
            &XamlVisualTransitionObject::From,
            &XamlVisualTransitionObject::SetFrom)
        .Property(
            "To",
            &XamlVisualTransitionObject::To,
            &XamlVisualTransitionObject::SetTo)
        .Property(
            "GeneratedDuration",
            &XamlVisualTransitionObject::GeneratedDuration,
            &XamlVisualTransitionObject::SetGeneratedDuration)
        .Property<
            Base::Ref<Media::Animation::EasingFunctionBase>,
            &XamlVisualTransitionObject::GeneratedEasingFunction,
            &XamlVisualTransitionObject::SetGeneratedEasingFunction>(
            "GeneratedEasingFunction",
            PropertyFlags::Structural)
        .Content<Media::Animation::Storyboard>(
            "Storyboard",
            ContentKind::Single,
            &SetTransitionStoryboard,
            &ClearTransitionStoryboard)
        .Factory();
    status = transition.Result();
    return status;
}

} // namespace Aero::Markup::Detail
