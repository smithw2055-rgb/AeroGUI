#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Platform/Window.hpp>

namespace Aero { class View; }

namespace Aero::App::Detail {

struct ApplicationRuntimeState final {
    void* context = nullptr;
    void (*requestExit)(void* context, int exitCode) noexcept = nullptr;
};

struct WindowRuntimeState final {
    void* context = nullptr;
    Base::Result<void> (*show)(void* context) noexcept = nullptr;
    void (*close)(void* context) noexcept = nullptr;
    bool (*isOpen)(const void* context) noexcept = nullptr;
    Platform::NativeWindowHandle (*nativeHandle)(
        const void* context) noexcept = nullptr;
    View* (*hostedView)(void* context) noexcept = nullptr;
};

} // namespace Aero::App::Detail
