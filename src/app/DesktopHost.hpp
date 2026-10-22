#pragma once

#include <AeroApp/App.hpp>

#include <cstddef>
#include <cstdint>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::App {

struct DesktopHostState;

// Private implementation of the optional desktop application framework.
// It is deliberately not installed and does not form a second authoring API.
class DesktopHost {
public:
    DesktopHost(
        Application& application,
        Base::Ref<Window> window,
        const RunOptions& options) noexcept;
    ~DesktopHost() noexcept;

    DesktopHost(const DesktopHost&) = delete;
    DesktopHost& operator=(const DesktopHost&) = delete;

    Base::Result<int> Run() noexcept;

private:
    DesktopHostState* state_ = nullptr;
};

} // namespace Aero::App
