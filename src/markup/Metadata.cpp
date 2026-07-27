#include <Aero/Markup/Schema.hpp>

// Markup-specific metadata declarations.

#include <Aero/Core/Metadata/Describe.hpp>
#include <Aero/Presentation/Style.hpp>

#include "TemplateCompiler.hpp"

namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;
using namespace Aero::Presentation;

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

} // namespace

Base::Result<void> PopulateMarkupMetadata(
    MetadataContext& context) noexcept {
    Base::Result<void> status =
        Describe<DynamicResourceExtensionToken>(
            context,
            TypeFlags::MarkupExtension |
                TypeFlags::Sealed).Result();
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
        .Factory();
    status = stateGroup.Result();
    if (!status) return status.GetStatus();

    auto state = Describe<XamlVisualStateObject>(context);
    state
        .Property(
            "Name",
            &XamlVisualStateObject::Name,
            &XamlVisualStateObject::SetName)
        .Content<Setter>(
            "Setters",
            ContentKind::Collection,
            &AddStateSetter,
            &ClearStateSetters)
        .Factory();
    status = state.Result();
    return status;
}

} // namespace Aero::Markup::Detail
