#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

using XamlAsBorderCallback = Core::Border* (*)(
    Base::Object& object, void* context) noexcept;

struct XamlBorderTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsBorderCallback cast = nullptr;
    void* context = nullptr;
};

// Writes the core Border Background property from #RRGGBB or #AARRGGBB XAML
// color text. Stroke and brush/resource indirection remain later M2 work.
class [[deprecated("Use XamlDependencyPropertyBridge")]] AERO_API XamlBorderExtension final {
public:
    explicit XamlBorderExtension(
        Base::IAllocator* allocator = nullptr) noexcept;

    XamlBorderExtension(const XamlBorderExtension&) = delete;
    XamlBorderExtension& operator=(const XamlBorderExtension&) = delete;

    AERO_NODISCARD Base::Result<void> TryRegisterType(
        const XamlBorderTypeRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> Register(
        XamlSchemaContext& schema) noexcept;

private:
    XamlSchemaContext* schema_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<XamlBorderTypeRegistration> types_;
    Base::Vector<Core::MemberId> backgroundMembers_;

    AERO_NODISCARD const XamlBorderTypeRegistration* FindTypeRegistration(
        Core::TypeId type) const noexcept;
    AERO_NODISCARD Base::Result<Core::Border*> ResolveBorder(
        Base::Object& object,
        const XamlServiceProvider& services) const noexcept;
    AERO_NODISCARD bool IsBackgroundMember(Core::MemberId member) const noexcept;

    static AERO_NODISCARD Base::Result<void> SetBackgroundMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
