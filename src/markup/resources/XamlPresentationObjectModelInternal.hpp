#pragma once

#include <Aero/Markup/Runtime/XamlActivation.hpp>
#include <Aero/Markup/Resources/XamlPresentationObjectModel.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Presentation/Style.hpp>

namespace Aero::Markup::Detail {

class XamlStyleSchemaFacet final {
public:
    explicit XamlStyleSchemaFacet(
        const XamlPresentationObjectModelOptions& options) noexcept;

    Base::Result<void> Register(
        XamlSchemaContext& schema,
        Core::ActivationProviderRegistry& activation,
        Core::TypeId styleType,
        Core::TypeId setterType,
        Core::DependencyPropertyHandle styleProperty,
        Core::TypeId triggerType) noexcept;
    void SetTypeReferenceType(Core::TypeId type) noexcept {
        options_.typeReferenceType = type;
    }

private:
    XamlPresentationObjectModelOptions options_;
    XamlSchemaContext* schema_ = nullptr;
    Core::TypeId styleType_ = Core::InvalidTypeId;
    Core::TypeId setterType_ = Core::InvalidTypeId;
    Core::TypeId triggerType_ = Core::InvalidTypeId;
    Core::DependencyPropertyHandle styleProperty_;
    Core::MemberId targetTypeMember_ = Core::InvalidMemberId;
    Core::MemberId basedOnMember_ = Core::InvalidMemberId;
    Core::MemberId settersMember_ = Core::InvalidMemberId;
    Core::MemberId triggersMember_ = Core::InvalidMemberId;
    Core::MemberId setterPropertyMember_ = Core::InvalidMemberId;
    Core::MemberId setterValueMember_ = Core::InvalidMemberId;
    Core::MemberId triggerPropertyMember_ = Core::InvalidMemberId;
    Core::MemberId triggerValueMember_ = Core::InvalidMemberId;
    Core::MemberId triggerSettersMember_ = Core::InvalidMemberId;

    Base::Result<void> FinalizeStyle(
        Presentation::Style& style) noexcept;
    Base::Result<Core::PropertyValue> ConvertValueForProperty(
        const XamlValue& value,
        Core::TypeId targetType,
        Base::StringView propertyName) const noexcept;

    static Base::Result<Base::Ref<Base::Object>> ActivateStyle(
        Core::TypeId requestedType,
        const XamlActivationContext& activation,
        void* context) noexcept;
    static Base::Result<Base::Ref<Base::Object>> ActivateSetter(
        Core::TypeId requestedType,
        const XamlActivationContext& activation,
        void* context) noexcept;
    static Base::Result<Base::Ref<Base::Object>> ActivateTrigger(
        Core::TypeId requestedType,
        const XamlActivationContext& activation,
        void* context) noexcept;
    static Base::Result<void> SetTargetType(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
    static Base::Result<void> SetBasedOn(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> AddSetter(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> AddTrigger(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> SetSetterProperty(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> SetSetterValue(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> SetTriggerProperty(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> SetTriggerValue(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> AddTriggerSetter(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> EndStyleInit(
        Base::Object& object,
        void* context) noexcept;
    static Base::Result<void> SetStyleMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

class XamlTemplateSchemaFacet final {
public:
    XamlTemplateSchemaFacet(
        Core::MetadataRuntime& runtime,
        Core::DependencyPropertyRegistry& properties,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlTemplateSchemaFacet() noexcept;

    XamlTemplateSchemaFacet(
        const XamlTemplateSchemaFacet&) = delete;
    XamlTemplateSchemaFacet& operator=(
        const XamlTemplateSchemaFacet&) = delete;

    Base::Result<void> Register(
        XamlSchemaContext& schema) noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Markup::Detail
