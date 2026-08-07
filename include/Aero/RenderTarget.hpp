#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/RenderDevice.hpp>

#include <cstdint>

namespace Aero {

// Presentation policy is relevant only when a target owns a presentable native
// surface. Embedded render targets ignore it.
enum class PresentMode : std::uint8_t {
    Immediate = 0U,
    Fifo,
    Mailbox
};

enum class RenderTargetKind : std::uint8_t {
    Embedded = 0U,
    Window
};

enum class RenderTargetState : std::uint8_t {
    Ready = 0U,
    Lost,
    DeviceLost,
    Failed,
    Shutdown
};

// Host-owned onscreen target. A target keeps a strong reference to a shared
// RenderDevice while native swap-chain/context presentation remains an App or
// backend implementation detail. Multiple targets may share one device.
class AERO_API RenderTarget final : public Base::Object {
    struct ConstructionToken {};

public:
    struct Impl;

    RenderTarget(
        ConstructionToken,
        Base::Ref<Aero::RenderDevice> device,
        RenderTargetKind kind,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RenderTarget() noexcept override;

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    RenderTargetKind Kind() const noexcept;
    RenderTargetState State() const noexcept;
    Base::Ref<Aero::RenderDevice> GetDevice() const noexcept;

    // Resize is meaningful for presentable/window targets. Embedded targets
    // keep their dimensions in the host-provided target descriptor.
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
