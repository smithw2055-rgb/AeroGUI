#pragma once

#include <Aero/Controls/Popup.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ToolTip
    : public Primitives::Popup {
    AERO_DECLARE_TYPE(ToolTip, Primitives::Popup)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    ToolTip() noexcept
        : Primitives::Popup(StaticTypeId()) {}
    ~ToolTip() override = default;

    std::uint32_t GetInitialShowDelay()
        const noexcept;
    void SetInitialShowDelay(std::uint32_t value) noexcept;
    std::uint32_t GetShowDuration()
        const noexcept;
    void SetShowDuration(std::uint32_t value) noexcept;

    AERO_DEPENDENCY_PROPERTY(std::uint32_t, InitialShowDelay);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, ShowDuration);
};

class AERO_GUI_API ToolTipService
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ToolTipService, Base::Object)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Ref<ToolTip> GetToolTip(
        const DependencyObject& target) noexcept;
    static void SetToolTip(DependencyObject& target, Ref<ToolTip> value) noexcept;
    static std::uint32_t GetInitialShowDelay(
        const DependencyObject& target) noexcept;
    static void SetInitialShowDelay(DependencyObject& target, std::uint32_t value) noexcept;
    static std::uint32_t GetShowDuration(
        const DependencyObject& target) noexcept;
    static void SetShowDuration(DependencyObject& target, std::uint32_t value) noexcept;

    AERO_ATTACHED_PROPERTY(Ref<ToolTip>, ToolTip);
    AERO_ATTACHED_PROPERTY(std::uint32_t, InitialShowDelay);
    AERO_ATTACHED_PROPERTY(std::uint32_t, ShowDuration);
};

} // namespace Aero::Controls
