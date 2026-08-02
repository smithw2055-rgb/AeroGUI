#include <Aero/Integration/Platform.hpp>

namespace Aero::Integration {

Base::Result<void> MemoryClipboard::ReadText(
    Base::String& output) noexcept {
    return output.Assign(text_.View());
}

Base::Result<void> MemoryClipboard::WriteText(
    Base::StringView text) noexcept {
    Base::Result<void> assigned = text_.Assign(text);
    if (!assigned) {
        return assigned;
    }
    ++generation_;
    return {};
}

} // namespace Aero::Integration
