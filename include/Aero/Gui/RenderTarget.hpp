#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/RenderDevice.hpp>

#include <cstdint>

namespace Aero {

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

// Host-owned onscreen target. Native backend state is the source-private Impl
// itself, avoiding a second NativeRenderTarget wrapper. Native acquire/present
// remains an implementation concern under src/render.
class AERO_API RenderTarget final : public Base::Object {
    struct ConstructionToken {};

public:
    struct Impl;

    RenderTarget(
        ConstructionToken,
        Base::Ref<Aero::RenderDevice> device,
        Impl* implementation) noexcept;
    ~RenderTarget() noexcept override;

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    RenderTargetKind Kind() const noexcept;
    RenderTargetState State() const noexcept;
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

    Base::Ref<Aero::RenderDevice> device_;
    Impl* impl_ = nullptr;
};

} // namespace Aero
