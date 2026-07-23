#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

// Provides a deterministic, RTTI-free type token for `{x:Type Name}`. The
// token is a registered value type so consumers can distinguish it from an
// arbitrary unsigned integer in a XAML attribute.
class AERO_API XamlTypeExtension final {
public:
    explicit XamlTypeExtension(Core::TypeId typeReferenceType) noexcept
        : typeReferenceType_(typeReferenceType) {}

    XamlTypeExtension(const XamlTypeExtension&) = delete;
    XamlTypeExtension& operator=(const XamlTypeExtension&) = delete;

    Base::Result<void> Register(
        XamlSchemaContext& schema,
        Core::TypeId markupExtensionType) noexcept;

    void SetTypeReferenceType(Core::TypeId type) noexcept {
        typeReferenceType_ = type;
    }

private:
    Core::TypeId typeReferenceType_ = Core::InvalidTypeId;

    static Base::Result<XamlValue> ProvideValue(
        Base::StringView arguments,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
