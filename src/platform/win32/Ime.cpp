#include "InputRouters.hpp"

#include <Aero/Base/String.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

Base::Status ImeFailure(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

const wchar_t* ImeAdapterPropertyName() noexcept {
    return L"AeroGui.Win32ImeAdapter";
}

class InputContextScope final {
public:
    explicit InputContextScope(HWND window) noexcept
        : window_(window),
          context_(ImmGetContext(window)) {}
    ~InputContextScope() {
        if (context_ != nullptr) {
            static_cast<void>(
                ImmReleaseContext(window_, context_));
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
        return ImeFailure(
            "Win32 IME context is unavailable");
    }
    const LONG byteCount = ImmGetCompositionStringW(
        input.Get(), kind, nullptr, 0U);
    if (byteCount < 0 ||
        byteCount % static_cast<LONG>(sizeof(wchar_t)) != 0) {
        return ImeFailure(
            "Win32 IME returned invalid composition bytes");
    }
    if (byteCount == 0) {
        output.Clear();
        return {};
    }
    const std::uint32_t characterCount =
        static_cast<std::uint32_t>(
            byteCount / static_cast<LONG>(sizeof(wchar_t)));
    Base::Vector<wchar_t> wide;
    Base::Result<void> resized =
        wide.TryResize(characterCount);
    if (!resized) return resized;
    const LONG copied = ImmGetCompositionStringW(
        input.Get(), kind, wide.Data(),
        static_cast<DWORD>(byteCount));
    if (copied != byteCount ||
        characterCount > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())) {
        return ImeFailure(
            "Win32 IME composition read failed");
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        wide.Data(), static_cast<int>(characterCount),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidUtf8,
            "Win32 IME composition is not valid Unicode");
    }
    Base::Vector<char> bytes;
    Base::Result<void> byteStorage = bytes.TryResize(
        static_cast<std::uint32_t>(required));
    if (!byteStorage) return byteStorage;
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.Data(), static_cast<int>(characterCount),
            bytes.Data(), required,
            nullptr, nullptr) != required) {
        return ImeFailure(
            "Win32 IME UTF-8 conversion failed");
    }
    return output.TryAssignUnchecked({
        bytes.Data(),
        static_cast<std::uint32_t>(required)});
}

double WindowDpiScale(
    HWND window,
    double fallback) noexcept {
    using GetDpiForWindowFunction = UINT (WINAPI*)(HWND);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto getDpi = user32 != nullptr
        ? reinterpret_cast<GetDpiForWindowFunction>(
              GetProcAddress(user32, "GetDpiForWindow"))
        : nullptr;
    if (getDpi != nullptr) {
        const UINT dpi = getDpi(window);
        if (dpi != 0U) {
            return static_cast<double>(dpi) / 96.0;
        }
    }
    return fallback;
}

LRESULT CALLBACK ImeWindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) noexcept {
    return static_cast<LRESULT>(
        DispatchWin32ImeWindowMessage(
            window,
            static_cast<std::uint32_t>(message),
            static_cast<std::uintptr_t>(wParam),
            static_cast<std::intptr_t>(lParam)));
}
#endif

} // namespace

