#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Integration/RenderEndpoint.hpp>
#include <Aero/Integration/NativeWindow.hpp>

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

struct OpenGL33EmbeddedTarget final {
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

struct OpenGL33EmbeddedEndpointOptions final {
    OpenGL33ProcResolver resolve = nullptr;
    OpenGL33MakeCurrent makeCurrent = nullptr;
    OpenGL33IsCurrent isCurrent = nullptr;
    OpenGL33ContextGeneration contextGeneration = nullptr;
    OpenGL33TargetCallback acquireTarget = nullptr;
    void* callbackContext = nullptr;
    OpenGL33StatePreservationPolicy statePolicy =
        OpenGL33StatePreservationPolicy::HostResetsState;
};

struct OpenGL33WindowEndpointOptions final {
    NativeWindowHandle window;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    RenderPresentMode presentMode = RenderPresentMode::Fifo;
    bool enableDebugContext = false;
};

AERO_API Base::Result<Base::Ref<RenderEndpoint>>
CreateOpenGL33EmbeddedEndpoint(
    const OpenGL33EmbeddedEndpointOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

AERO_API Base::Result<Base::Ref<RenderEndpoint>>
CreateOpenGL33WindowEndpoint(
    const OpenGL33WindowEndpointOptions& options,
    Base::IAllocator* allocator = nullptr) noexcept;

} // namespace Aero::Integration
