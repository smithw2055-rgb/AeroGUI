#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

#include <cstdint>

namespace Aero {

// Private compatibility owners used by the built-in theme schema. They are
// registered for XAML compatibility but are not C++ authoring APIs.
class Element final : public Base::Object {
    AERO_DECLARE_TYPE(Element, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Core::AttachedPropertyRef<Element, double>
        PPAAOutProperty{"PPAAOut"};
    inline static constexpr Core::AttachedPropertyRef<Element, bool>
        IsFocusEngagedProperty{"IsFocusEngaged"};
};

class TextProperties final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TextProperties, Base::Object, "urn:aero", "Text")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Core::AttachedPropertyRef<
        TextProperties, std::uint32_t>
        PasswordLengthProperty{"PasswordLength"};
};

} // namespace Aero
