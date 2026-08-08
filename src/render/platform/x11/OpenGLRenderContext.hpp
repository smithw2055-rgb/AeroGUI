#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include "render/opengl33/OpenGL33.hpp"
#include "render/WindowRenderContext.hpp"

#include <cstdint>

namespace Aero::Graphics {

class AERO_API GlxRenderContext {
public:
    explicit GlxRenderContext(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~GlxRenderContext() noexcept;

    GlxRenderContext(const GlxRenderContext&) = delete;
    GlxRenderContext& operator=(const GlxRenderContext&) = delete;

    std::uintptr_t NativeDisplay() const noexcept;
    std::uintptr_t NativeDrawable() const noexcept;
    std::uintptr_t NativeContext() const noexcept;
    bool OwnsDisplay() const noexcept;
    bool OwnsDrawable() const noexcept;
    bool OwnsContext() const noexcept;
    GlContextGeneration ContextGeneration() const noexcept;

    Base::Result<GlFunctionTable> LoadFunctions() noexcept;
    Base::Result<GlContextBinding> ContextBinding() noexcept;
    Base::Result<void> MakeCurrent() noexcept;

    WindowRenderContextCaps
    Caps() const noexcept;
    Base::Result<void> Create(
        const WindowRenderContextDescriptor& descriptor) noexcept;
    void Shutdown() noexcept;
    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    Base::Result<RenderTargetBinding>
    AcquireTarget(std::uint64_t frameSerial) noexcept;
    Base::Result<void> Present(
        std::uint64_t frameSerial,
        FenceValue signalFence) noexcept;
    void DiscardFrame(std::uint64_t frameSerial) noexcept;
    void NotifyLost() noexcept;
    Base::Result<void> Restore(
        const WindowRenderContextDescriptor& descriptor) noexcept;
    bool IsLost() const noexcept;

private:
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Graphics
