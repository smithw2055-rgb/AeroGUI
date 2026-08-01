#include "InputServices.hpp"

#include <Aero/Base/Utf8.hpp>

#include <limits>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace Aero::Platform {
namespace {

Base::Status UnsupportedClipboard() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 clipboard is unavailable on this platform");
}

#if defined(_WIN32)
Base::Status ClipboardFailure(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

class ClipboardScope final {
public:
    explicit ClipboardScope(void* owner) noexcept
        : opened_(OpenClipboard(
              static_cast<HWND>(owner)) != FALSE) {}
    ~ClipboardScope() {
        if (opened_) {
            static_cast<void>(CloseClipboard());
        }
    }
    bool Opened() const noexcept {
        return opened_;
    }

private:
    bool opened_ = false;
};

Base::Result<int> Utf16LengthForUtf8(
    Base::StringView text) noexcept {
    if (!Base::ValidateUtf8(text).valid) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidUtf8,
            "Clipboard text is not valid UTF-8");
    }
    if (text.SizeBytes() >
        static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Clipboard text exceeds Win32 conversion limits");
    }
    const int required = text.Empty()
        ? 0
        : MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            text.Data(),
            static_cast<int>(text.SizeBytes()),
            nullptr, 0);
    if (!text.Empty() && required <= 0) {
        return ClipboardFailure(
            "Win32 failed to measure UTF-8 clipboard text");
    }
    return required;
}
#endif

} // namespace

Base::Result<void> Win32Clipboard::ReadText(
    Base::String& output) noexcept {
#if defined(_WIN32)
    ClipboardScope clipboard(ownerWindow_);
    if (!clipboard.Opened()) {
        return ClipboardFailure(
            "Win32 failed to open the clipboard");
    }
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data == nullptr) {
        output.Clear();
        return {};
    }
    const auto* wide = static_cast<const wchar_t*>(
        GlobalLock(data));
    if (wide == nullptr) {
        return ClipboardFailure(
            "Win32 failed to lock clipboard text");
    }
    const std::size_t measuredLength =
        wcslen(wide);
    if (measuredLength >
        static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        static_cast<void>(GlobalUnlock(data));
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Clipboard UTF-16 text exceeds Win32 conversion limits");
    }
    const int wideLength =
        static_cast<int>(measuredLength);
    const int required = wideLength == 0
        ? 0
        : WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            wide, wideLength,
            nullptr, 0, nullptr, nullptr);
    if (wideLength != 0 && required <= 0) {
        static_cast<void>(GlobalUnlock(data));
        return ClipboardFailure(
            "Win32 failed to measure clipboard UTF-16 text");
    }
    Base::String converted(&output.Allocator());
    Base::Result<void> reserved =
        converted.TryReserve(
            static_cast<std::uint32_t>(required));
    if (!reserved) {
        static_cast<void>(GlobalUnlock(data));
        return reserved;
    }
    if (required != 0) {
        // String has no writable resize operation, so convert into a
        // temporary GlobalAlloc buffer and append the validated UTF-8 bytes.
        char* bytes = static_cast<char*>(
            GlobalAlloc(
                GMEM_FIXED,
                static_cast<std::size_t>(required)));
        if (bytes == nullptr) {
            static_cast<void>(GlobalUnlock(data));
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Win32 clipboard UTF-8 allocation failed");
        }
        const int convertedCount =
            WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS,
                wide, wideLength,
                bytes, required, nullptr, nullptr);
        static_cast<void>(GlobalUnlock(data));
        if (convertedCount != required) {
            static_cast<void>(GlobalFree(bytes));
            return ClipboardFailure(
                "Win32 failed to convert clipboard text to UTF-8");
        }
        Base::Result<void> appended =
            converted.TryAppendUnchecked({
                bytes,
                static_cast<std::uint32_t>(required)});
        static_cast<void>(GlobalFree(bytes));
        if (!appended) {
            return appended;
        }
    } else {
        static_cast<void>(GlobalUnlock(data));
    }
    output = std::move(converted);
    return {};
#else
    static_cast<void>(output);
    return UnsupportedClipboard();
#endif
}

Base::Result<void> Win32Clipboard::WriteText(
    Base::StringView text) noexcept {
#if defined(_WIN32)
    Base::Result<int> measured =
        Utf16LengthForUtf8(text);
    if (!measured) {
        return measured.GetStatus();
    }
    const int characterCount =
        measured.Value();
    const std::size_t allocationBytes =
        (static_cast<std::size_t>(
            characterCount) + 1U) *
        sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(
        GMEM_MOVEABLE, allocationBytes);
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Win32 clipboard UTF-16 allocation failed");
    }
    auto* wide = static_cast<wchar_t*>(
        GlobalLock(memory));
    if (wide == nullptr) {
        static_cast<void>(GlobalFree(memory));
        return ClipboardFailure(
            "Win32 failed to lock clipboard output");
    }
    if (characterCount != 0 &&
        MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            text.Data(),
            static_cast<int>(text.SizeBytes()),
            wide, characterCount) !=
                characterCount) {
        static_cast<void>(GlobalUnlock(memory));
        static_cast<void>(GlobalFree(memory));
        return ClipboardFailure(
            "Win32 failed to convert clipboard text to UTF-16");
    }
    wide[characterCount] = L'\0';
    static_cast<void>(GlobalUnlock(memory));
    ClipboardScope clipboard(ownerWindow_);
    if (!clipboard.Opened()) {
        static_cast<void>(GlobalFree(memory));
        return ClipboardFailure(
            "Win32 failed to open the clipboard");
    }
    if (EmptyClipboard() == FALSE) {
        static_cast<void>(GlobalFree(memory));
        return ClipboardFailure(
            "Win32 failed to clear the clipboard");
    }
    // Ownership transfers to the system only after SetClipboardData succeeds.
    if (SetClipboardData(
            CF_UNICODETEXT, memory) == nullptr) {
        static_cast<void>(GlobalFree(memory));
        return ClipboardFailure(
            "Win32 failed to publish clipboard text");
    }
    return {};
#else
    static_cast<void>(text);
    return UnsupportedClipboard();
#endif
}

} // namespace Aero::Platform
