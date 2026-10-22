#include "Window.hpp"

#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>

#include <cmath>
#include <cstdint>
#include <limits>

#if AERO_PLATFORM_HAS_X11_WINDOW
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#ifdef Status
#undef Status
#endif
#endif

namespace Aero::Platform {
namespace {

Base::Status WindowFailure(
    Base::ErrorCode code,
    const char* message) noexcept {
    return Base::Status::Failure(code, message);
}

[[maybe_unused]] Base::Status UnsupportedX11Window() noexcept {
    return WindowFailure(
        Base::ErrorCode::Unsupported,
        "X11 window carrier is unavailable in this build");
}

#if AERO_PLATFORM_HAS_X11_WINDOW

std::uint32_t X11Modifiers(
    unsigned int state) noexcept {
    std::uint32_t result = WindowModifierNone;
    if ((state & ShiftMask) != 0U) {
        result |= WindowModifierShift;
    }
    if ((state & ControlMask) != 0U) {
        result |= WindowModifierControl;
    }
    if ((state & Mod1Mask) != 0U) {
        result |= WindowModifierAlt;
    }
    return result;
}

WindowPointerButton X11PointerButton(
    unsigned int button) noexcept {
    switch (button) {
    case Button1:
        return WindowPointerButton::Left;
    case Button2:
        return WindowPointerButton::Middle;
    case Button3:
        return WindowPointerButton::Right;
    case 8U:
        return WindowPointerButton::XButton1;
    case 9U:
        return WindowPointerButton::XButton2;
    default:
        return WindowPointerButton::Unknown;
    }
}

std::uint32_t X11TranslateKey(KeySym key) noexcept {
    switch (key) {
    case XK_BackSpace:
        return WindowKeyBackspace;
    case XK_Tab:
    case XK_ISO_Left_Tab:
        return WindowKeyTab;
    case XK_Return:
    case XK_KP_Enter:
        return WindowKeyEnter;
    case XK_Escape:
        return WindowKeyEscape;
    case XK_Page_Up:
    case XK_KP_Page_Up:
        return WindowKeyPageUp;
    case XK_Page_Down:
    case XK_KP_Page_Down:
        return WindowKeyPageDown;
    case XK_space:
        return WindowKeySpace;
    case XK_Home:
    case XK_KP_Home:
        return WindowKeyHome;
    case XK_End:
    case XK_KP_End:
        return WindowKeyEnd;
    case XK_Left:
    case XK_KP_Left:
        return WindowKeyLeft;
    case XK_Up:
    case XK_KP_Up:
        return WindowKeyUp;
    case XK_Right:
    case XK_KP_Right:
        return WindowKeyRight;
    case XK_Down:
    case XK_KP_Down:
        return WindowKeyDown;
    case XK_Delete:
    case XK_KP_Delete:
        return WindowKeyDelete;
    default:
        break;
    }
    if (key >= XK_a && key <= XK_z) {
        return static_cast<std::uint32_t>(
            static_cast<unsigned long>('A') +
            (key - XK_a));
    }
    if (key >= XK_A && key <= XK_Z) {
        return static_cast<std::uint32_t>(key);
    }
    if (key >= XK_0 && key <= XK_9) {
        return static_cast<std::uint32_t>(key);
    }
    return key <= static_cast<KeySym>(UINT32_MAX)
        ? static_cast<std::uint32_t>(key)
        : 0U;
}

#endif

} // namespace

X11Window::X11Window(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      events_(allocator_) {
}

X11Window::~X11Window() {
    Close();
}

#if AERO_PLATFORM_HAS_X11_WINDOW

void X11Window::Push(const WindowEvent& event) noexcept {
    events_.PushBack(event);
}

bool X11Window::Dequeue(WindowEvent& event) noexcept {
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

double X11Window::QueryDpiScale() const noexcept {
    if (display_ == nullptr || screen_ < 0) {
        return 1.0;
    }
    const int pixels = DisplayWidth(display_, screen_);
    const int millimeters = DisplayWidthMM(display_, screen_);
    if (pixels <= 0 || millimeters <= 0) {
        return 1.0;
    }
    const double dpi =
        static_cast<double>(pixels) * 25.4 /
        static_cast<double>(millimeters);
    return std::isfinite(dpi) && dpi > 0.0
        ? dpi / 96.0
        : 1.0;
}

Base::Result<void> X11Window::SetTitle(
    Base::StringView title) noexcept {
    const Base::StringView source = title.Empty()
        ? Base::StringView("AeroGUI")
        : title;
    Base::Vector<char> storage(allocator_);
    storage.Resize(
        source.SizeBytes() + 1U);
    for (std::uint32_t index = 0U;
         index < source.SizeBytes(); ++index) {
        storage[index] = source[index];
    }
    storage[source.SizeBytes()] = '\0';
    XStoreName(display_, window_, storage.Data());
    return {};
}

Base::Result<void> X11Window::Configure(
    Base::StringView title,
    std::uint32_t requestedWidth,
    std::uint32_t requestedHeight,
    bool visible) noexcept {
    events_.Clear();
    width_ = requestedWidth;
    height_ = requestedHeight;
    open_ = true;
    dpiScale_ = QueryDpiScale();

    const long eventMask =
        StructureNotifyMask |
        ExposureMask |
        PointerMotionMask |
        ButtonPressMask |
        ButtonReleaseMask |
        KeyPressMask |
        KeyReleaseMask |
        FocusChangeMask;
    XSelectInput(display_, window_, eventMask);
    closeAtom_ = XInternAtom(
        display_, "WM_DELETE_WINDOW", False);
    if (closeAtom_ != 0U) {
        Atom protocol = closeAtom_;
        static_cast<void>(XSetWMProtocols(
            display_, window_, &protocol, 1));
    }

    Base::Result<void> named = SetTitle(title);
    if (!named) {
        return named.GetStatus();
    }
    if (visible) {
        XMapWindow(display_, window_);
    }
    XFlush(display_);
    return {};
}

void X11Window::QueuePointer(
    WindowEventType type,
    double x,
    double y,
    unsigned int state,
    WindowPointerButton button) noexcept {
    WindowEvent event;
    event.type = type;
    event.x = x;
    event.y = y;
    event.modifiers = X11Modifiers(state);
    event.button = button;
    event.dpiScale = dpiScale_;
    Push(event);
}

void X11Window::QueueText(XKeyEvent& keyEvent) noexcept {
    WindowEvent input;
    char nativeText[sizeof(input.text)]{};
    KeySym ignored = NoSymbol;
    const int count = XLookupString(
        &keyEvent,
        nativeText,
        static_cast<int>(sizeof(nativeText)),
        &ignored,
        nullptr);
    if (count <= 0 ||
        count > static_cast<int>(sizeof(input.text))) {
        return;
    }
    const Base::StringView text(
        nativeText,
        static_cast<std::uint32_t>(count));
    if (!Base::ValidateUtf8(text).valid) {
        return;
    }
    input.type = WindowEventType::TextInput;
    input.textSize = static_cast<std::uint8_t>(count);
    for (std::uint8_t index = 0U;
         index < input.textSize; ++index) {
        input.text[index] = nativeText[index];
    }
    Push(input);
}

void X11Window::HandleEvent(XEvent& native) noexcept {
    switch (native.type) {
    case ClientMessage:
        if (closeAtom_ != 0U &&
            static_cast<Atom>(
                native.xclient.data.l[0]) == closeAtom_) {
            WindowEvent event;
            event.type = WindowEventType::CloseRequested;
            Push(event);
        }
        break;
    case DestroyNotify: {
        open_ = false;
        window_ = 0U;
        WindowEvent event;
        event.type = WindowEventType::Closed;
        Push(event);
        break;
    }
    case ConfigureNotify:
        if (native.xconfigure.width > 0 &&
            native.xconfigure.height > 0) {
            const std::uint32_t resizedWidth =
                static_cast<std::uint32_t>(
                    native.xconfigure.width);
            const std::uint32_t resizedHeight =
                static_cast<std::uint32_t>(
                    native.xconfigure.height);
            if (resizedWidth != width_ ||
                resizedHeight != height_) {
                width_ = resizedWidth;
                height_ = resizedHeight;
                WindowEvent event;
                event.type = WindowEventType::Resized;
                event.width = width_;
                event.height = height_;
                event.dpiScale = dpiScale_;
                Push(event);
            }
        }
        break;
    case Expose:
        if (native.xexpose.count == 0) {
            WindowEvent event;
            event.type = WindowEventType::Exposed;
            event.width = width_;
            event.height = height_;
            event.dpiScale = dpiScale_;
            Push(event);
        }
        break;
    case MotionNotify:
        QueuePointer(
            WindowEventType::PointerMove,
            static_cast<double>(native.xmotion.x),
            static_cast<double>(native.xmotion.y),
            native.xmotion.state);
        break;
    case ButtonPress:
        if (native.xbutton.button >= Button4 &&
            native.xbutton.button <= 7U) {
            WindowEvent event;
            event.type = WindowEventType::PointerWheel;
            event.x = static_cast<double>(native.xbutton.x);
            event.y = static_cast<double>(native.xbutton.y);
            event.modifiers = X11Modifiers(native.xbutton.state);
            event.dpiScale = dpiScale_;
            if (native.xbutton.button == Button4) {
                event.wheelDeltaY = 120.0;
            } else if (native.xbutton.button == Button5) {
                event.wheelDeltaY = -120.0;
            } else if (native.xbutton.button == 6U) {
                event.wheelDeltaX = -120.0;
            } else {
                event.wheelDeltaX = 120.0;
            }
            Push(event);
        } else {
            QueuePointer(
                WindowEventType::PointerDown,
                static_cast<double>(native.xbutton.x),
                static_cast<double>(native.xbutton.y),
                native.xbutton.state,
                X11PointerButton(native.xbutton.button));
        }
        break;
    case ButtonRelease:
        if (native.xbutton.button < Button4 ||
            native.xbutton.button > 7U) {
            QueuePointer(
                WindowEventType::PointerUp,
                static_cast<double>(native.xbutton.x),
                static_cast<double>(native.xbutton.y),
                native.xbutton.state,
                X11PointerButton(native.xbutton.button));
        }
        break;
    case KeyPress:
    case KeyRelease: {
        const KeySym key = XLookupKeysym(&native.xkey, 0);
        WindowEvent event;
        event.type = native.type == KeyPress
            ? WindowEventType::KeyDown
            : WindowEventType::KeyUp;
        event.key = X11TranslateKey(key);
        event.modifiers = X11Modifiers(native.xkey.state);
        event.repeat = false;
        Push(event);
        if (native.type == KeyPress) {
            QueueText(native.xkey);
        }
        break;
    }
    default:
        break;
    }
}

#endif // AERO_PLATFORM_HAS_X11_WINDOW

Base::Result<void> X11Window::Create(
    const WindowDescriptor& descriptor) noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (display_ != nullptr ||
        window_ != 0U ||
        open_) {
        return WindowFailure(
            Base::ErrorCode::AlreadyExists,
            "X11 window is already created");
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
            "X11 window dimensions are invalid");
    }

    const std::uint32_t width =
        platformDefaultSize ? 900U : descriptor.width;
    const std::uint32_t height =
        platformDefaultSize ? 640U : descriptor.height;

    display_ = XOpenDisplay(nullptr);
    if (display_ == nullptr) {
        return WindowFailure(
            Base::ErrorCode::Unsupported,
            "X11 display is unavailable");
    }
    ownsDisplay_ = true;
    screen_ = DefaultScreen(display_);
    window_ = XCreateSimpleWindow(
        display_,
        RootWindow(display_, screen_),
        0,
        0,
        width,
        height,
        0U,
        BlackPixel(display_, screen_),
        BlackPixel(display_, screen_));
    if (window_ == 0U) {
        Close();
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "X11 window creation failed");
    }
    ownsWindow_ = true;
    Base::Result<void> configured = Configure(
        descriptor.title,
        width,
        height,
        descriptor.visible);
    if (!configured) {
        Close();
        return configured.GetStatus();
    }
    return {};
#else
    static_cast<void>(descriptor);
    return UnsupportedX11Window();
#endif
}