std::intptr_t DispatchWin32ImeWindowMessage(
    void* window,
    std::uint32_t message,
    std::uintptr_t wParam,
    std::intptr_t lParam) noexcept {
#if defined(_WIN32)
    const HWND nativeWindow = static_cast<HWND>(window);
    auto* adapter = static_cast<Win32ImeAdapter*>(
        GetPropW(nativeWindow, ImeAdapterPropertyName()));
    if (adapter == nullptr) {
        return static_cast<std::intptr_t>(
            DefWindowProcW(
                nativeWindow,
                static_cast<UINT>(message),
                static_cast<WPARAM>(wParam),
                static_cast<LPARAM>(lParam)));
    }
    const auto previous = reinterpret_cast<WNDPROC>(
        adapter->previousProcedure_);
    if (message == WM_NCDESTROY) {
        const LRESULT result = previous != nullptr
            ? CallWindowProcW(
                  previous, nativeWindow,
                  static_cast<UINT>(message),
                  static_cast<WPARAM>(wParam),
                  static_cast<LPARAM>(lParam))
            : DefWindowProcW(
                  nativeWindow,
                  static_cast<UINT>(message),
                  static_cast<WPARAM>(wParam),
                  static_cast<LPARAM>(lParam));
        adapter->DestroyNativeCaret();
        static_cast<void>(RemovePropW(
            nativeWindow, ImeAdapterPropertyName()));
        adapter->window_ = nullptr;
        adapter->previousProcedure_ = nullptr;
        adapter->composing_ = false;
        return static_cast<std::intptr_t>(result);
    }
    if (message == WM_IME_STARTCOMPOSITION ||
        message == WM_IME_COMPOSITION ||
        message == WM_IME_ENDCOMPOSITION) {
        Base::Result<bool> handled = adapter->HandleMessage(
            message, wParam, lParam);
        if (handled && handled.Value()) {
            static_cast<void>(InvalidateRect(
                nativeWindow, nullptr, FALSE));
            return 0;
        }
        if (!handled) {
            static_cast<void>(InvalidateRect(
                nativeWindow, nullptr, FALSE));
            return 0;
        }
    }
    const LRESULT result = previous != nullptr
        ? CallWindowProcW(
              previous, nativeWindow,
              static_cast<UINT>(message),
              static_cast<WPARAM>(wParam),
              static_cast<LPARAM>(lParam))
        : DefWindowProcW(
              nativeWindow,
              static_cast<UINT>(message),
              static_cast<WPARAM>(wParam),
              static_cast<LPARAM>(lParam));
    return static_cast<std::intptr_t>(result);
#else
    static_cast<void>(window);
    static_cast<void>(message);
    static_cast<void>(wParam);
    static_cast<void>(lParam);
    return 0;
#endif
}

Win32ImeAdapter::~Win32ImeAdapter() {
    static_cast<void>(Detach());
}

Base::Result<void> Win32ImeAdapter::Attach(
    void* window) noexcept {
#if defined(_WIN32)
    const HWND nativeWindow = static_cast<HWND>(window);
    if (nativeWindow == nullptr ||
        IsWindow(nativeWindow) == FALSE) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Win32 IME requires a valid window");
    }
    if (window_ == nativeWindow) return {};
    if (IsAttached()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Win32 IME adapter is already attached");
    }
    if (SetPropW(
            nativeWindow,
            ImeAdapterPropertyName(),
            this) == FALSE) {
        return ImeFailure(
            "Win32 failed to associate the IME adapter with the window");
    }
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(
        nativeWindow,
        GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&ImeWindowProcedure));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        static_cast<void>(RemovePropW(
            nativeWindow, ImeAdapterPropertyName()));
        return ImeFailure(
            "Win32 failed to subclass the IME window");
    }
    window_ = nativeWindow;
    previousProcedure_ = reinterpret_cast<void*>(previous);
    composing_ = false;
    return {};
#else
    static_cast<void>(window);
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

