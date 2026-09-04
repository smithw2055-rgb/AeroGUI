#pragma once

#include <Aero/Controls/ContextMenu.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ContextMenuService
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ContextMenuService, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Ref<ContextMenu> GetContextMenu(
        const DependencyObject& target) noexcept;
    static void SetContextMenu(
        DependencyObject& target,
        Ref<ContextMenu> value) noexcept;

    inline static constexpr AttachedProperty<Ref<ContextMenu>> ContextMenuProperty{"ContextMenu"};
};
} // namespace Aero::Controls
