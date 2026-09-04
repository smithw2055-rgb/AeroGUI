#pragma once

#include <Aero/VisualState.hpp>
#include <Aero/VisualTransition.hpp>
#include <Aero/VisualStateGroup.hpp>
#include <Aero/VisualStateGroupCollection.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Controls { class Control; }

namespace Aero {

struct VisualStateManagerRuntime;

// Public authoring uses the WPF static entry point. Runtime state and animation
// bookkeeping remain private and are accessed only by the controls runtime.
class AERO_GUI_API VisualStateManager : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualStateManager, Base::Object, "urn:aero", "VisualStateManager")
public:

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static bool GoToState(
        Controls::Control& control,
        StringView stateName,
        bool useTransitions = true) noexcept;

    inline static constexpr AttachedProperty<Ref<VisualStateGroupCollection>> VisualStateGroupsProperty{"VisualStateGroups"};

    ~VisualStateManager() noexcept override;
    VisualStateManager(const VisualStateManager&) = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

private:
    friend struct VisualStateManagerRuntime;
    VisualStateManager() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero
