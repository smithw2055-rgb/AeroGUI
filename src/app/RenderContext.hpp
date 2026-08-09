#pragma once

#include <AeroApp/App.hpp>
#include <Aero/IRenderer.hpp>
#include <AeroApp/WindowInterop.hpp>
#include <AeroRender/RenderTarget.hpp>

namespace Aero::App {

// Private desktop presentation owner. Backend-specific contexts create the
// native device/target pair; this base owns the common frame and Present state.
class RenderContext {
public:
    RenderContext() noexcept = default;
    virtual ~RenderContext() noexcept = default;

    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    Base::Result<void> BeginFrame() noexcept;
    Base::Result<void> EndFrame() noexcept;
    Base::Result<void> Present() noexcept;
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

protected:
    Base::Result<void> AdoptTarget(
        Base::Ref<RenderTarget> target) noexcept;

    virtual Base::Result<void> BeginPresentation() noexcept = 0;
    virtual Base::Result<void> ResizePresentation(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual Base::Result<void> PresentFrame() noexcept = 0;
    virtual void CancelFrame() noexcept = 0;
    virtual void ShutdownPresentation() noexcept = 0;

private:
    Base::Ref<RenderTarget> target_;
    RenderTarget* currentTarget_ = nullptr;
    bool frameOpen_ = false;
    bool frameRendered_ = false;
    bool frameEnded_ = false;
};

// Creates the concrete desktop context selected by RunOptions. Ownership of a
// successful result transfers to the caller.
Base::Result<RenderContext*> CreateRenderContext(
    GraphicsBackend backend,
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator = nullptr) noexcept;

Base::Result<RenderContext*> CreateD3D11RenderContext(
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator = nullptr) noexcept;
Base::Result<RenderContext*> CreateOpenGL33RenderContext(
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::App
