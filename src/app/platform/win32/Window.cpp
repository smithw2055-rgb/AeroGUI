#include "Window.hpp"

#include <Aero/Base/Vector.hpp>

#include <cstdint>
#include <limits>
#include <new>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#endif

namespace Aero::Platform {
namespace {

Base::Status WindowFailure(
    Base::ErrorCode code,
    const char* message) noexcept {
    return Base::Status::Failure(code, message);
}

[[maybe_unused]] Base::Status UnsupportedWin32Window() noexcept {
    return WindowFailure(
        Base::ErrorCode::Unsupported,
        "Win32 window carrier is unavailable on this platform");
}

#if defined(_WIN32)

void EnablePerMonitorDpiAwareness() noexcept {
    // Match the reference Win32 host: the client rectangle and D3D11 surface
    // must use real monitor pixels, never the DPI-virtualized coordinates
    // supplied to legacy processes.
    using SetProcessDpiAwarenessContextFunction = BOOL (WINAPI*)(HANDLE);
    using SetProcessDpiAwarenessFunction = HRESULT (WINAPI*)(int);
    using SetProcessDpiAwareFunction = BOOL (WINAPI*)();

    static const bool initialized = []() noexcept {
        const HMODULE user32 = GetModuleHandleW(L"user32.dll");
        const auto setContext = user32 != nullptr
            ? reinterpret_cast<SetProcessDpiAwarenessContextFunction>(
                  GetProcAddress(
                      user32, "SetProcessDpiAwarenessContext"))
            : nullptr;
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 is intentionally kept
        // as its documented value so this builds with older Windows SDKs.
        if (setContext != nullptr &&
            setContext(reinterpret_cast<HANDLE>(-4)) != FALSE) {
            return true;
        }

        const HMODULE shcore = LoadLibraryW(L"Shcore.dll");
        const auto setAwareness = shcore != nullptr
            ? reinterpret_cast<SetProcessDpiAwarenessFunction>(
                  GetProcAddress(shcore, "SetProcessDpiAwareness"))
            : nullptr;
        const bool perMonitor = setAwareness != nullptr &&
            SUCCEEDED(setAwareness(2)); // PROCESS_PER_MONITOR_DPI_AWARE
        if (shcore != nullptr) {
            FreeLibrary(shcore);
        }
        if (perMonitor) {
            return true;
        }

        const auto setAware = user32 != nullptr
            ? reinterpret_cast<SetProcessDpiAwareFunction>(
                  GetProcAddress(user32, "SetProcessDPIAware"))
            : nullptr;
        return setAware != nullptr && setAware() != FALSE;
    }();
    static_cast<void>(initialized);
}

const wchar_t* Win32WindowClassName() noexcept {
    return L"AeroGuiPlatformWindow";
}

Base::Result<void> ConvertWindowTitle(
    Base::StringView title,
    Base::Vector<wchar_t>& output) noexcept {
    const Base::StringView source = title.Empty()
        ? Base::StringView("AeroGUI")
        : title;
    if (source.SizeBytes() > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())) {
        return WindowFailure(
            Base::ErrorCode::OutOfRange,
            "Win32 window title is too long");
    }
    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        source.Data(),
        static_cast<int>(source.SizeBytes()),
        nullptr,
        0);
    if (required <= 0) {
        return WindowFailure(
            Base::ErrorCode::InvalidUtf8,
            "Win32 window title is not valid UTF-8");
    }
    Base::Result<void> resized = output.Resize(
        static_cast<std::uint32_t>(required) + 1U);
    if (!resized) {
        return resized.GetStatus();
    }
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            source.Data(),
            static_cast<int>(source.SizeBytes()),
            output.Data(),
            required) != required) {
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window title conversion failed");
    }
    output[static_cast<std::uint32_t>(required)] = L'\0';
    return {};
}

#endif

} // namespace

struct Win32WindowState {
    explicit Win32WindowState(Base::IAllocator& allocator) noexcept
        : events(&allocator) {}

    Base::Vector<WindowEvent> events;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    double dpiScale = 1.0;
    bool open = false;

#if defined(_WIN32)
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    std::uint16_t pendingHighSurrogate = 0U;

    void Push(const WindowEvent& event) noexcept {
        Base::Result<void> pushed = events.PushBack(event);
        static_cast<void>(pushed);
    }

