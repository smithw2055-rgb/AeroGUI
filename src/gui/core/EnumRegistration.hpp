#pragma once

// Shared helper for built-in enum metadata registration.
// Families own AERO_REGISTER_ENUM bodies; BuiltinModules only orchestrates.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Meta.hpp>

namespace Aero {

template<class T, class TPopulate>
inline Base::Result<void> RegisterEnum(
    Meta::Registration& context,
    Base::StringView name,
    TPopulate&& populate) noexcept {
    auto description = Meta::Register<T>(context, name);
    populate(description);
    return description.Result();
}

#define AERO_REGISTER_ENUM(Type, Name, Values) \
    status = ::Aero::RegisterEnum<Type>(context, Name, \
        [](auto& description) { Values }); \
    if (!status) return status.GetStatus()

Base::Result<void> PopulateInputEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateAnimationEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateInteractivityEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateElementEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateMediaEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateShapesEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateDataEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateTextEnums(
    Meta::Registration& context) noexcept;
Base::Result<void> PopulateControlsEnums(
    Meta::Registration& context) noexcept;

} // namespace Aero
