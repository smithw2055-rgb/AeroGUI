#pragma once

#include <Aero/Gui/ItemsControl.hpp>

namespace Aero::Controls {

class VirtualizingStackPanel;

class AERO_GUI_API ItemContainerGenerator {
public:
    struct Access;

    ~ItemContainerGenerator() noexcept;
    ItemContainerGenerator(const ItemContainerGenerator&) = delete;
    ItemContainerGenerator& operator=(const ItemContainerGenerator&) = delete;

    Base::Result<void> Attach(
        ItemsControl& owner,
        Panel& itemsHost) noexcept;
    Base::Result<void> AttachVirtualized(
        ItemsControl& owner,
        VirtualizingStackPanel& itemsHost) noexcept;
    Base::Result<bool> Detach() noexcept;
    Base::Result<void> Refresh() noexcept;
    void SetRealizationRange(
        std::uint32_t firstIndex,
        std::uint32_t count) noexcept;

    std::uint32_t GetGeneratedCount() const noexcept;
    std::uint32_t GetFirstGeneratedIndex() const noexcept;
    std::uint32_t GetCreatedContainerCount() const noexcept;
    std::uint32_t GetRecycledContainerUseCount() const noexcept;
    FrameworkElement* ContainerFromIndex(
        std::uint32_t index) const noexcept;
    std::uint32_t IndexFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Ref<Base::Object> ItemFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Status LastError() const noexcept;

private:
    friend struct Control::Access;
    friend struct Access;

    ItemContainerGenerator() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero::Controls
