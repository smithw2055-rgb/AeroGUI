#pragma once

#include <Aero/Controls/ItemsControl.hpp>


namespace Aero { class AeroGuiInternal; }

namespace Aero::Controls {

class VirtualizingStackPanel;

class AERO_GUI_API ItemContainerGenerator {
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
    void SetRealizationRange(std::uint32_t firstIndex, std::uint32_t count) noexcept;

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
    struct Impl;
    friend struct Impl;
    friend class ::Aero::AeroGuiInternal;

    ItemContainerGenerator() noexcept = default;
    Impl* impl_ = nullptr;

    // Privileged helpers: ItemContainerGenerator is the sole friend of
    // ItemsControl / VirtualizingStackPanel; Impl calls these instead of
    // touching their private members directly.
    static bool OwnerHasGenerator(const ItemsControl& owner) noexcept;
    static void SetOwnerGenerator(
        ItemsControl& owner,
        ItemContainerGenerator* generator) noexcept;
    static void NotifyOwnerContainersChanged(ItemsControl& owner) noexcept;
    static Result<void> AttachHostGenerator(
        VirtualizingStackPanel& host,
        ItemContainerGenerator& generator,
        std::uint32_t itemCount) noexcept;
    static void DetachHostGenerator(
        VirtualizingStackPanel& host,
        ItemContainerGenerator& generator) noexcept;
    static Result<void> HostHandleItemsChanged(
        VirtualizingStackPanel& host,
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
};

} // namespace Aero::Controls
