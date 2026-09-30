#pragma once

// Src-only bridge for Style private seal/runtime/provider seams. Installed
// Style.hpp friends this type instead of naming StyleSetter / TriggerPlan /
// StyleProviderSession in the public header.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Style.hpp>

namespace Aero {

struct StyleSetter;
struct TriggerPlan;
class StyleProviderSession;

class StyleSeams {
public:
    static Base::Result<void> Seal(
        Style& style,
        const Meta::DependencyPropertyRegistry& properties) noexcept;

    static Base::Span<const StyleSetter> RuntimeSetters(const Style& style) noexcept;

    static Base::Span<const TriggerPlan> RuntimeTriggers(const Style& style) noexcept;

    static Base::Result<void> ApplySetters(
        const Style& style,
        DependencyObject& object,
        StyleProviderSession& values) noexcept;

    static Base::Result<void> ClearSetters(
        const Style& style,
        DependencyObject& object,
        StyleProviderSession& values) noexcept;
};

} // namespace Aero
