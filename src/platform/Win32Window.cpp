#include <Aero/Platform/Win32Window.hpp>

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
    Base::Result<void> resized = output.TryResize(
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

struct Win32Window::Impl final {
    explicit Impl(Base::IAllocator& allocator) noexcept
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
        Base::Result<void> pushed = events.TryPushBack(event);
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
        case WM_LBUTTONUP:
            return WindowPointerButton::Left;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return WindowPointerButton::Right;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return WindowPointerButton::Middle;
        case WM_XBUTTONDOWN:
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
        Impl* self = reinterpret_cast<Impl*>(
            GetWindowLongPtrW(nativeWindow, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* created =
                reinterpret_cast<const CREATESTRUCTW*>(value);
            self = created != nullptr
                ? static_cast<Impl*>(created->lpCreateParams)
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

Win32Window::Win32Window(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::General});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::General);
    }
    impl_ = new (memory) Impl(*allocator_);
}

Win32Window::~Win32Window() {
    Close();
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_,
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::General);
        impl_ = nullptr;
    }
}

Base::Result<void> Win32Window::Create(
    const WindowDescriptor& descriptor) noexcept {
#if defined(_WIN32)
    if (impl_->window != nullptr || impl_->open) {
        return WindowFailure(
            Base::ErrorCode::AlreadyExists,
            "Win32 window is already created");
    }
    if (descriptor.width == 0U || descriptor.height == 0U ||
        descriptor.width > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max()) ||
        descriptor.height > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())) {
        return WindowFailure(
            Base::ErrorCode::InvalidArgument,
            "Win32 window dimensions are invalid");
    }

    impl_->events.Clear();
    impl_->width = descriptor.width;
    impl_->height = descriptor.height;
    impl_->dpiScale = 1.0;
    impl_->pendingHighSurrogate = 0U;
    impl_->instance = GetModuleHandleW(nullptr);
    if (impl_->instance == nullptr) {
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 module handle is unavailable");
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &Impl::WindowProcedure;
    windowClass.hInstance = impl_->instance;
    windowClass.hCursor =
        LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = Win32WindowClassName();
    if (RegisterClassExW(&windowClass) == 0U &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        impl_->instance = nullptr;
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window class registration failed");
    }

    Base::Vector<wchar_t> title(allocator_);
    Base::Result<void> converted = ConvertWindowTitle(
        descriptor.title, title);
    if (!converted) {
        impl_->instance = nullptr;
        return converted.GetStatus();
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!descriptor.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    RECT bounds{
        0,
        0,
        static_cast<LONG>(descriptor.width),
        static_cast<LONG>(descriptor.height)};
    if (AdjustWindowRectEx(
            &bounds, style, FALSE, 0U) == FALSE) {
        impl_->instance = nullptr;
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window bounds adjustment failed");
    }

    impl_->window = CreateWindowExW(
        0U,
        Win32WindowClassName(),
        title.Data(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        impl_->instance,
        impl_);
    if (impl_->window == nullptr) {
        impl_->instance = nullptr;
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "Win32 window creation failed");
    }

    impl_->open = true;
    impl_->UpdateClientMetrics();
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
    if (impl_->window == nullptr || !impl_->open) {
        return WindowFailure(
            Base::ErrorCode::NotInitialized,
            "Win32 window is not created");
    }
    ShowWindow(impl_->window, SW_SHOW);
    if (UpdateWindow(impl_->window) == FALSE) {
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
    if (impl_->Dequeue(event)) {
        return true;
    }
    MSG message{};
    while (PeekMessageW(
               &message, nullptr, 0U, 0U,
               PM_REMOVE) != FALSE) {
        if (message.message == WM_QUIT) {
            impl_->open = false;
        } else {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (impl_->Dequeue(event)) {
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
    if (impl_->Dequeue(event)) {
        return true;
    }
    while (impl_->open) {
        MSG message{};
        const BOOL received = GetMessageW(
            &message, nullptr, 0U, 0U);
        if (received < 0) {
            return WindowFailure(
                Base::ErrorCode::InternalError,
                "Win32 message retrieval failed");
        }
        if (received == 0) {
            impl_->open = false;
            return impl_->Dequeue(event);
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (impl_->Dequeue(event)) {
            return true;
        }
    }
    return impl_->Dequeue(event);
#else
    static_cast<void>(event);
    return UnsupportedWin32Window();
#endif
}

void Win32Window::Close() noexcept {
#if defined(_WIN32)
    HWND nativeWindow = impl_ != nullptr
        ? impl_->window
        : nullptr;
    if (nativeWindow != nullptr &&
        IsWindow(nativeWindow) != FALSE) {
        static_cast<void>(DestroyWindow(nativeWindow));
    }
    if (impl_ != nullptr) {
        impl_->window = nullptr;
        impl_->instance = nullptr;
        impl_->open = false;
        impl_->pendingHighSurrogate = 0U;
    }
#endif
}

bool Win32Window::IsOpen() const noexcept {
    return impl_ != nullptr && impl_->open;
}

std::uint32_t Win32Window::ClientWidth() const noexcept {
    return impl_ != nullptr ? impl_->width : 0U;
}

std::uint32_t Win32Window::ClientHeight() const noexcept {
    return impl_ != nullptr ? impl_->height : 0U;
}

double Win32Window::DpiScale() const noexcept {
    return impl_ != nullptr ? impl_->dpiScale : 1.0;
}

NativeWindowHandle Win32Window::NativeHandle() const noexcept {
    NativeWindowHandle handle;
#if defined(_WIN32)
    if (impl_ != nullptr) {
        handle.system = WindowSystem::Win32;
        handle.window = reinterpret_cast<std::uintptr_t>(impl_->window);
        handle.instance = reinterpret_cast<std::uintptr_t>(impl_->instance);
    }
#endif
    return handle;
}

} // namespace Aero::Platform
