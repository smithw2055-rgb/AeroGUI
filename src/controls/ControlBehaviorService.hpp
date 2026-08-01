#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include "runtime/RuntimeFwd.hpp"

namespace Aero {
class ObjectTree;
class Visual;
namespace Core { class MetadataDomain; }
namespace Detail { class InputService; }
namespace Integration { class IClipboard; class ITextInputMethodHost; }
}

namespace Aero::Controls {
class VisualStateManager;
namespace Detail {

// Owns the built-in control class behavior state for one View. The individual
// behavior tables are implementation details; ViewRuntime only creates this
// aggregate and asks it to attach a newly mounted element.
class ControlBehaviorService final {
public:
    ControlBehaviorService(Base::IAllocator& allocator, Core::MetadataDomain& metadata,
        Aero::ObjectTree& tree, Aero::Detail::EventRouter& events,
        Aero::Detail::InputService& input, VisualStateManager* visualStates,
        Integration::IClipboard* clipboard, bool controlsEnabled,
        bool textEditingEnabled) noexcept;
    ~ControlBehaviorService() noexcept;

    ControlBehaviorService(const ControlBehaviorService&) = delete;
    ControlBehaviorService& operator=(const ControlBehaviorService&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(Visual& visual, Integration::ITextInputMethodHost* inputMethodHost) noexcept;
    Base::Result<std::uint32_t> AdvanceTime(std::uint32_t elapsedMilliseconds) noexcept;
    void Shutdown() noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Detail
} // namespace Aero::Controls
