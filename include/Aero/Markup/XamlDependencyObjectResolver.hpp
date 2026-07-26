#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
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

    Base::Result<Core::DependencyObject*> RequireResolve(
        Base::Object& object,
        const char* failureMessage) const noexcept {
        if (cast == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML DependencyObject resolver is not configured");
        }
        Core::DependencyObject* resolved = cast(object, context);
        if (resolved == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                failureMessage);
        }
        return resolved;
    }
};

} // namespace Aero::Markup
