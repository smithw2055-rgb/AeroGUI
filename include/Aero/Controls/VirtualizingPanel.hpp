#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

namespace Aero::Controls {

class ItemContainerGenerator;

using ::Aero::Meta::TypeId;

enum class ScrollUnit : std::uint8_t { Item = 0U, Pixel };

enum class VirtualizationMode : std::uint8_t { Standard = 0U, Recycling };

// WPF attached-property owner shared by all virtualizing panels. The current
// panel implementation is pixel-based; exposing this owner preserves the
// authored contract while item-unit realization is added.
class AERO_GUI_API VirtualizingPanel : public Panel {
    AERO_DECLARE_TYPE(VirtualizingPanel, Panel)

public:
    AERO_ATTACHED_PROPERTY(ScrollUnit, ScrollUnit);
    AERO_ATTACHED_PROPERTY(VirtualizationMode, VirtualizationMode);

protected:
    friend class ItemContainerGenerator;

    explicit VirtualizingPanel(TypeId runtimeType) noexcept : Panel(runtimeType) {}
    ~VirtualizingPanel() override = default;

    // Shared by stack and wrap virtualization. Stack keeps its own extent tree.
    virtual Result<void> AttachGenerator(ItemContainerGenerator& generator, std::uint32_t itemCount) noexcept;
    virtual void DetachGenerator(ItemContainerGenerator& generator) noexcept;
    virtual void HandleItemsChanged(const Collections::ItemsChangedEvent& event, std::uint32_t itemCount) noexcept;

    ItemContainerGenerator* generator_ = nullptr;
    Base::Vector<double> itemExtents_;
    ScrollData data_{};
    std::uint32_t visibleFirstIndex_ = 0U;
    std::uint32_t visibleCount_ = 0U;
    std::uint32_t desiredFirstIndex_ = 0U;
    std::uint32_t desiredCount_ = 0U;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollUnit)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::VirtualizationMode)
