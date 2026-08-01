#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Markup::Detail {

// Supports the object-element form used by the reference theme, for example
// <StaticResource ResourceKey="Anim.Expand.Vertical.Loaded"/>.
class StaticResourceObject final : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        StaticResourceObject,
        ::Aero::DependencyObject,
        "urn:aero",
        "StaticResource")
public:
    StaticResourceObject() noexcept
        : DependencyObject(StaticTypeId()) {}

    Base::StringView ResourceKey() const noexcept {
        return GetValueOr(ResourceKeyProperty, Base::StringView{});
    }
    Base::Result<void> SetResourceKey(
        Base::StringView value) noexcept {
        return SetValue(ResourceKeyProperty, value);
    }

    inline static constexpr Members::Property<Base::String>
        ResourceKeyProperty{"ResourceKey"};
};

} // namespace Aero::Markup::Detail