    bool Dequeue(WindowEvent& event) noexcept {
        if (events.Empty()) {
            return false;
        }
        event = events[0U];
        for (std::uint32_t index = 1U;
             index < events.Size(); ++index) {
            events[index - 1U] = events[index];
        }
        events.PopBack();
        return true;
    }

    static std::uint32_t Modifiers() noexcept {
        std::uint32_t result = WindowModifierNone;
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            result |= WindowModifierShift;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
            result |= WindowModifierControl;
        }
        if ((GetKeyState(VK_MENU) & 0x8000) != 0) {
            result |= WindowModifierAlt;
        }
        return result;
    }

    static WindowPointerButton ButtonFromMessage(
        UINT message,
        WPARAM word) noexcept {
        switch (message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
            return WindowPointerButton::Left;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONUP:
            return WindowPointerButton::Right;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONUP:
            return WindowPointerButton::Middle;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONUP:
            return GET_XBUTTON_WPARAM(word) == XBUTTON1
                ? WindowPointerButton::XButton1
                : WindowPointerButton::XButton2;
        default:
            return WindowPointerButton::Unknown;
        }
    }

    void UpdateClientMetrics() noexcept {
        if (window == nullptr) {
            return;
        }
        RECT client{};
        if (GetClientRect(window, &client) != FALSE) {
            width = static_cast<std::uint32_t>(
                client.right - client.left);
            height = static_cast<std::uint32_t>(
                client.bottom - client.top);
        }
        using GetDpiForWindowFunction = UINT (WINAPI*)(HWND);
        const HMODULE user32 = GetModuleHandleW(L"user32.dll");
        const auto getDpi = user32 != nullptr
            ? reinterpret_cast<GetDpiForWindowFunction>(
                  GetProcAddress(user32, "GetDpiForWindow"))
            : nullptr;
        dpiScale = getDpi != nullptr
            ? static_cast<double>(getDpi(window)) / 96.0
            : 1.0;
    }

    void QueuePointer(
        WindowEventType type,
        double x,
        double y,
        WindowPointerButton button =
            WindowPointerButton::Unknown) noexcept {
        WindowEvent event;
        event.type = type;
        event.x = x;
        event.y = y;
        event.button = button;
        event.modifiers = Modifiers();
        event.dpiScale = dpiScale;
        Push(event);
    }

    void QueueCodePoint(std::uint32_t codePoint) noexcept {
        if (codePoint > 0x10FFFFU ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            codePoint = 0xFFFDU;
        }
        WindowEvent event;
        event.type = WindowEventType::TextInput;
        if (codePoint <= 0x7FU) {
            event.text[0] = static_cast<char>(codePoint);
            event.textSize = 1U;
        } else if (codePoint <= 0x7FFU) {
            event.text[0] = static_cast<char>(
                0xC0U | (codePoint >> 6U));
            event.text[1] = static_cast<char>(
                0x80U | (codePoint & 0x3FU));
            event.textSize = 2U;
        } else if (codePoint <= 0xFFFFU) {
            event.text[0] = static_cast<char>(
                0xE0U | (codePoint >> 12U));
            event.text[1] = static_cast<char>(
                0x80U | ((codePoint >> 6U) & 0x3FU));
            event.text[2] = static_cast<char>(
                0x80U | (codePoint & 0x3FU));
            event.textSize = 3U;
        } else {
            event.text[0] = static_cast<char>(
                0xF0U | (codePoint >> 18U));
            event.text[1] = static_cast<char>(
                0x80U | ((codePoint >> 12U) & 0x3FU));
            event.text[2] = static_cast<char>(
                0x80U | ((codePoint >> 6U) & 0x3FU));
            event.text[3] = static_cast<char>(
                0x80U | (codePoint & 0x3FU));
            event.textSize = 4U;
        }
        Push(event);
    }

    void QueueUtf16(std::uint16_t value) noexcept {
        if (value >= 0xD800U && value <= 0xDBFFU) {
            if (pendingHighSurrogate != 0U) {
                QueueCodePoint(0xFFFDU);
            }
            pendingHighSurrogate = value;
            return;
        }
        if (value >= 0xDC00U && value <= 0xDFFFU) {
            if (pendingHighSurrogate == 0U) {
                QueueCodePoint(0xFFFDU);
                return;
            }
            const std::uint32_t codePoint =
                0x10000U +
                ((static_cast<std::uint32_t>(
                      pendingHighSurrogate) - 0xD800U) << 10U) +
                (static_cast<std::uint32_t>(value) - 0xDC00U);
            pendingHighSurrogate = 0U;
            QueueCodePoint(codePoint);
            return;
        }
        if (pendingHighSurrogate != 0U) {
            pendingHighSurrogate = 0U;
            QueueCodePoint(0xFFFDU);
        }
        QueueCodePoint(value);
    }

    LRESULT HandleMessage(
        HWND nativeWindow,
        UINT message,
        WPARAM word,
        LPARAM value) noexcept {
        switch (message) {
        case WM_CLOSE: {
            open = false;
            WindowEvent event;
            event.type = WindowEventType::CloseRequested;
            Push(event);
            return 0;
        }
        case WM_DESTROY: {
            open = false;
            window = nullptr;
            WindowEvent event;
            event.type = WindowEventType::Closed;
            Push(event);
            return 0;
        }
        case WM_SIZE: {
            const std::uint32_t resizedWidth = LOWORD(value);
            const std::uint32_t resizedHeight = HIWORD(value);
            if (resizedWidth != 0U && resizedHeight != 0U) {
                width = resizedWidth;
                height = resizedHeight;
                WindowEvent event;
                event.type = WindowEventType::Resized;
                event.width = width;
                event.height = height;
                event.dpiScale = dpiScale;
                Push(event);
            }
            return 0;
        }
        case WM_DPICHANGED: {
            dpiScale = static_cast<double>(HIWORD(word)) / 96.0;
            const auto* suggested =
                reinterpret_cast<const RECT*>(value);
            if (suggested != nullptr) {
                static_cast<void>(SetWindowPos(
                    nativeWindow,
                    nullptr,
                    suggested->left,
                    suggested->top,
                    suggested->right - suggested->left,
                    suggested->bottom - suggested->top,
                    SWP_NOACTIVATE | SWP_NOZORDER));
            }
            UpdateClientMetrics();
            WindowEvent event;
            event.type = WindowEventType::ScaleChanged;
            event.width = width;
            event.height = height;
            event.dpiScale = dpiScale;
            Push(event);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(nativeWindow, &paint);
            EndPaint(nativeWindow, &paint);
            WindowEvent event;
            event.type = WindowEventType::Exposed;
            event.width = width;
            event.height = height;
            event.dpiScale = dpiScale;
            Push(event);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE:
            QueuePointer(
                WindowEventType::PointerMove,
                static_cast<double>(GET_X_LPARAM(value)),
                static_cast<double>(GET_Y_LPARAM(value)));
            return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
            static_cast<void>(SetCapture(nativeWindow));
            QueuePointer(
                WindowEventType::PointerDown,
                static_cast<double>(GET_X_LPARAM(value)),
                static_cast<double>(GET_Y_LPARAM(value)),
                ButtonFromMessage(message, word));
            return message == WM_XBUTTONDOWN ? TRUE : 0;
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDBLCLK:
            static_cast<void>(SetCapture(nativeWindow));
            QueuePointer(
                WindowEventType::PointerDoubleClick,
                static_cast<double>(GET_X_LPARAM(value)),
                static_cast<double>(GET_Y_LPARAM(value)),
                ButtonFromMessage(message, word));
            return message == WM_XBUTTONDBLCLK ? TRUE : 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
            static_cast<void>(ReleaseCapture());
            QueuePointer(
                WindowEventType::PointerUp,
                static_cast<double>(GET_X_LPARAM(value)),
                static_cast<double>(GET_Y_LPARAM(value)),
                ButtonFromMessage(message, word));
            return message == WM_XBUTTONUP ? TRUE : 0;
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            POINT point{
                GET_X_LPARAM(value),
                GET_Y_LPARAM(value)};
            static_cast<void>(ScreenToClient(nativeWindow, &point));
            WindowEvent event;
            event.type = WindowEventType::PointerWheel;
            event.x = static_cast<double>(point.x);
            event.y = static_cast<double>(point.y);
            const double delta = static_cast<double>(
                GET_WHEEL_DELTA_WPARAM(word));
            if (message == WM_MOUSEWHEEL) {
                event.wheelDeltaY = delta;
            } else {
                event.wheelDeltaX = delta;
            }
            event.modifiers = Modifiers();
            event.dpiScale = dpiScale;
            Push(event);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            WindowEvent event;
            event.type =
                message == WM_KEYDOWN || message == WM_SYSKEYDOWN
                ? WindowEventType::KeyDown
                : WindowEventType::KeyUp;
            event.key = static_cast<std::uint32_t>(word);
            event.modifiers = Modifiers();
            event.repeat = (static_cast<std::uintptr_t>(value) &
                (UINT64_C(1) << 30U)) != 0U;
            Push(event);
            return 0;
        }
        case WM_CHAR:
            QueueUtf16(static_cast<std::uint16_t>(word));
            return 0;
        case WM_UNICHAR:
            if (word == UNICODE_NOCHAR) {
                return TRUE;
            }
            QueueCodePoint(static_cast<std::uint32_t>(word));
            return 0;
        default:
            break;
        }
        return DefWindowProcW(nativeWindow, message, word, value);
    }

    static LRESULT CALLBACK WindowProcedure(
        HWND nativeWindow,
        UINT message,
        WPARAM word,
        LPARAM value) noexcept {
        Win32WindowState* self = reinterpret_cast<Win32WindowState*>(
            GetWindowLongPtrW(nativeWindow, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* created =
                reinterpret_cast<const CREATESTRUCTW*>(value);
            self = created != nullptr
                ? static_cast<Win32WindowState*>(created->lpCreateParams)
                : nullptr;
            if (self != nullptr) {
                self->window = nativeWindow;
                SetWindowLongPtrW(
                    nativeWindow,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(self));
            }
        }
        return self != nullptr
            ? self->HandleMessage(nativeWindow, message, word, value)
            : DefWindowProcW(nativeWindow, message, word, value);
    }
#endif
};

