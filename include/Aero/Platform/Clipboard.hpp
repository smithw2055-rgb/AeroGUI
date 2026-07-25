#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Platform {

class AERO_API IClipboard {
public:
    virtual ~IClipboard() = default;

    virtual Base::Result<void> ReadText(
        Base::String& output) noexcept = 0;
    virtual Base::Result<void> WriteText(
        Base::StringView text) noexcept = 0;
};

class AERO_API MemoryClipboard final : public IClipboard {
public:
    explicit MemoryClipboard(
        Base::IAllocator* allocator = nullptr) noexcept
        : text_(allocator) {}

    Base::Result<void> ReadText(
        Base::String& output) noexcept override;
    Base::Result<void> WriteText(
        Base::StringView text) noexcept override;
    std::uint64_t Generation() const noexcept {
        return generation_;
    }

private:
    Base::String text_;
    std::uint64_t generation_ = 0U;
};

// Native Windows CF_UNICODETEXT adapter. The owner is an optional HWND kept
// opaque in the public contract so platform headers never leak to consumers.
// On non-Windows builds operations return ErrorCode::Unsupported.
class AERO_API Win32Clipboard final : public IClipboard {
public:
    explicit Win32Clipboard(
        void* ownerWindow = nullptr) noexcept
        : ownerWindow_(ownerWindow) {}

    void SetOwnerWindow(void* value) noexcept {
        ownerWindow_ = value;
    }
    void* OwnerWindow() const noexcept {
        return ownerWindow_;
    }

    Base::Result<void> ReadText(
        Base::String& output) noexcept override;
    Base::Result<void> WriteText(
        Base::StringView text) noexcept override;

private:
    void* ownerWindow_ = nullptr;
};

} // namespace Aero::Platform
