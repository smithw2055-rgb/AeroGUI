#pragma once

#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

class AERO_API Panel : public FrameworkElement {
    AERO_DECLARE_METADATA(Panel, FrameworkElement)
protected:
    explicit Panel(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Panel() override = default;
};

class AERO_API Decorator : public FrameworkElement {
    AERO_DECLARE_METADATA(Decorator, FrameworkElement)
protected:
    explicit Decorator(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
    ~Decorator() override = default;
};

class AERO_API Control : public FrameworkElement {
    AERO_DECLARE_METADATA(Control, FrameworkElement)
protected:
    explicit Control(TypeId runtimeType) noexcept : FrameworkElement(runtimeType) {}
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
