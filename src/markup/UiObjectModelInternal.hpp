#pragma once

#include "UiObjectModel.hpp"
#include <Aero/Markup/Schema.hpp>
#include <Aero/Styling.hpp>

namespace Aero::Markup::Detail {

class XamlStyleSchemaFacet final {
public:
    explicit XamlStyleSchemaFacet(
        const UiObjectModelOptions& options) noexcept;

    Base::Result<void> Register(
        Schema& schema,
        Core::TypeId styleType,
        Core::TypeId setterType,
        Core::DependencyPropertyHandle styleProperty,
        Core::TypeId triggerType) noexcept;
private:
    UiObjectModelOptions options_;
    Schema* schema_ = nullptr;
    Core::TypeId styleType_ = Core::InvalidTypeId;
    Core::TypeId setterType_ = Core::InvalidTypeId;
    Core::TypeId triggerType_ = Core::InvalidTypeId;

    Base::Result<void> FinalizeStyle(
        Aero::Style& style) noexcept;
    Base::Result<Core::PropertyValue> ConvertValueForProperty(
        const Core::Value& value,
        Core::TypeId targetType,
        Base::StringView propertyName) const noexcept;

    static Base::Result<void> EndStyleInit(
        Base::Object& object,
        void* context) noexcept;
};

class XamlTemplateSchemaFacet final {
public:
    XamlTemplateSchemaFacet(
        ::Aero::Meta::Registry& metadata,
        Core::DependencyPropertyRegistry& properties,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlTemplateSchemaFacet() noexcept;

    XamlTemplateSchemaFacet(
        const XamlTemplateSchemaFacet&) = delete;
    XamlTemplateSchemaFacet& operator=(
        const XamlTemplateSchemaFacet&) = delete;

    Base::Result<void> Register(
        Schema& schema) noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Markup::Detail