Base::Result<void> X11Window::Attach(
    std::uintptr_t display,
    std::uintptr_t window,
    std::uint32_t width,
    std::uint32_t height,
    Base::StringView title,
    bool visible) noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (display_ != nullptr ||
        window_ != 0U ||
        open_) {
        return WindowFailure(
            Base::ErrorCode::AlreadyExists,
            "X11 window is already attached");
    }
    if (display == 0U || window == 0U ||
        width == 0U || height == 0U) {
        return WindowFailure(
            Base::ErrorCode::InvalidArgument,
            "X11 attachment requires display, window, and dimensions");
    }

    display_ = reinterpret_cast<Display*>(display);
    window_ = static_cast<::Window>(window);
    screen_ = DefaultScreen(display_);
    ownsDisplay_ = false;
    ownsWindow_ = false;
    Base::Result<void> configured = Configure(
        title, width, height, visible);
    if (!configured) {
        display_ = nullptr;
        window_ = 0U;
        return configured.GetStatus();
    }
    return {};
#else
    static_cast<void>(display);
    static_cast<void>(window);
    static_cast<void>(width);
    static_cast<void>(height);
    static_cast<void>(title);
    static_cast<void>(visible);
    return UnsupportedX11Window();
#endif
}

Base::Result<void> X11Window::Show() noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (display_ == nullptr ||
        window_ == 0U ||
        !open_) {
        return WindowFailure(
            Base::ErrorCode::NotInitialized,
            "X11 window is not created");
    }
    XMapWindow(display_, window_);
    XFlush(display_);
    return {};
