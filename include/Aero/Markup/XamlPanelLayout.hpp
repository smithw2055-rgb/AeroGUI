#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>

namespace Aero::Markup {

using XamlAsPanelLayoutElementCallback = Core::LayoutElement* (*)(
    Base::Object& object, void* context) noexcept;

struct XamlPanelLayoutTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsPanelLayoutElementCallback cast = nullptr;
    void* context = nullptr;
};

// Stages Canvas.Left/Top and Grid.Row/Column attached values while XAML is
// constructed, then applies them once XamlVisualTreeHost has attached the
// child to its parent container.
class [[deprecated("Use attached dependency properties")]] AERO_API XamlPanelLayoutExtension final {
public:
    XamlPanelLayoutExtension(
        Core::TypeId canvasType, Core::TypeId gridType) noexcept;

    Base::Result<void> TryRegisterType(
        const XamlPanelLayoutTypeRegistration& registration) noexcept;
    Base::Result<std::uint32_t> Register(
        XamlSchemaContext& schema) noexcept;

    static Base::Result<void> ConfigureCanvasChild(
        Base::Object& parentObject, Core::LayoutElement& parent,
        Core::LayoutElement& child, void* context) noexcept;
    static Base::Result<void> ConfigureGridChild(
        Base::Object& parentObject, Core::LayoutElement& parent,
        Core::LayoutElement& child, void* context) noexcept;

private:
    struct Values final {
        Core::LayoutElement* child = nullptr;
        double left = 0.0;
        double top = 0.0;
        std::uint32_t row = 0U;
        std::uint32_t column = 0U;
        bool hasLeft = false;
        bool hasTop = false;
        bool hasRow = false;
        bool hasColumn = false;
    };

    XamlSchemaContext* schema_ = nullptr;
    Base::Vector<XamlPanelLayoutTypeRegistration> types_;
    Base::Vector<Values> values_;
    Core::TypeId canvasType_ = Core::InvalidTypeId;
    Core::TypeId gridType_ = Core::InvalidTypeId;
    Core::MemberId canvasLeftMember_ = Core::InvalidMemberId;
    Core::MemberId canvasTopMember_ = Core::InvalidMemberId;
    Core::MemberId gridRowMember_ = Core::InvalidMemberId;
    Core::MemberId gridColumnMember_ = Core::InvalidMemberId;

    const XamlPanelLayoutTypeRegistration* FindType(
        Core::TypeId type) const noexcept;
    Base::Result<Core::LayoutElement*> ResolveElement(
        Base::Object& object, const XamlServiceProvider& services) const noexcept;
    Base::Result<Values*> FindOrCreate(
        Core::LayoutElement& child) noexcept;
    Values* FindValues(Core::LayoutElement& child) noexcept;
    void RemoveValues(Core::LayoutElement& child) noexcept;

    static Base::Result<void> SetAttachedMember(
        Base::Object& object, const XamlValue& value,
        const XamlServiceProvider& services, void* context) noexcept;
};

} // namespace Aero::Markup
