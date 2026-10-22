#include "Window.hpp"

#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>
#include <limits>

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

void EnsureInteractiveDesktop() noexcept {
    const HWINSTA station = GetProcessWindowStation();
    if (station != nullptr) {
        wchar_t name[256]{};
        DWORD needed = 0U;
        if (GetUserObjectInformationW(
                station, UOI_NAME, name, sizeof(name), &needed) != FALSE) {
            if (_wcsicmp(name, L"WinSta0") == 0) {
                const HDESK desktop = OpenDesktopW(
                    L"Default", 0U, FALSE, GENERIC_ALL);
                if (desktop != nullptr) {
                    static_cast<void>(SetThreadDesktop(desktop));
                    CloseDesktop(desktop);
                }
            }
        }
    }
}

UINT QuerySystemDpi() noexcept {
    using GetDpiForSystemFunction = UINT (WINAPI*)();
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto getDpiForSystem = user32 != nullptr
        ? reinterpret_cast<GetDpiForSystemFunction>(
              GetProcAddress(user32, "GetDpiForSystem"))
        : nullptr;
    if (getDpiForSystem != nullptr) {
        const UINT dpi = getDpiForSystem();
        if (dpi != 0U) {
            return dpi;
        }
    }
    return 96U;
}

BOOL AdjustWindowRectForDpi(
    RECT* bounds,
    DWORD style,
    UINT dpi) noexcept {
    using AdjustWindowRectExForDpiFunction =
        BOOL (WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto adjustForDpi = user32 != nullptr
        ? reinterpret_cast<AdjustWindowRectExForDpiFunction>(
              GetProcAddress(user32, "AdjustWindowRectExForDpi"))
        : nullptr;
    if (adjustForDpi != nullptr) {
        return adjustForDpi(bounds, style, FALSE, 0U, dpi);
    }
    return AdjustWindowRectEx(bounds, style, FALSE, 0U);
}

LONG DipToPhysical(
    std::uint32_t dips,
    UINT dpi) noexcept {
    return static_cast<LONG>(
        (static_cast<std::uint64_t>(dips) *
             static_cast<std::uint64_t>(dpi) +
         48ULL) /
        96ULL);
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
    output.Resize(
        static_cast<std::uint32_t>(required) + 1U);
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

std::uint32_t Win32Modifiers() noexcept {
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

WindowPointerButton Win32ButtonFromMessage(
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

#endif

} // namespace

Win32Window::Win32Window(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      events_(allocator_) {
}

Win32Window::~Win32Window() {
    Close();
}

#if defined(_WIN32)

void Win32Window::Push(const WindowEvent& event) noexcept {
    events_.PushBack(event);
}

bool Win32Window::Dequeue(WindowEvent& event) noexcept {
    if (eventHead_ >= events_.Size()) {
        eventHead_ = 0U;
        events_.Clear();
        return false;
    }
    event = events_[eventHead_++];
    if (eventHead_ >= events_.Size()) {
        eventHead_ = 0U;
        events_.Clear();
    }
    return true;
}

void Win32Window::UpdateClientMetrics() noexcept {
    if (window_ == nullptr) {
        return;
    }
    RECT client{};
    if (GetClientRect(window_, &client) != FALSE) {
        width_ = static_cast<std::uint32_t>(
            client.right - client.left);
        height_ = static_cast<std::uint32_t>(
            client.bottom - client.top);
    }
    using GetDpiForWindowFunction = UINT (WINAPI*)(HWND);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto getDpi = user32 != nullptr
        ? reinterpret_cast<GetDpiForWindowFunction>(
              GetProcAddress(user32, "GetDpiForWindow"))
        : nullptr;
    dpiScale_ = getDpi != nullptr
        ? static_cast<double>(getDpi(window_)) / 96.0
        : 1.0;
}

void Win32Window::QueuePointer(
    WindowEventType type,
    double x,
    double y,
    WindowPointerButton button) noexcept {
    WindowEvent event;
    event.type = type;
    event.x = x;
    event.y = y;
    event.button = button;
    event.modifiers = Win32Modifiers();
    event.dpiScale = dpiScale_;
    Push(event);
}

void Win32Window::QueueKey(
    WindowEventType type,
    std::uint32_t key,
    bool repeat) noexcept {
    WindowEvent event;
    event.type = type;
    event.key = key;
    event.modifiers = Win32Modifiers();
    event.repeat = repeat;
    Push(event);
}

void Win32Window::QueueCodePoint(std::uint32_t codePoint) noexcept {
    if (codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        codePoint = 0xFFFDU;
    }
    WindowEvent event;
    event.type = WindowEventType::TextInput;
    event.textSize = static_cast<std::uint8_t>(
        Base::EncodeCodePoint(codePoint, event.text));
    Push(event);
}

void Win32Window::QueueUtf16(std::uint16_t value) noexcept {
    if (value >= 0xD800U && value <= 0xDBFFU) {
        if (pendingHighSurrogate_ != 0U) {
            QueueCodePoint(0xFFFDU);
        }
        pendingHighSurrogate_ = value;
        return;
    }
    if (value >= 0xDC00U && value <= 0xDFFFU) {
        if (pendingHighSurrogate_ == 0U) {
            QueueCodePoint(0xFFFDU);
            return;
        }
        const std::uint32_t codePoint =
            0x10000U +
            ((static_cast<std::uint32_t>(
                  pendingHighSurrogate_) - 0xD800U) << 10U) +
            (static_cast<std::uint32_t>(value) - 0xDC00U);
        pendingHighSurrogate_ = 0U;
        QueueCodePoint(codePoint);
        return;
    }
    if (pendingHighSurrogate_ != 0U) {
        pendingHighSurrogate_ = 0U;
        QueueCodePoint(0xFFFDU);
    }
    QueueCodePoint(value);
}

LRESULT Win32Window::HandleMessage(
    HWND nativeWindow,
    UINT message,
    WPARAM word,
    LPARAM value) noexcept {
    switch (message) {
    case WM_CLOSE: {
        // Do not mark the native window closed here. WPF raises Closing
        // first; DestroyWindow runs only if Close() is not cancelled.
        // Do not PostQuitMessage from this handler: DXGI Present() pumps
        // messages, and WM_QUIT consumed inside Present leaves the host
        // looping with a still-open HWND / swapchain.
        WindowEvent event;
        event.type = WindowEventType::CloseRequested;
        Push(event);
        return 0;
    }
    case WM_DESTROY: {
        open_ = false;
        window_ = nullptr;
        WindowEvent event;
        event.type = WindowEventType::Closed;
        Push(event);
        PostQuitMessage(0);
        return 0;
    }
    case WM_SIZE: {
        const std::uint32_t resizedWidth = LOWORD(value);
        const std::uint32_t resizedHeight = HIWORD(value);
        if (resizedWidth != 0U && resizedHeight != 0U) {
            width_ = resizedWidth;
            height_ = resizedHeight;
            WindowEvent event;
            event.type = WindowEventType::Resized;
            event.width = width_;
            event.height = height_;
            event.dpiScale = dpiScale_;
            Push(event);
        }
        return 0;
    }
    case WM_DPICHANGED: {
        dpiScale_ = static_cast<double>(HIWORD(word)) / 96.0;
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
        event.width = width_;
        event.height = height_;
        event.dpiScale = dpiScale_;
        Push(event);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(nativeWindow, &paint);
        EndPaint(nativeWindow, &paint);
        WindowEvent event;
        event.type = WindowEventType::Exposed;
        event.width = width_;
        event.height = height_;
        event.dpiScale = dpiScale_;
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
            Win32ButtonFromMessage(message, word));
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
            Win32ButtonFromMessage(message, word));
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
            Win32ButtonFromMessage(message, word));
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
        event.modifiers = Win32Modifiers();
        event.dpiScale = dpiScale_;
        Push(event);
        return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        QueueKey(
            message == WM_KEYDOWN || message == WM_SYSKEYDOWN
                ? WindowEventType::KeyDown
                : WindowEventType::KeyUp,
            static_cast<std::uint32_t>(word),
            (static_cast<std::uintptr_t>(value) &
                (UINT64_C(1) << 30U)) != 0U);
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

LRESULT CALLBACK Win32Window::WindowProcedure(
    HWND nativeWindow,
    UINT message,
    WPARAM word,
    LPARAM value) noexcept {
    Win32Window* self = reinterpret_cast<Win32Window*>(
        GetWindowLongPtrW(nativeWindow, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* created =
            reinterpret_cast<const CREATESTRUCTW*>(value);
        self = created != nullptr
            ? static_cast<Win32Window*>(created->lpCreateParams)
            : nullptr;
        if (self != nullptr) {
            self->window_ = nativeWindow;
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

#endif // defined(_WIN32)

Base::Result<void> Win32Window::Create(
    const WindowDescriptor& descriptor) noexcept {
#if defined(_WIN32)
    if (window_ != nullptr || open_) {
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

    events_.Clear();
    width_ = descriptor.width;
    height_ = descriptor.height;
    dpiScale_ = 1.0;
    pendingHighSurrogate_ = 0U;
    EnsureInteractiveDesktop();
    EnablePerMonitorDpiAwareness();
    const UINT dpi = QuerySystemDpi();
    dpiScale_ = static_cast<double>(dpi) / 96.0;
    instance_ = GetModuleHandleW(nullptr);
    if (instance_ == nullptr) {
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 module handle is unavailable");
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = &Win32Window::WindowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor =
        LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = Win32WindowClassName();
    if (RegisterClassExW(&windowClass) == 0U &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        instance_ = nullptr;
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window class registration failed");
    }

    Base::Vector<wchar_t> title(allocator_);
    Base::Result<void> converted = ConvertWindowTitle(
        descriptor.title, title);
    if (!converted) {
        instance_ = nullptr;
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
        // XAML Window Width/Height are device-independent pixels. The Win32
        // client must be physical pixels so layout DIPs match the viewport.
        bounds.right = DipToPhysical(descriptor.width, dpi);
        bounds.bottom = DipToPhysical(descriptor.height, dpi);
        if (AdjustWindowRectForDpi(
                &bounds, style, dpi) == FALSE) {
            instance_ = nullptr;
            return WindowFailure(
                Base::ErrorCode::InternalError,
                "Win32 window bounds adjustment failed");
        }
        outerWidth = bounds.right - bounds.left;
        outerHeight = bounds.bottom - bounds.top;
    }

    window_ = CreateWindowExW(
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
        instance_,
        this);
    if (window_ == nullptr) {
        instance_ = nullptr;
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window creation failed");
    }

    ApplyDarkWindowChrome(window_);

    open_ = true;
    UpdateClientMetrics();
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
    if (window_ == nullptr || !open_) {
        return WindowFailure(
            Base::ErrorCode::NotInitialized,
            "Win32 window is not created");
    }
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
    BringWindowToTop(window_);
    if (UpdateWindow(window_) == FALSE) {
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
    if (Dequeue(event)) {
        return true;
    }
    MSG message{};
    while (PeekMessageW(
               &message, nullptr, 0U, 0U,
               PM_REMOVE) != FALSE) {
        if (message.message == WM_QUIT) {
            open_ = false;
            WindowEvent quit;
            quit.type = WindowEventType::CloseRequested;
            Push(quit);
        } else {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (Dequeue(event)) {
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
    if (Dequeue(event)) {
        return true;
    }
    while (open_) {
        MSG message{};
        const BOOL received = GetMessageW(
            &message, nullptr, 0U, 0U);
        if (received < 0) {
            return WindowFailure(
                Base::ErrorCode::InternalError,
                "Win32 message retrieval failed");
        }
        if (received == 0) {
            open_ = false;
            WindowEvent quit;
            quit.type = WindowEventType::CloseRequested;
            Push(quit);
            return Dequeue(event);
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (Dequeue(event)) {
            return true;
        }
    }
    return Dequeue(event);
#else
    static_cast<void>(event);
    return UnsupportedWin32Window();
#endif
}

void Win32Window::Close() noexcept {
#if defined(_WIN32)
    HWND nativeWindow = window_;
    if (nativeWindow != nullptr &&
        IsWindow(nativeWindow) != FALSE) {
        static_cast<void>(DestroyWindow(nativeWindow));
    }
    window_ = nullptr;
    instance_ = nullptr;
    open_ = false;
    pendingHighSurrogate_ = 0U;
#endif
}

bool Win32Window::IsOpen() const noexcept {
#if defined(_WIN32)
    return open_ &&
        window_ != nullptr &&
        IsWindow(window_) != FALSE;
#else
    return open_;
#endif
}

std::uint32_t Win32Window::ClientWidth() const noexcept {
    return width_;
}

std::uint32_t Win32Window::ClientHeight() const noexcept {
    return height_;
}

double Win32Window::DpiScale() const noexcept {
    return dpiScale_;
}

NativeWindowHandle Win32Window::NativeHandle() const noexcept {
    NativeWindowHandle handle;
#if defined(_WIN32)
    handle.system = WindowSystem::Win32;
    handle.window = reinterpret_cast<std::uintptr_t>(window_);
    handle.instance = reinterpret_cast<std::uintptr_t>(instance_);
#endif
    return handle;
}

} // namespace Aero::Platform
