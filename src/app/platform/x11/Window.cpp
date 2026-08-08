#include "Window.hpp"

#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <new>

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

} // namespace

struct X11WindowState {
    explicit X11WindowState(Base::IAllocator& allocator) noexcept
        : events(&allocator) {}

    Base::Vector<WindowEvent> events;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    double dpiScale = 1.0;
    bool open = false;

#if AERO_PLATFORM_HAS_X11_WINDOW
    Display* display = nullptr;
    ::Window window = 0U;
    Atom closeAtom = 0U;
    int screen = 0;
    bool ownsDisplay = false;
    bool ownsWindow = false;

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

    double QueryDpiScale() const noexcept {
        if (display == nullptr || screen < 0) {
            return 1.0;
        }
        const int pixels = DisplayWidth(display, screen);
        const int millimeters = DisplayWidthMM(display, screen);
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

    Base::Result<void> SetTitle(
        Base::StringView title) noexcept {
        const Base::StringView source = title.Empty()
            ? Base::StringView("AeroGUI")
            : title;
        Base::Vector<char> storage(&events.Allocator());
        Base::Result<void> resized = storage.Resize(
            source.SizeBytes() + 1U);
        if (!resized) {
            return resized.GetStatus();
        }
        for (std::uint32_t index = 0U;
             index < source.SizeBytes(); ++index) {
            storage[index] = source[index];
        }
        storage[source.SizeBytes()] = '\0';
        XStoreName(display, window, storage.Data());
        return {};
    }

    Base::Result<void> Configure(
        Base::StringView title,
        std::uint32_t requestedWidth,
        std::uint32_t requestedHeight,
        bool visible) noexcept {
        events.Clear();
        width = requestedWidth;
        height = requestedHeight;
        open = true;
        dpiScale = QueryDpiScale();

        const long eventMask =
            StructureNotifyMask |
            ExposureMask |
            PointerMotionMask |
            ButtonPressMask |
            ButtonReleaseMask |
            KeyPressMask |
            KeyReleaseMask |
            FocusChangeMask;
        XSelectInput(display, window, eventMask);
        closeAtom = XInternAtom(
            display, "WM_DELETE_WINDOW", False);
        if (closeAtom != 0U) {
            Atom protocol = closeAtom;
            static_cast<void>(XSetWMProtocols(
                display, window, &protocol, 1));
        }

        Base::Result<void> named = SetTitle(title);
        if (!named) {
            return named.GetStatus();
        }
        if (visible) {
            XMapWindow(display, window);
        }
        XFlush(display);
        return {};
    }

    static std::uint32_t Modifiers(
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

    static WindowPointerButton PointerButton(
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

    static std::uint32_t TranslateKey(KeySym key) noexcept {
        switch (key) {
        case XK_BackSpace:
            return WindowKeyBackspace;
        case XK_Tab:
        case XK_ISO_Left_Tab:
            return WindowKeyTab;
        case XK_Return:
        case XK_KP_Enter:
            return WindowKeyEnter;
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

    void QueuePointer(
        WindowEventType type,
        double x,
        double y,
        unsigned int state,
        WindowPointerButton button =
            WindowPointerButton::Unknown) noexcept {
        WindowEvent event;
        event.type = type;
        event.x = x;
        event.y = y;
        event.modifiers = Modifiers(state);
        event.button = button;
        event.dpiScale = dpiScale;
        Push(event);
    }

    void QueueText(XKeyEvent& keyEvent) noexcept {
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

    void HandleEvent(XEvent& native) noexcept {
        switch (native.type) {
        case ClientMessage:
            if (closeAtom != 0U &&
                static_cast<Atom>(
                    native.xclient.data.l[0]) == closeAtom) {
                open = false;
                WindowEvent event;
                event.type = WindowEventType::CloseRequested;
                Push(event);
            }
            break;
        case DestroyNotify: {
            open = false;
            window = 0U;
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
                if (resizedWidth != width ||
                    resizedHeight != height) {
                    width = resizedWidth;
                    height = resizedHeight;
                    WindowEvent event;
                    event.type = WindowEventType::Resized;
                    event.width = width;
                    event.height = height;
                    event.dpiScale = dpiScale;
                    Push(event);
                }
            }
            break;
        case Expose:
            if (native.xexpose.count == 0) {
                WindowEvent event;
                event.type = WindowEventType::Exposed;
                event.width = width;
                event.height = height;
                event.dpiScale = dpiScale;
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
                event.modifiers = Modifiers(native.xbutton.state);
                event.dpiScale = dpiScale;
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
                    PointerButton(native.xbutton.button));
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
                    PointerButton(native.xbutton.button));
            }
            break;
        case KeyPress:
        case KeyRelease: {
            const KeySym key = XLookupKeysym(&native.xkey, 0);
            WindowEvent event;
            event.type = native.type == KeyPress
                ? WindowEventType::KeyDown
                : WindowEventType::KeyUp;
            event.key = TranslateKey(key);
            event.modifiers = Modifiers(native.xkey.state);
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
#endif
};

static_assert(sizeof(X11WindowState) <= 8192U,
    "X11Window inline state storage is too small");
static_assert(alignof(X11WindowState) <= alignof(std::max_align_t),
    "X11Window inline state alignment is insufficient");

X11Window::X11Window(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) X11WindowState(*allocator_);
}

X11Window::~X11Window() {
    Close();
    if (state_ != nullptr) {
        state_->~X11WindowState();
        state_ = nullptr;
    }
}

Base::Result<void> X11Window::Create(
    const WindowDescriptor& descriptor) noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (state_->display != nullptr ||
        state_->window != 0U ||
        state_->open) {
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

    state_->display = XOpenDisplay(nullptr);
    if (state_->display == nullptr) {
        return WindowFailure(
            Base::ErrorCode::Unsupported,
            "X11 display is unavailable");
    }
    state_->ownsDisplay = true;
    state_->screen = DefaultScreen(state_->display);
    state_->window = XCreateSimpleWindow(
        state_->display,
        RootWindow(state_->display, state_->screen),
        0,
        0,
        width,
        height,
        0U,
        BlackPixel(state_->display, state_->screen),
        BlackPixel(state_->display, state_->screen));
    if (state_->window == 0U) {
        Close();
        return WindowFailure(
            Base::ErrorCode::InternalError,
            "X11 window creation failed");
    }
    state_->ownsWindow = true;
    Base::Result<void> configured = state_->Configure(
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
    if (state_->display != nullptr ||
        state_->window != 0U ||
        state_->open) {
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

    state_->display = reinterpret_cast<Display*>(display);
    state_->window = static_cast<::Window>(window);
    state_->screen = DefaultScreen(state_->display);
    state_->ownsDisplay = false;
    state_->ownsWindow = false;
    Base::Result<void> configured = state_->Configure(
        title, width, height, visible);
    if (!configured) {
        state_->display = nullptr;
        state_->window = 0U;
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
    if (state_->display == nullptr ||
        state_->window == 0U ||
        !state_->open) {
        return WindowFailure(
            Base::ErrorCode::NotInitialized,
            "X11 window is not created");
    }
    XMapWindow(state_->display, state_->window);
    XFlush(state_->display);
    return {};
#else
    return UnsupportedX11Window();
#endif
}

Base::Result<bool> X11Window::PollEvent(
    WindowEvent& event) noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (state_->Dequeue(event)) {
        return true;
    }
    if (state_->display == nullptr) {
        return false;
    }
    while (XPending(state_->display) > 0) {
        XEvent native{};
        XNextEvent(state_->display, &native);
        state_->HandleEvent(native);
        if (state_->Dequeue(event)) {
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
    if (state_->Dequeue(event)) {
        return true;
    }
    while (state_->open && state_->display != nullptr) {
        XEvent native{};
        XNextEvent(state_->display, &native);
        state_->HandleEvent(native);
        if (state_->Dequeue(event)) {
            return true;
        }
    }
    return state_->Dequeue(event);
#else
    static_cast<void>(event);
    return UnsupportedX11Window();
#endif
}

void X11Window::Close() noexcept {
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (state_ == nullptr) {
        return;
    }
    if (state_->display != nullptr &&
        state_->ownsWindow &&
        state_->window != 0U) {
        XDestroyWindow(state_->display, state_->window);
        XFlush(state_->display);
    }
    if (state_->ownsDisplay && state_->display != nullptr) {
        XCloseDisplay(state_->display);
    }
    state_->display = nullptr;
    state_->window = 0U;
    state_->closeAtom = 0U;
    state_->screen = 0;
    state_->ownsDisplay = false;
    state_->ownsWindow = false;
#endif
    if (state_ != nullptr) {
        state_->open = false;
        state_->width = 0U;
        state_->height = 0U;
        state_->dpiScale = 1.0;
    }
}

bool X11Window::IsOpen() const noexcept {
    return state_ != nullptr && state_->open;
}

std::uint32_t X11Window::ClientWidth() const noexcept {
    return state_ != nullptr ? state_->width : 0U;
}

std::uint32_t X11Window::ClientHeight() const noexcept {
    return state_ != nullptr ? state_->height : 0U;
}

double X11Window::DpiScale() const noexcept {
    return state_ != nullptr ? state_->dpiScale : 1.0;
}

NativeWindowHandle X11Window::NativeHandle() const noexcept {
    NativeWindowHandle handle;
#if AERO_PLATFORM_HAS_X11_WINDOW
    if (state_ != nullptr) {
        handle.system = WindowSystem::X11;
        handle.display = reinterpret_cast<std::uintptr_t>(state_->display);
        handle.window = static_cast<std::uintptr_t>(state_->window);
    }
#endif
    return handle;
}

} // namespace Aero::Platform
