#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

#include <cstdint>

namespace Aero::Markup {

using XamlAsDependencyObjectCallback = Core::DependencyObject* (*)(
    Base::Object& object,
    void* context) noexcept;

struct XamlDependencyObjectTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsDependencyObjectCallback cast = nullptr;
    void* context = nullptr;
};

class AERO_API XamlDependencyPropertyBridge final {
public:
    XamlDependencyPropertyBridge(
        XamlSchemaContext& schema,
        Core::DependencyPropertyRegistry& properties,
        Base::IAllocator* allocator = nullptr) noexcept;

    XamlDependencyPropertyBridge(const XamlDependencyPropertyBridge&) = delete;
    XamlDependencyPropertyBridge& operator=(
        const XamlDependencyPropertyBridge&) = delete;

    AERO_NODISCARD Base::Result<void> TryRegisterType(
        const XamlDependencyObjectTypeRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> TryRegisterProperties() noexcept;

    AERO_NODISCARD bool IsTypeRegistered(Core::TypeId type) const noexcept;
    AERO_NODISCARD std::uint32_t RegisteredTypeCount() const noexcept {
        return types_.Size();
    }
    AERO_NODISCARD std::uint32_t RegisteredPropertyCount() const noexcept {
        return propertiesByMember_.Size();
    }

private:
    struct PropertyBinding final {
        Core::MemberId member = Core::InvalidMemberId;
        Core::DependencyPropertyHandle property;
    };

    XamlSchemaContext* schema_ = nullptr;
    Core::DependencyPropertyRegistry* properties_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<XamlDependencyObjectTypeRegistration> types_;
    Base::Vector<PropertyBinding> propertiesByMember_;

    AERO_NODISCARD const XamlDependencyObjectTypeRegistration*
    FindTypeRegistration(Core::TypeId type) const noexcept;
    AERO_NODISCARD const PropertyBinding* FindPropertyBinding(
        Core::MemberId member) const noexcept;
    AERO_NODISCARD Base::Result<Core::PropertyValue> ConvertValue(
        const XamlValue& value,
        const Core::DependencyProperty& property) const noexcept;

    AERO_NODISCARD static Base::Result<void> SetDependencyProperty(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
