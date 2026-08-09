#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/RenderTarget.hpp>

#include <cstdint>

namespace Aero::Render::OpenGL33 {

enum class StatePreservationPolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
};

using ProcAddress = void (*)();
using ProcResolver = ProcAddress (*)(
    void* context,
    const char* name) noexcept;
using MakeCurrent = Base::Status (*)(
    void* context) noexcept;
using IsCurrent = bool (*)(
    void* context) noexcept;
using ContextGeneration = std::uint64_t (*)(
    void* context) noexcept;

struct DeviceOptions {
    ProcResolver resolve = nullptr;
    MakeCurrent makeCurrent = nullptr;
    IsCurrent isCurrent = nullptr;
    ContextGeneration contextGeneration = nullptr;
    void* callbackContext = nullptr;
    StatePreservationPolicy statePolicy =
        StatePreservationPolicy::HostResetsState;
    bool checkErrors = false;
};

struct EmbeddedTarget {
    std::uint32_t framebuffer = 0U;
    std::uint32_t depthStencilTexture = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint64_t stableId = 0U;
    bool defaultFramebuffer = false;
};

using TargetCallback = Base::Status (*)(
    void* context,
    EmbeddedTarget* target) noexcept;

// Device/context activation belongs to the explicitly supplied RenderDevice.
// The target contract only selects the framebuffer exposed by the host.
struct TargetOptions {
    TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    void* targetContext = nullptr;
    // Desktop hosts normally clear the default framebuffer. Embedded hosts
    // preserve the existing contents unless they opt in explicitly.
    bool clearBeforeRender = false;
};

AERO_RENDER_OPENGL33_API Result<Ref<Aero::RenderDevice>>
CreateDevice(
    const DeviceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_RENDER_OPENGL33_API Result<Ref<Aero::RenderTarget>>
CreateTarget(
    Ref<Aero::RenderDevice> device,
    const TargetOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;
} // namespace Aero::Render::OpenGL33
