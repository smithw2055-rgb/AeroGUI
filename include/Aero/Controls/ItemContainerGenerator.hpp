#pragma once

#include <Aero/Controls/ItemsControl.hpp>

namespace Aero::Controls {

class VirtualizingStackPanel;

class AERO_GUI_API ItemContainerGenerator {
#if defined(AERO_GUI_IMPLEMENTATION)
public:
#else
private:
#endif
    struct Access;

public:

    ~ItemContainerGenerator() noexcept;
    ItemContainerGenerator(const ItemContainerGenerator&) = delete;
    ItemContainerGenerator& operator=(const ItemContainerGenerator&) = delete;

    Result<void> Attach(
        ItemsControl& owner,
        Panel& itemsHost) noexcept;
    Result<void> AttachVirtualized(
        ItemsControl& owner,
        VirtualizingStackPanel& itemsHost) noexcept;
    Result<bool> Detach() noexcept;
    Result<void> Refresh() noexcept;
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
    Ref<Base::Object> ItemFromContainer(
        const FrameworkElement& container) const noexcept;
    Base::Status LastError() const noexcept;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct Control::Access;
#endif
#endif
    friend struct Access;

    ItemContainerGenerator() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero::Controls
