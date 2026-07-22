#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Layout.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

using XamlAsLayoutElementCallback = Core::LayoutElement* (*)(
    Base::Object& object, void* context) noexcept;

struct XamlLayoutTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsLayoutElementCallback cast = nullptr;
    void* context = nullptr;
};

// Bridges the first FrameworkElement layout contract into XAML. Numeric
// members use the registered Double scalar type; Margin and alignment members
// use a registered String scalar and are parsed by this bridge.
class AERO_API XamlLayoutExtension final {
public:
    explicit XamlLayoutExtension(
        Core::TypeId layoutElementType,
        Base::IAllocator* allocator = nullptr) noexcept;

    XamlLayoutExtension(const XamlLayoutExtension&) = delete;
    XamlLayoutExtension& operator=(const XamlLayoutExtension&) = delete;

    AERO_NODISCARD Base::Result<void> TryRegisterType(
        const XamlLayoutTypeRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> Register(
        XamlSchemaContext& schema) noexcept;

    void SetLayoutElementType(Core::TypeId type) noexcept {
        if (schema_ == nullptr) layoutElementType_ = type;
    }

private:
    XamlSchemaContext* schema_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<XamlLayoutTypeRegistration> types_;
    Core::TypeId layoutElementType_ = Core::InvalidTypeId;
    Core::MemberId widthMember_ = Core::InvalidMemberId;
    Core::MemberId heightMember_ = Core::InvalidMemberId;
    Core::MemberId minWidthMember_ = Core::InvalidMemberId;
    Core::MemberId maxWidthMember_ = Core::InvalidMemberId;
    Core::MemberId minHeightMember_ = Core::InvalidMemberId;
    Core::MemberId maxHeightMember_ = Core::InvalidMemberId;
    Core::MemberId marginMember_ = Core::InvalidMemberId;
    Core::MemberId horizontalAlignmentMember_ = Core::InvalidMemberId;
    Core::MemberId verticalAlignmentMember_ = Core::InvalidMemberId;

    AERO_NODISCARD const XamlLayoutTypeRegistration* FindTypeRegistration(
        Core::TypeId type) const noexcept;
    AERO_NODISCARD Base::Result<Core::LayoutElement*> ResolveElement(
        Base::Object& object,
        const XamlServiceProvider& services) const noexcept;

    static AERO_NODISCARD Base::Result<void> SetLayoutMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
