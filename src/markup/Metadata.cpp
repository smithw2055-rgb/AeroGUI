#include <Aero/Markup/Metadata.hpp>

#include <Aero/Core/Metadata/MetadataDsl.hpp>

#include "XamlThemeObjectModel.hpp"

namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;

Base::Result<Value> StringValue(Base::StringView value) noexcept {
    return Value::TryFromString(TypeOf<Base::String>(), value);
}

Base::Result<Value> GetDictionaryVariant(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const ResourceDictionaryObject&>(object).Variant());
}

Base::Result<void> SetDictionaryVariant(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ResourceDictionary Variant expects a string");
    }
    return static_cast<ResourceDictionaryObject&>(object).SetVariant(
        value.AsString());
}

Base::Result<void> AddDictionaryEntry(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            ThemeControlTemplateObject::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ResourceDictionary entry expects a ControlTemplate");
    }
    return static_cast<ResourceDictionaryObject&>(object).AddEntry(value);
}

Base::Result<void> ClearDictionaryEntries(
    Base::Object& object,
    void*) noexcept {
    static_cast<ResourceDictionaryObject&>(object).ClearEntries();
    return {};
}

Base::Result<Value> GetTemplateTargetType(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const ThemeControlTemplateObject&>(object).TargetType());
}

Base::Result<void> SetTemplateTargetType(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ControlTemplate TargetType expects a string");
    }
    return static_cast<ThemeControlTemplateObject&>(object).SetTargetType(
        value.AsString());
}

Base::Result<Value> GetTemplateVisualTree(
    const Base::Object& object,
    void*) noexcept {
    const Base::Ref<Base::Object>& value =
        static_cast<const ThemeControlTemplateObject&>(object).VisualTree();
    return Value::FromObject(TypeOf<Base::Object>(), value);
}

Base::Result<void> SetTemplateVisualTree(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::Object || value.IsNullObject()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ControlTemplate VisualTree expects an object");
    }
    return static_cast<ThemeControlTemplateObject&>(object).SetVisualTree(
        value.AsObject());
}

Base::Result<void> AddTemplateVisualStateGroup(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            ThemeVisualStateGroupObject::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ControlTemplate VisualStateGroups expects VisualStateGroup");
    }
    return static_cast<ThemeControlTemplateObject&>(object)
        .AddVisualStateGroup(value);
}

Base::Result<void> ClearTemplateVisualStateGroups(
    Base::Object& object,
    void*) noexcept {
    static_cast<ThemeControlTemplateObject&>(object)
        .ClearVisualStateGroups();
    return {};
}

Base::Result<Value> GetGroupName(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const ThemeVisualStateGroupObject&>(object).Name());
}

Base::Result<void> SetGroupName(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualStateGroup Name expects a string");
    }
    return static_cast<ThemeVisualStateGroupObject&>(object).SetName(
        value.AsString());
}

Base::Result<void> AddGroupState(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() !=
            ThemeVisualStateObject::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualStateGroup States expects VisualState");
    }
    return static_cast<ThemeVisualStateGroupObject&>(object).AddState(value);
}

Base::Result<void> ClearGroupStates(
    Base::Object& object,
    void*) noexcept {
    static_cast<ThemeVisualStateGroupObject&>(object).ClearStates();
    return {};
}

Base::Result<Value> GetStateName(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const ThemeVisualStateObject&>(object).Name());
}

Base::Result<void> SetStateName(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Name expects a string");
    }
    return static_cast<ThemeVisualStateObject&>(object).SetName(
        value.AsString());
}

Base::Result<void> AddStateSetter(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != ThemeSetterObject::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Setters expects Setter");
    }
    return static_cast<ThemeVisualStateObject&>(object).AddSetter(value);
}

Base::Result<void> ClearStateSetters(
    Base::Object& object,
    void*) noexcept {
    static_cast<ThemeVisualStateObject&>(object).ClearSetters();
    return {};
}

Base::Result<Value> GetSetterTargetName(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const ThemeSetterObject&>(object).TargetName());
}

