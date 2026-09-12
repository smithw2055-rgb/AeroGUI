#pragma once

#include <Aero/Controls/ContextMenu.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ContextMenuService
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ContextMenuService, Base::Object)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Ref<ContextMenu> GetContextMenu(
        const DependencyObject& target) noexcept;
    static void SetContextMenu(DependencyObject& target, Ref<ContextMenu> value) noexcept;

    AERO_ATTACHED_PROPERTY(Ref<ContextMenu>, ContextMenu);
};
} // namespace Aero::Controls
