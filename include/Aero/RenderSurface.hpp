#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/RenderDevice.hpp>

#include <cstdint>

namespace Aero {

// Presentation policy belongs to a window or embedded target, not to the
// shared GPU device.
enum class PresentMode : std::uint8_t {
    Immediate = 0U,
    Fifo,
    Mailbox
};

enum class RenderSurfaceKind : std::uint8_t {
    Embedded = 0U,
    Window
};

enum class RenderSurfaceState : std::uint8_t {
    Ready = 0U,
    Lost,
    DeviceLost,
    Failed,
    Shutdown
};

// Host-owned onscreen target. A surface keeps a strong reference to a shared
// RenderDevice and owns only its native presentation state. Multiple surfaces
// may therefore render through the same device and GPU resource registries.
class AERO_API RenderSurface final : public Base::Object {
    struct ConstructionToken {};

public:
    struct Impl;

    RenderSurface(
        ConstructionToken,
        Base::Ref<Aero::RenderDevice> device,
        RenderSurfaceKind kind,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RenderSurface() noexcept override;

    RenderSurface(const RenderSurface&) = delete;
    RenderSurface& operator=(const RenderSurface&) = delete;

    RenderSurfaceKind Kind() const noexcept;
    RenderSurfaceState State() const noexcept;
    Base::Ref<Aero::RenderDevice> GetDevice() const noexcept;

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    void NotifyLost() noexcept;
    Base::Result<void> Restore() noexcept;

private:
    friend struct Impl;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    Impl* impl_ = nullptr;
};

} // namespace Aero