static_assert(sizeof(Win32WindowState) <= 8192U,
    "Win32Window inline state storage is too small");
static_assert(alignof(Win32WindowState) <= alignof(std::max_align_t),
    "Win32Window inline state alignment is insufficient");

Win32Window::Win32Window(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) Win32WindowState(*allocator_);
}

void ApplyDarkWindowChrome(HWND window) noexcept {
    if (window == nullptr) return;
    const HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (dwm == nullptr) return;
    using DwmSetWindowAttributeFunction = HRESULT (WINAPI*)(
        HWND, DWORD, LPCVOID, DWORD);
    const auto setAttribute =
        reinterpret_cast<DwmSetWindowAttributeFunction>(
            GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (setAttribute != nullptr) {
        const BOOL enabled = TRUE;
        // DWMWA_USE_IMMERSIVE_DARK_MODE is 20 on supported Windows 10/11
        // builds; 19 is retained as the compatibility value used by older
        // Windows 10 releases.
        HRESULT applied = setAttribute(
            window, 20U, &enabled, sizeof(enabled));
        if (FAILED(applied)) {
            static_cast<void>(setAttribute(
                window, 19U, &enabled, sizeof(enabled)));
        }
    }
    FreeLibrary(dwm);
}

Win32Window::~Win32Window() {
    Close();
    if (state_ != nullptr) {
        state_->~Win32WindowState();
        state_ = nullptr;
    }
}

Base::Result<void> Win32Window::Create(
    const WindowDescriptor& descriptor) noexcept {
#if defined(_WIN32)
    if (state_->window != nullptr || state_->open) {
        return WindowFailure(
            Base::ErrorCode::AlreadyExists,
            "Win32 window is already created");
    }
    const bool platformDefaultSize =
        descriptor.width == 0U &&
        descriptor.height == 0U;
    if ((descriptor.width == 0U) !=
            (descriptor.height == 0U) ||
        descriptor.width > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max()) ||
        descriptor.height > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())) {
        return WindowFailure(
            Base::ErrorCode::InvalidArgument,
            "Win32 window dimensions are invalid");
    }

    state_->events.Clear();
    state_->width = descriptor.width;
    state_->height = descriptor.height;
    state_->dpiScale = 1.0;
    state_->pendingHighSurrogate = 0U;
    EnablePerMonitorDpiAwareness();
    state_->instance = GetModuleHandleW(nullptr);
    if (state_->instance == nullptr) {
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 module handle is unavailable");
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = &Win32WindowState::WindowProcedure;
    windowClass.hInstance = state_->instance;
    windowClass.hCursor =
        LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = Win32WindowClassName();
    if (RegisterClassExW(&windowClass) == 0U &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        state_->instance = nullptr;
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window class registration failed");
    }

    Base::Vector<wchar_t> title(allocator_);
    Base::Result<void> converted = ConvertWindowTitle(
        descriptor.title, title);
    if (!converted) {
        state_->instance = nullptr;
        return converted.GetStatus();
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!descriptor.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    RECT bounds{};
    int outerWidth = CW_USEDEFAULT;
    int outerHeight = CW_USEDEFAULT;
    if (!platformDefaultSize) {
        bounds.right =
            static_cast<LONG>(descriptor.width);
        bounds.bottom =
            static_cast<LONG>(descriptor.height);
        if (AdjustWindowRectEx(
                &bounds, style, FALSE, 0U) == FALSE) {
            state_->instance = nullptr;
            return WindowFailure(
                Base::ErrorCode::InternalError,
                "Win32 window bounds adjustment failed");
        }
        outerWidth = bounds.right - bounds.left;
        outerHeight = bounds.bottom - bounds.top;
    }

    state_->window = CreateWindowExW(
        0U,
        Win32WindowClassName(),
        title.Data(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        outerWidth,
        outerHeight,
        nullptr,
        nullptr,
        state_->instance,
        state_);
    if (state_->window == nullptr) {
        state_->instance = nullptr;
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window creation failed");
    }

    ApplyDarkWindowChrome(state_->window);

    state_->open = true;
    state_->UpdateClientMetrics();
    if (descriptor.visible) {
        Base::Result<void> shown = Show();
        if (!shown) {
            Close();
            return shown.GetStatus();
        }
    }
    return {};
#else
    static_cast<void>(descriptor);
    return UnsupportedWin32Window();
#endif
}

Base::Result<void> Win32Window::Show() noexcept {
#if defined(_WIN32)
    if (state_->window == nullptr || !state_->open) {
        return WindowFailure(
            Base::ErrorCode::NotInitialized,
            "Win32 window is not created");
    }
    ShowWindow(state_->window, SW_SHOW);
    if (UpdateWindow(state_->window) == FALSE) {
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window update failed");
    }
    return {};
#else
    return UnsupportedWin32Window();
#endif
}

Base::Result<bool> Win32Window::PollEvent(
    WindowEvent& event) noexcept {
#if defined(_WIN32)
    if (state_->Dequeue(event)) {
        return true;
    }
    MSG message{};
    while (PeekMessageW(
               &message, nullptr, 0U, 0U,
               PM_REMOVE) != FALSE) {
        if (message.message == WM_QUIT) {
            state_->open = false;
        } else {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (state_->Dequeue(event)) {
            return true;
        }
    }
    return false;
#else
    static_cast<void>(event);
    return UnsupportedWin32Window();
#endif
}

Base::Result<bool> Win32Window::WaitEvent(
    WindowEvent& event) noexcept {
#if defined(_WIN32)
    if (state_->Dequeue(event)) {
        return true;
    }
    while (state_->open) {
        MSG message{};
        const BOOL received = GetMessageW(
            &message, nullptr, 0U, 0U);
        if (received < 0) {
            return WindowFailure(
                Base::ErrorCode::InternalError,
                "Win32 message retrieval failed");
        }
        if (received == 0) {
            state_->open = false;
            return state_->Dequeue(event);
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (state_->Dequeue(event)) {
            return true;
        }
    }
    return state_->Dequeue(event);
#else
    static_cast<void>(event);
    return UnsupportedWin32Window();
#endif
}

void Win32Window::Close() noexcept {
#if defined(_WIN32)
    HWND nativeWindow = state_ != nullptr
        ? state_->window
        : nullptr;
    if (nativeWindow != nullptr &&
        IsWindow(nativeWindow) != FALSE) {
        static_cast<void>(DestroyWindow(nativeWindow));
    }
    if (state_ != nullptr) {
        state_->window = nullptr;
        state_->instance = nullptr;
        state_->open = false;
        state_->pendingHighSurrogate = 0U;
    }
#endif
}

bool Win32Window::IsOpen() const noexcept {
    return state_ != nullptr && state_->open;
}

std::uint32_t Win32Window::ClientWidth() const noexcept {
    return state_ != nullptr ? state_->width : 0U;
}

std::uint32_t Win32Window::ClientHeight() const noexcept {
    return state_ != nullptr ? state_->height : 0U;
}

double Win32Window::DpiScale() const noexcept {
    return state_ != nullptr ? state_->dpiScale : 1.0;
}

NativeWindowHandle Win32Window::NativeHandle() const noexcept {
    NativeWindowHandle handle;
#if defined(_WIN32)
    if (state_ != nullptr) {
        handle.system = WindowSystem::Win32;
        handle.window = reinterpret_cast<std::uintptr_t>(state_->window);
        handle.instance = reinterpret_cast<std::uintptr_t>(state_->instance);
    }
#endif
    return handle;
}

} // namespace Aero::Platform