Base::Result<bool>
Win32ImeAdapter::AttachActiveWindow() noexcept {
#if defined(_WIN32)
    HWND window = GetFocus();
    if (window == nullptr) window = GetActiveWindow();
    if (window == nullptr) return false;
    if (window_ == window) return false;
    if (IsAttached()) {
        Base::Result<bool> detached = Detach();
        if (!detached) return detached.GetStatus();
    }
    Base::Result<void> attached = Attach(window);
    if (!attached) return attached.GetStatus();
    return true;
#else
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

Base::Result<bool> Win32ImeAdapter::Detach() noexcept {
    if (!IsAttached()) return false;
#if defined(_WIN32)
    Base::Status failure;
    if (composing_) {
        Base::Result<void> native =
            CancelNativeComposition();
        if (!native) failure = native.GetStatus();
        if (client_ != nullptr) {
            Base::Result<void> client =
                client_->CancelComposition();
            if (!client && failure.IsOk()) {
                failure = client.GetStatus();
            }
        }
    }
    DestroyNativeCaret();
    const HWND nativeWindow = static_cast<HWND>(window_);
    if (IsWindow(nativeWindow) != FALSE) {
        const auto current = reinterpret_cast<WNDPROC>(
            GetWindowLongPtrW(nativeWindow, GWLP_WNDPROC));
        if (current == &ImeWindowProcedure &&
            previousProcedure_ != nullptr) {
            SetLastError(ERROR_SUCCESS);
            if (SetWindowLongPtrW(
                    nativeWindow,
                    GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(
                        previousProcedure_)) == 0 &&
                GetLastError() != ERROR_SUCCESS &&
                failure.IsOk()) {
                failure = ImeFailure(
                    "Win32 failed to restore the window procedure");
            }
        }
        static_cast<void>(RemovePropW(
            nativeWindow, ImeAdapterPropertyName()));
    }
    composing_ = false;
    window_ = nullptr;
    previousProcedure_ = nullptr;
    if (!failure.IsOk()) return failure;
    return true;
#else
    window_ = nullptr;
    previousProcedure_ = nullptr;
    composing_ = false;
    client_ = nullptr;
    return true;
#endif
}

void Win32ImeAdapter::SetClient(
    Integration::ITextCompositionClient* client) noexcept {
#if defined(_WIN32)
    if (client != nullptr &&
        client_ != nullptr &&
        client_ != client && composing_) {
        return;
    }
    if (client == nullptr && composing_) {
        if (IsAttached()) {
            Base::Result<void> cancelled =
                CancelNativeComposition();
            if (!cancelled) return;
        } else {
            composing_ = false;
            DestroyNativeCaret();
        }
    }
    client_ = client;
    if (client_ == nullptr) DestroyNativeCaret();
#else
    static_cast<void>(client);
#endif
}

Base::Result<bool> Win32ImeAdapter::HandleMessage(
    std::uint32_t message,
    std::uintptr_t wParam,
    std::intptr_t lParam) noexcept {
    static_cast<void>(wParam);
#if defined(_WIN32)
    if (!IsAttached()) return NotAttached();
    if (client_ == nullptr) return false;
    if (message == WM_IME_STARTCOMPOSITION) {
        if (composing_) return true;
        Base::Result<void> begun =
            client_->BeginComposition();
        if (!begun) return begun.GetStatus();
        composing_ = true;
        return true;
    }
    if (message == WM_IME_COMPOSITION) {
        if (!composing_) {
            Base::Result<void> begun =
                client_->BeginComposition();
            if (!begun) return begun.GetStatus();
            composing_ = true;
        }
        const auto flags =
            static_cast<std::uintptr_t>(lParam);
        Base::String text;
        if ((flags & GCS_RESULTSTR) != 0U) {
            Base::Result<void> read = ReadCompositionText(
                static_cast<HWND>(window_),
                GCS_RESULTSTR, text);
            if (!read) return read.GetStatus();
            Base::Result<void> committed =
                client_->CommitComposition(text.View());
            if (!committed) return committed.GetStatus();
            composing_ = false;
            DestroyNativeCaret();
            return true;
        }
        if ((flags & GCS_COMPSTR) != 0U) {
            Base::Result<void> read = ReadCompositionText(
                static_cast<HWND>(window_),
                GCS_COMPSTR, text);
            if (!read) return read.GetStatus();
            Base::Result<void> updated =
                client_->UpdateComposition(text.View());
            if (!updated) return updated.GetStatus();
        }
        return true;
    }
    if (message == WM_IME_ENDCOMPOSITION) {
        if (composing_) {
            Base::Result<void> cancelled =
                client_->CancelComposition();
            if (!cancelled) return cancelled.GetStatus();
            composing_ = false;
        }
        DestroyNativeCaret();
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

void
Win32ImeAdapter::SetCandidateWindow(
    const Integration::ImeCandidateWindow& value) noexcept {
#if defined(_WIN32)
    if (!IsAttached()) {
        Base::Result<bool> attached = AttachActiveWindow();
        if (!attached) return;
        if (!attached.Value() && !IsAttached()) return;
    }
    if (!Base::IsValidRect(value.caret) ||
        !std::isfinite(value.dpiScale) ||
        value.dpiScale <= 0.0) {
        return;
    }
    const HWND nativeWindow = static_cast<HWND>(window_);
    const double scale = WindowDpiScale(
        nativeWindow, value.dpiScale);
    const double x = value.caret.x * scale;
    const double y = (value.caret.y + value.caret.height) * scale;
    const double width = value.caret.width * scale;
    const double height = value.caret.height * scale;
    if (x < static_cast<double>(
            std::numeric_limits<LONG>::min()) ||
        x > static_cast<double>(
            std::numeric_limits<LONG>::max()) ||
        y < static_cast<double>(
            std::numeric_limits<LONG>::min()) ||
        y > static_cast<double>(
            std::numeric_limits<LONG>::max())) {
        return;
    }
    const LONG caretX = static_cast<LONG>(std::lround(x));
    const LONG caretY = static_cast<LONG>(std::lround(y));
    InputContextScope input(nativeWindow);
    if (input.Get() == nullptr) {
        return;
    }
    CANDIDATEFORM candidate{};
    candidate.dwIndex = 0U;
    candidate.dwStyle = CFS_CANDIDATEPOS;
    candidate.ptCurrentPos.x = caretX;
    candidate.ptCurrentPos.y = caretY;
    if (ImmSetCandidateWindow(
            input.Get(), &candidate) == FALSE) {
        return;
    }
    COMPOSITIONFORM composition{};
    composition.dwStyle = CFS_POINT;
    composition.ptCurrentPos.x = caretX;
    composition.ptCurrentPos.y = caretY;
    static_cast<void>(ImmSetCompositionWindow(
        input.Get(), &composition));

    const int nativeWidth = std::max(
        1, static_cast<int>(std::lround(width)));
    const int nativeHeight = std::max(
        1, static_cast<int>(std::lround(height)));
    if (caretCreated_ &&
        (nativeWidth != caretWidth_ ||
         nativeHeight != caretHeight_)) {
        DestroyNativeCaret();
    }
    if (!caretCreated_) {
        if (CreateCaret(
                nativeWindow, nullptr,
                nativeWidth, nativeHeight) == FALSE) {
            return;
        }
        caretCreated_ = true;
        caretWidth_ = nativeWidth;
        caretHeight_ = nativeHeight;
    }
    if (SetCaretPos(caretX, caretY - nativeHeight) == FALSE) {
        return;
    }
    if (!caretVisible_) {
        if (ShowCaret(nativeWindow) == FALSE) {
            return;
        }
        caretVisible_ = true;
    }
#else
    static_cast<void>(value);
#endif
}

void Win32ImeAdapter::DestroyNativeCaret() noexcept {
#if defined(_WIN32)
    if (caretVisible_ && window_ != nullptr) {
        static_cast<void>(HideCaret(
            static_cast<HWND>(window_)));
    }
    if (caretCreated_) {
        static_cast<void>(DestroyCaret());
    }
#endif
    caretCreated_ = false;
    caretVisible_ = false;
    caretWidth_ = 0;
    caretHeight_ = 0;
}

Base::Result<void>
Win32ImeAdapter::CancelNativeComposition() noexcept {
#if defined(_WIN32)
    if (!IsAttached()) return NotAttached();
    InputContextScope input(
        static_cast<HWND>(window_));
    if (input.Get() == nullptr) {
        return ImeFailure(
            "Win32 IME context is unavailable");
    }
    if (ImmNotifyIME(
            input.Get(),
            NI_COMPOSITIONSTR,
            CPS_CANCEL, 0U) == FALSE) {
        return ImeFailure(
            "Win32 failed to cancel IME composition");
    }
    composing_ = false;
    DestroyNativeCaret();
    return {};
#else
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "Win32 IME is unavailable on this platform");
#endif
}

} // namespace Aero::Platform
