#pragma once

#include <Aero/DependencyProperty.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

namespace Aero {

class DependencyObject;

namespace Meta {
class DependencyPropertyRegistry;
}

struct ResolvedAnimationProperty {
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
};

Base::Result<ResolvedAnimationProperty> ResolveAnimationPropertyPath(
    ::Aero::DependencyObject& rootTarget,
    Base::StringView authoredPath,
    Meta::DependencyPropertyRegistry& properties) noexcept;

} // namespace Aero
