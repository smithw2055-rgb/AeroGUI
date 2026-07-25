#include <Aero/Platform/Ime.hpp>

#include <Aero/Base/String.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>

#include <cmath>
#include <limits>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <imm.h>
#endif

namespace Aero::Platform {
namespace {

#if defined(_WIN32)
Base::Status NotAttached() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized,
        "Win32 IME adapter is not attached");
}

class InputContextScope final {
public:
    explicit InputContextScope(HWND window) noexcept
        : window_(window),
          context_(ImmGetContext(window)) {}
    ~InputContextScope() {
        if (context_ != nullptr) {
            static_cast<void>(
                ImmReleaseContext(
                    window_, context_));
        }
    }
    HIMC Get() const noexcept {
        return context_;
    }

private:
    HWND window_ = nullptr;
    HIMC context_ = nullptr;
};

Base::Result<void> ReadCompositionText(
    HWND window,
    DWORD kind,
    Base::String& output) noexcept {
    InputContextScope input(window);
    if (input.Get() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 IME context is unavailable");
    }
    const LONG byteCount =
        ImmGetCompositionStringW(
            input.Get(), kind, nullptr, 0U);
    if (byteCount < 0 ||
        byteCount % static_cast<LONG>(
            sizeof(wchar_t)) != 0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 IME returned invalid composition bytes");
    }
    if (byteCount == 0) {
        output.Clear();
        return {};
    }
    const std::uint32_t characterCount =
        static_cast<std::uint32_t>(
            byteCount /
            static_cast<LONG>(
                sizeof(wchar_t)));
    Base::Vector<wchar_t> wide;
    Base::Result<void> resized =
        wide.TryResize(characterCount);
    if (!resized) {
        return resized;
    }
    const LONG copied =
        ImmGetCompositionStringW(
            input.Get(), kind,
            wide.Data(),
            static_cast<DWORD>(byteCount));
    if (copied != byteCount ||
        characterCount >
            static_cast<std::uint32_t>(
                std::numeric_limits<int>::max())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 IME composition read failed");
    }
    const int required =
        WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.Data(),
            static_cast<int>(characterCount),
            nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidUtf8,
            "Win32 IME composition is not valid Unicode");
    }
    Base::Vector<char> bytes;
    Base::Result<void> byteStorage =
        bytes.TryResize(
            static_cast<std::uint32_t>(
                required));
    if (!byteStorage) {
        return byteStorage;
    }
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.Data(),
            static_cast<int>(characterCount),
            bytes.Data(), required,
            nullptr, nullptr) != required) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 IME UTF-8 conversion failed");
    }
    return output.TryAssignUnchecked({
        bytes.Data(),
        static_cast<std::uint32_t>(
            required)});
}
#endif

} // namespace

Win32ImeAdapter::~Win32ImeAdapter() {
    static_cast<void>(Detach());
}