Base::Result<void> SetSetterTargetName(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Setter TargetName expects a string");
    }
    return static_cast<ThemeSetterObject&>(object).SetTargetName(
        value.AsString());
}

Base::Result<Value> GetSetterProperty(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const ThemeSetterObject&>(object).Property());
}

Base::Result<void> SetSetterProperty(
    Base::Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Setter Property expects a string");
    }
    return static_cast<ThemeSetterObject&>(object).SetProperty(
        value.AsString());
}

PropertyRegistration OrdinaryProperty(
    Base::StringView name,
    TypeId type,
    PropertyGetCallback get,
    PropertySetCallback set,
    PropertyFlags flags = PropertyFlags::None) noexcept {
    PropertyRegistration registration;
    registration.name = name;
    registration.valueType = type;
    registration.flags = flags;
    registration.access = PropertyAccessKind::Ordinary;
    registration.get = get;
    registration.set = set;
    return registration;
}

} // namespace

Base::Result<void> PopulateMarkupMetadata(
    MetaRegistrationContext& context) noexcept {
    Base::Result<void> status;

    MetaTypeBuilder<ResourceDictionaryObject> dictionary =
        MetaTypeBuilder<ResourceDictionaryObject>::Object(context);
    dictionary
        .Property(OrdinaryProperty(
            "Variant",
            TypeOf<Base::String>(),
            &GetDictionaryVariant,
            &SetDictionaryVariant))
        .Content<ThemeControlTemplateObject>(
            "Entries",
            ContentKind::Collection,
            &AddDictionaryEntry,
            &ClearDictionaryEntries)
        .DefaultFactory();
    status = dictionary.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ThemeControlTemplateObject> controlTemplate =
        MetaTypeBuilder<ThemeControlTemplateObject>::Object(context);
    controlTemplate
        .Property(OrdinaryProperty(
            "TargetType",
            TypeOf<Base::String>(),
            &GetTemplateTargetType,
            &SetTemplateTargetType))
        .Property(OrdinaryProperty(
            "VisualTree",
            TypeOf<Base::Object>(),
            &GetTemplateVisualTree,
            &SetTemplateVisualTree,
            PropertyFlags::Structural))
        .Content<ThemeVisualStateGroupObject>(
            "VisualStateGroups",
            ContentKind::Collection,
            &AddTemplateVisualStateGroup,
            &ClearTemplateVisualStateGroups)
        .DefaultFactory();
    status = controlTemplate.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ThemeVisualStateGroupObject> stateGroup =
        MetaTypeBuilder<ThemeVisualStateGroupObject>::Object(context);
    stateGroup
        .Property(OrdinaryProperty(
            "Name",
            TypeOf<Base::String>(),
            &GetGroupName,
            &SetGroupName))
        .Content<ThemeVisualStateObject>(
            "States",
            ContentKind::Collection,
            &AddGroupState,
            &ClearGroupStates)
        .DefaultFactory();
    status = stateGroup.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ThemeVisualStateObject> state =
        MetaTypeBuilder<ThemeVisualStateObject>::Object(context);
    state
        .Property(OrdinaryProperty(
            "Name",
            TypeOf<Base::String>(),
            &GetStateName,
            &SetStateName))
        .Content<ThemeSetterObject>(
            "Setters",
            ContentKind::Collection,
            &AddStateSetter,
            &ClearStateSetters)
        .DefaultFactory();
    status = state.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<ThemeSetterObject> setter =
        MetaTypeBuilder<ThemeSetterObject>::Object(context);
    setter
        .Property(OrdinaryProperty(
            "TargetName",
            TypeOf<Base::String>(),
            &GetSetterTargetName,
            &SetSetterTargetName))
        .Property(OrdinaryProperty(
            "Property",
            TypeOf<Base::String>(),
            &GetSetterProperty,
            &SetSetterProperty))
        .Property({
            "Value",
            TypeOf<Base::String>(),
            PropertyFlags::None})
        .DefaultFactory();
    return setter.Finish();
}

} // namespace Aero::Markup::Detail
