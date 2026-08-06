#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/RenderSurface.hpp>
#include <Aero/Platform/NativeWindow.hpp>

#include <cstdint>

namespace Aero::Integration {

enum class OpenGL33StatePreservationPolicy : std::uint8_t {
    HostResetsState = 0U,
    PreserveRequiredState
};

using OpenGL33ProcAddress = void (*)();
using OpenGL33ProcResolver = OpenGL33ProcAddress (*)(
    void* context,
    const char* name) noexcept;
using OpenGL33MakeCurrent = Base::Status (*)(
    void* context) noexcept;
using OpenGL33IsCurrent = bool (*)(
    void* context) noexcept;
using OpenGL33ContextGeneration = std::uint64_t (*)(
    void* context) noexcept;

struct OpenGL33DeviceOptions {
    OpenGL33ProcResolver resolve = nullptr;
    OpenGL33MakeCurrent makeCurrent = nullptr;
    OpenGL33IsCurrent isCurrent = nullptr;
    OpenGL33ContextGeneration contextGeneration = nullptr;
    void* callbackContext = nullptr;
    OpenGL33StatePreservationPolicy statePolicy =
        OpenGL33StatePreservationPolicy::HostResetsState;
    bool checkErrors = false;
};

struct OpenGL33EmbeddedTarget  {
    std::uint32_t framebuffer = 0U;
    std::uint32_t depthStencilTexture = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint64_t stableId = 0U;
    bool defaultFramebuffer = false;
};

using OpenGL33TargetCallback = Base::Status (*)(
    void* context,
    OpenGL33EmbeddedTarget* target) noexcept;

struct OpenGL33EmbeddedSurfaceOptions  {
    OpenGL33ProcResolver resolve = nullptr;
    OpenGL33MakeCurrent makeCurrent = nullptr;
    OpenGL33IsCurrent isCurrent = nullptr;
    OpenGL33ContextGeneration contextGeneration = nullptr;
    OpenGL33TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    void* targetContext = nullptr;
    OpenGL33StatePreservationPolicy statePolicy =
        OpenGL33StatePreservationPolicy::HostResetsState;
};

struct OpenGL33WindowSurfaceOptions  {
    NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    PresentMode presentMode = PresentMode::Fifo;
    bool enableDebugContext = false;
};

AERO_API Base::Result<Base::Ref<Aero::RenderDevice>>
CreateOpenGL33Device(
    const OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<RenderSurface>>
CreateOpenGL33EmbeddedSurface(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<RenderSurface>>
CreateOpenGL33EmbeddedSurface(
    const OpenGL33EmbeddedSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<RenderSurface>>
CreateOpenGL33WindowSurface(
    const OpenGL33WindowSurfaceOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration
