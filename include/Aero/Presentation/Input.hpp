#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/InputValues.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>

namespace Aero::Presentation {

using namespace Aero::Core;


// UI-thread visual-tree hit testing. The last visual child is frontmost.
// Callers register the C++ cast for each concrete layout type; this avoids
// RTTI and keeps the Core runtime usable with /GR- builds.

using PointerStateChangedHandler =
    Base::Delegate<void(UIElement&)>;
using PointerCaptureChangedHandler =
    Base::Delegate<void(
        std::uint32_t, UIElement*, bool)>;


enum class FocusNavigationDirection : std::uint8_t {
    Next,
    Previous,
};

enum class KeyboardNavigationMode : std::uint8_t {
    Continue = 0U,
    Once,
    Cycle,
    None,
    Contained,
    Local
};

class AERO_API KeyboardNavigation final
    : public Base::Object {
    AERO_DECLARE_TYPE(
        KeyboardNavigation,
        Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Members::AttachedProperty<
        KeyboardNavigationMode>
        DirectionalNavigationProperty{
            "DirectionalNavigation"};

    inline static constexpr Members::AttachedProperty<
        KeyboardNavigationMode>
        TabNavigationProperty{
            "TabNavigation"};
    inline static constexpr Members::AttachedProperty<
        std::uint32_t>
        TabIndexProperty{"TabIndex"};
};


// UI-thread keyboard router. It delivers KeyDown/KeyUp to the current focus
// target and uses the same routed-event snapshot semantics as pointer input.


} // namespace Aero::Presentation

namespace Aero::Core {

template<>
struct MetaTypeTraits<
    Presentation::KeyboardNavigationMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId(
            "KeyboardNavigationMode");
    }
    static constexpr Base::StringView
    Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "KeyboardNavigationMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
