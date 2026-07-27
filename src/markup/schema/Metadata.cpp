#include <Aero/Markup/Schema/Metadata.hpp>

#include <Aero/Core/Metadata/MetadataDsl.hpp>
#include <Aero/Presentation/Style.hpp>

#include "../resources/XamlTemplateCompiler.hpp"

namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;
using namespace Aero::Presentation;

Base::Result<Value> StringValue(Base::StringView value) noexcept {
    return Value::TryFromString(TypeOf<Base::String>(), value);
}

Base::Result<Value> GetGroupName(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const XamlVisualStateGroupObject&>(object).Name());
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
    return static_cast<XamlVisualStateGroupObject&>(object).SetName(
        value.AsString());
}

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

Base::Result<Value> GetStateName(
    const Base::Object& object,
    void*) noexcept {
    return StringValue(
        static_cast<const XamlVisualStateObject&>(object).Name());
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
    return static_cast<XamlVisualStateObject&>(object).SetName(
        value.AsString());
}

Base::Result<void> AddStateSetter(
    Base::Object& object,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value || value->RuntimeType() != Setter::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VisualState Setters expects Setter");
    }
    return static_cast<XamlVisualStateObject&>(object).AddSetter(value);
}

Base::Result<void> ClearStateSetters(
    Base::Object& object,
    void*) noexcept {
    static_cast<XamlVisualStateObject&>(object).ClearSetters();
    return {};
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
    Base::Result<TypeId> dynamicResource =
        context.Types().TryRegisterType(
            TypeRegistration::Object(
                Base::StringView("urn:aero"),
                Base::StringView("DynamicResource"),
                TypeOf<Base::Object>(),
                TypeFlags::MarkupExtension |
                    TypeFlags::Sealed));
    if (!dynamicResource) {
        return dynamicResource.GetStatus();
    }
    Base::Result<void> status;

    MetaTypeBuilder<XamlVisualStateGroupObject> stateGroup =
        MetaTypeBuilder<XamlVisualStateGroupObject>::Object(context);
    stateGroup
        .Property(OrdinaryProperty(
            "Name",
            TypeOf<Base::String>(),
            &GetGroupName,
            &SetGroupName))
        .Content<XamlVisualStateObject>(
            "States",
            ContentKind::Collection,
            &AddGroupState,
            &ClearGroupStates)
        .DefaultFactory();
    status = stateGroup.Finish();
    if (!status) return status.GetStatus();

    MetaTypeBuilder<XamlVisualStateObject> state =
        MetaTypeBuilder<XamlVisualStateObject>::Object(context);
    state
        .Property(OrdinaryProperty(
            "Name",
            TypeOf<Base::String>(),
            &GetStateName,
            &SetStateName))
        .Content<Setter>(
            "Setters",
            ContentKind::Collection,
            &AddStateSetter,
            &ClearStateSetters)
        .DefaultFactory();
    status = state.Finish();
    return status;
}

} // namespace Aero::Markup::Detail