Base::Result<void> Win32ImeAdapter::Attach(
    void* window) noexcept {
#if defined(_WIN32)
    if (window == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Win32 IME requires a window");
    }
    if (IsAttached()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Win32 IME adapter is already attached");
    }
    window_ = window;
    composing_ = false;
    return {};
#else
    static_cast<void>(window);
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

Base::Result<bool> Win32ImeAdapter::Detach() noexcept {
    if (!IsAttached()) {
        return false;
    }
    Base::Status failure;
    if (composing_) {
        Base::Result<void> native =
            CancelNativeComposition();
        if (!native) {
            failure = native.GetStatus();
        }
        if (client_ != nullptr) {
            Base::Result<void> client =
                client_->CancelComposition();
            if (!client && failure.IsOk()) {
                failure = client.GetStatus();
            }
        }
    }
    composing_ = false;
    client_ = nullptr;
    window_ = nullptr;
    if (!failure.IsOk()) {
        return failure;
    }
    return true;
}

Base::Result<void> Win32ImeAdapter::SetClient(
    ITextCompositionClient* client) noexcept {
#if defined(_WIN32)
    if (!IsAttached()) {
        return NotAttached();
    }
    if (client != nullptr &&
        client_ != nullptr &&
        client_ != client) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Win32 IME adapter already has a client");
    }
    if (client == nullptr && composing_) {
        Base::Result<void> cancelled =
            CancelNativeComposition();
        if (!cancelled) {
            return cancelled;
        }
        composing_ = false;
    }
    client_ = client;
    return {};
#else
    static_cast<void>(client);
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

Base::Result<bool> Win32ImeAdapter::HandleMessage(
    std::uint32_t message,
    std::uintptr_t wParam,
    std::intptr_t lParam) noexcept {
    static_cast<void>(wParam);
#if defined(_WIN32)
    if (!IsAttached() || client_ == nullptr) {
        return NotAttached();
    }
    if (message == WM_IME_STARTCOMPOSITION) {
        if (composing_) {
            return true;
        }
        Base::Result<void> begun =
            client_->BeginComposition();
        if (!begun) {
            return begun.GetStatus();
        }
        composing_ = true;
        return true;
    }
    if (message == WM_IME_COMPOSITION) {
        if (!composing_) {
            Base::Result<void> begun =
                client_->BeginComposition();
            if (!begun) {
                return begun.GetStatus();
            }
            composing_ = true;
        }
        const auto flags =
            static_cast<std::uintptr_t>(lParam);
        Base::String text;
        if ((flags & GCS_RESULTSTR) != 0U) {
            Base::Result<void> read =
                ReadCompositionText(
                    static_cast<HWND>(window_),
                    GCS_RESULTSTR, text);
            if (!read) {
                return read.GetStatus();
            }
            Base::Result<void> committed =
                client_->CommitComposition(
                    text.View());
            if (!committed) {
                return committed.GetStatus();
            }
            composing_ = false;
            return true;
        }
        if ((flags & GCS_COMPSTR) != 0U) {
            Base::Result<void> read =
                ReadCompositionText(
                    static_cast<HWND>(window_),
                    GCS_COMPSTR, text);
            if (!read) {
                return read.GetStatus();
            }
            Base::Result<void> updated =
                client_->UpdateComposition(
                    text.View());
            if (!updated) {
                return updated.GetStatus();
            }
        }
        return true;
    }
    if (message == WM_IME_ENDCOMPOSITION) {
        if (composing_) {
            Base::Result<void> cancelled =
                client_->CancelComposition();
            if (!cancelled) {
                return cancelled.GetStatus();
            }
            composing_ = false;
        }
        return true;
    }
    return false;
#else
    static_cast<void>(message);
    static_cast<void>(lParam);
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

Base::Result<void>
Win32ImeAdapter::SetCandidateWindow(
    const ImeCandidateWindow& value) noexcept {
#if defined(_WIN32)
    if (!IsAttached()) {
        return NotAttached();
    }
    if (!Base::IsValidRect(value.caret) ||
        !std::isfinite(value.dpiScale) ||
        value.dpiScale <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "IME candidate window geometry is invalid");
    }
    const double x =
        value.caret.x * value.dpiScale;
    const double y =
        (value.caret.y + value.caret.height) *
        value.dpiScale;
    if (x < static_cast<double>(
            std::numeric_limits<LONG>::min()) ||
        x > static_cast<double>(
            std::numeric_limits<LONG>::max()) ||
        y < static_cast<double>(
            std::numeric_limits<LONG>::min()) ||
        y > static_cast<double>(
            std::numeric_limits<LONG>::max())) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "IME candidate window exceeds Win32 coordinates");
    }
    InputContextScope input(
        static_cast<HWND>(window_));
    if (input.Get() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 IME context is unavailable");
    }
    CANDIDATEFORM form{};
    form.dwIndex = 0U;
    form.dwStyle = CFS_CANDIDATEPOS;
    form.ptCurrentPos.x =
        static_cast<LONG>(std::lround(x));
    form.ptCurrentPos.y =
        static_cast<LONG>(std::lround(y));
    if (ImmSetCandidateWindow(
            input.Get(), &form) == FALSE) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 failed to position the IME candidate window");
    }
    return {};
#else
    static_cast<void>(value);
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

Base::Result<void>
Win32ImeAdapter::CancelNativeComposition() noexcept {
#if defined(_WIN32)
    if (!IsAttached()) {
        return NotAttached();
    }
    InputContextScope input(
        static_cast<HWND>(window_));
    if (input.Get() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 IME context is unavailable");
    }
    if (ImmNotifyIME(
            input.Get(),
            NI_COMPOSITIONSTR,
            CPS_CANCEL, 0U) == FALSE) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Win32 failed to cancel IME composition");
    }
    composing_ = false;
    return {};
#else
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

} // namespace Aero::Platform
