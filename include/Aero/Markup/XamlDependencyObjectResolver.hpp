#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Markup {

// XAML does not depend on RTTI. Hosts register this explicit bridge for object
// families that are known to derive from Core::DependencyObject.
using XamlDependencyObjectCastCallback = Core::DependencyObject* (*)(
    Base::Object& object,
    void* context) noexcept;

struct XamlDependencyObjectResolver final {
    XamlDependencyObjectCastCallback cast = nullptr;
    void* context = nullptr;

    bool IsConfigured() const noexcept { return cast != nullptr; }

    Core::DependencyObject* TryResolve(Base::Object& object) const noexcept {
        return cast != nullptr ? cast(object, context) : nullptr;
    }
};

inline Core::DependencyObject* ResolveXamlDependencyObject(
    Base::Object& object,
    XamlDependencyObjectCastCallback cast,
    void* context) noexcept {
    return XamlDependencyObjectResolver{cast, context}.TryResolve(object);
}

} // namespace Aero::Markup