#else
    return UnsupportedX11Window();
#endif
}

Base::Result<bool> X11Window::PollEvent(
    WindowEvent& event) noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (Dequeue(event)) {
        return true;
    }
    if (display_ == nullptr) {
        return false;
    }
    while (XPending(display_) > 0) {
        XEvent native{};
        XNextEvent(display_, &native);
        HandleEvent(native);
        if (Dequeue(event)) {
            return true;
        }
    }
    return false;
#else
    static_cast<void>(event);
    return UnsupportedX11Window();
#endif
}

Base::Result<bool> X11Window::WaitEvent(
    WindowEvent& event) noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (Dequeue(event)) {
        return true;
    }
    while (open_ && display_ != nullptr) {
        XEvent native{};
        XNextEvent(display_, &native);
        HandleEvent(native);
        if (Dequeue(event)) {
            return true;
        }
    }
    return Dequeue(event);
#else
    static_cast<void>(event);
    return UnsupportedX11Window();
#endif
}

void X11Window::Close() noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (display_ != nullptr &&
        ownsWindow_ &&
        window_ != 0U) {
        XDestroyWindow(display_, window_);
        XFlush(display_);
    }
    if (ownsDisplay_ && display_ != nullptr) {
        XCloseDisplay(display_);
    }
    display_ = nullptr;
    window_ = 0U;
    closeAtom_ = 0U;
    screen_ = 0;
    ownsDisplay_ = false;
    ownsWindow_ = false;
#endif
    open_ = false;
    width_ = 0U;
    height_ = 0U;
    dpiScale_ = 1.0;
}

bool X11Window::IsOpen() const noexcept {
    return open_;
}

std::uint32_t X11Window::ClientWidth() const noexcept {
    return width_;
}

std::uint32_t X11Window::ClientHeight() const noexcept {
    return height_;
}

double X11Window::DpiScale() const noexcept {
    return dpiScale_;
}

NativeWindowHandle X11Window::NativeHandle() const noexcept {
    NativeWindowHandle handle;
#if AERO_PLATFORM_HAS_X11_WINDOW
    handle.system = WindowSystem::X11;
    handle.display = reinterpret_cast<std::uintptr_t>(display_);
    handle.window = static_cast<std::uintptr_t>(window_);
#endif
    return handle;
}

} // namespace Aero::Platform
