#pragma once

#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

// Transitional semantic names for the current compact presentation hierarchy.
// They provide stable vocabulary to external code while Visual/UIElement/
// FrameworkElement responsibilities are separated internally in later runtime
// work without forcing custom controls to inherit concrete built-in controls.
using Visual = TreeNode;
using UIElement = LayoutElement;
using FrameworkElement = RenderElement;

class AERO_API Panel : public RenderElement {
    AERO_DECLARE_METADATA(Panel, RenderElement)
protected:
    explicit Panel(TypeId runtimeType) noexcept : RenderElement(runtimeType) {}
    ~Panel() override = default;
};

class AERO_API Decorator : public RenderElement {
    AERO_DECLARE_METADATA(Decorator, RenderElement)
protected:
    explicit Decorator(TypeId runtimeType) noexcept : RenderElement(runtimeType) {}
    ~Decorator() override = default;
};

class AERO_API Control : public RenderElement {
    AERO_DECLARE_METADATA(Control, RenderElement)
protected:
    explicit Control(TypeId runtimeType) noexcept : RenderElement(runtimeType) {}
    ~Control() override = default;
};

class AERO_API ContentControl : public Control {
    AERO_DECLARE_METADATA(ContentControl, Control)
protected:
    explicit ContentControl(TypeId runtimeType) noexcept : Control(runtimeType) {}
    ~ContentControl() override = default;
};

class AERO_API UserControl : public ContentControl {
    AERO_DECLARE_METADATA(UserControl, ContentControl)
protected:
    explicit UserControl(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~UserControl() override = default;
};

#define AERO_DETAIL_IMPLEMENT_INLINE_EMPTY_METADATA(classType, typeFlags) \
    inline Base::Result<void> classType::TryRegisterMetadata( \
        MetaRegistrationContext& context) noexcept { \
        MetaRegistrationBuilder helper( \
            context, StaticTypeId(), StaticMetadataNamespace(), \
            StaticMetadataName(), ParentClass::StaticTypeId(), typeFlags); \
        Base::Result<void> begun = helper.Begin(); \
        if (!begun) return begun.GetStatus(); \
        StaticFillMetadata(helper); \
        return helper.Finish(); \
    } \
    inline void classType::StaticFillMetadata( \
        MetaRegistrationBuilder& helper) noexcept { (void)helper; }

AERO_DETAIL_IMPLEMENT_INLINE_EMPTY_METADATA(Panel, TypeFlags::Abstract)
AERO_DETAIL_IMPLEMENT_INLINE_EMPTY_METADATA(Decorator, TypeFlags::Abstract)
AERO_DETAIL_IMPLEMENT_INLINE_EMPTY_METADATA(Control, TypeFlags::Abstract)
AERO_DETAIL_IMPLEMENT_INLINE_EMPTY_METADATA(ContentControl, TypeFlags::Abstract)
AERO_DETAIL_IMPLEMENT_INLINE_EMPTY_METADATA(UserControl, TypeFlags::Abstract)

#undef AERO_DETAIL_IMPLEMENT_INLINE_EMPTY_METADATA

inline Base::Result<void> TryRegisterControlPrimitiveMetadata(
    MetaRegistrationContext& context) noexcept {
    using Registrar = Base::Result<void> (*)(MetaRegistrationContext&) noexcept;
    const Registrar registrars[] = {
        &Panel::TryRegisterMetadata,
        &Decorator::TryRegisterMetadata,
        &Control::TryRegisterMetadata,
        &ContentControl::TryRegisterMetadata,
        &UserControl::TryRegisterMetadata};
    for (Registrar registrar : registrars) {
        Base::Result<void> registered = registrar(context);
        if (!registered) {
            return registered.GetStatus();
        }
    }
    return {};
}

} // namespace Aero::Core
