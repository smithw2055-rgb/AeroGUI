#pragma once

#include <Aero/App.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/Platform/NativeWindow.hpp>
#include <Aero/RenderTarget.hpp>

namespace Aero::App::Detail {

// Private desktop presentation owner. Window/swap-chain creation, target resize
// and presentation stay out of View and the embeddable rendering contract.
class RenderContext final {
public:
    RenderContext() noexcept = default;
    ~RenderContext() noexcept { Shutdown(); }

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    Base::Result<void> Create(
        GraphicsBackend backend,
        Platform::NativeWindowHandle window,
        std::uint32_t width,
        std::uint32_t height,
        Base::IAllocator* allocator = nullptr) noexcept;
    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    Base::Result<void> Render(IRenderer& renderer) noexcept;
    void Shutdown() noexcept;

    bool IsReady() const noexcept {
        return target_ && target_->State() == RenderTargetState::Ready;
    }
    Base::Ref<RenderDevice> Device() const noexcept {
        return target_ ? target_->GetDevice() : Base::Ref<RenderDevice>{};
    }
    RenderTarget* Target() noexcept { return target_.Get(); }
    const RenderTarget* Target() const noexcept { return target_.Get(); }

private:
    Base::Ref<RenderTarget> target_;
};

} // namespace Aero::App::Detail
