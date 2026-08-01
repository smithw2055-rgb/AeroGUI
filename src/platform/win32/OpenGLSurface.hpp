#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include "graphics/OpenGL33.hpp"
#include "platform/Surface.hpp"

#include <cstdint>

namespace Aero::Graphics {

class AERO_API WglSurfaceBackend final : public ISurfaceBackend {
public:
    explicit WglSurfaceBackend(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~WglSurfaceBackend() noexcept override;

    WglSurfaceBackend(const WglSurfaceBackend&) = delete;
    WglSurfaceBackend& operator=(const WglSurfaceBackend&) = delete;

    std::uintptr_t NativeDeviceContext() const noexcept;
    std::uintptr_t NativeRenderContext() const noexcept;
    bool OwnsContext() const noexcept;
    GlContextGeneration ContextGeneration() const noexcept;

    Base::Result<GlFunctionTable> LoadFunctions() noexcept;
    Base::Result<GlContextContract> ContextContract() noexcept;
    Base::Result<void> MakeCurrent() noexcept;

    SurfaceCapabilities
    QuerySurfaceCapabilities() const noexcept override;
    Base::Result<void> CreateSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept override;
    void DestroySurface() noexcept override;
    Base::Result<void> ResizeSurface(
        std::uint32_t width,
        std::uint32_t height) noexcept override;
    Base::Result<ExternalRenderTargetDescriptor>
    AcquireSurfaceTarget(std::uint64_t frameSerial) noexcept override;
    Base::Result<void> PresentSurface(
        std::uint64_t frameSerial,
        FenceValue signalFence) noexcept override;
    void DiscardSurfaceFrame(std::uint64_t frameSerial) noexcept override;
    void NotifySurfaceLost() noexcept override;
    Base::Result<void> RestoreSurface(
        const NativeSurfaceDescriptor& descriptor) noexcept override;
    bool IsSurfaceLost() const noexcept override;

private:
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Graphics
