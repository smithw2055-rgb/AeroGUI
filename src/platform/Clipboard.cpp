#include <Aero/Integration/PlatformServices.hpp>

namespace Aero::Integration {

Base::Result<void> MemoryClipboard::ReadText(
    Base::String& output) noexcept {
    return output.TryAssign(text_.View());
}

Base::Result<void> MemoryClipboard::WriteText(
    Base::StringView text) noexcept {
    Base::Result<void> assigned = text_.TryAssign(text);
    if (!assigned) {
        return assigned;
    }
    ++generation_;
    return {};
}

} // namespace Aero::Integration
