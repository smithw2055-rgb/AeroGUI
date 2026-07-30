#include <Aero/App.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Presentation/Transforms.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#include <cstdio>
#include <cmath>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

struct Verification final {
    bool checked = false;
};

struct TraceLog final {
    std::FILE* output = nullptr;
};

struct ResizeVerification final {
    Aero::Presentation::Size initialClientSize;
    bool resizeRequested = false;
};

struct PreviewWindow final {
    bool resizeRequested = false;
};

void LogColor(
    std::FILE* output,
    const Aero::Presentation::Color& color) noexcept {
    std::fprintf(
        output, "%.3f,%.3f,%.3f,%.3f",
        color.red, color.green, color.blue, color.alpha);
}

Aero::Base::Result<void> TraceFrame(
    Aero::App::Application&,
    Aero::App::Window& window,
    std::uint64_t frameIndex,
    void* context) noexcept {
    auto* trace = static_cast<TraceLog*>(context);
    Aero::View* view = window.HostedView();
    if (trace == nullptr || trace->output == nullptr || view == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidState,
            "HelloWorld trace requires a log and hosted View");
    }
    if (frameIndex != 0U && frameIndex != 15U &&
        frameIndex != 30U && frameIndex != 60U &&
        frameIndex != 90U && frameIndex != 120U) {
        return {};
    }
    const auto* logo = view->FindNamed<Aero::Controls::Grid>("Logo");
    const auto* word = view->FindNamed<Aero::Controls::Grid>("Word");
    const auto* gui = view->FindNamed<Aero::Controls::Path>("GUI");
    const auto* hello = view->FindNamed<Aero::Controls::TextBlock>("Hello");
    const auto* world = view->FindNamed<Aero::Controls::TextBlock>("World");
    if (logo == nullptr || word == nullptr || gui == nullptr ||
        hello == nullptr || world == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::ValidationFailed,
            "HelloWorld trace targets were not created");
    }
    std::fprintf(trace->output, "frame=%llu background=",
        static_cast<unsigned long long>(frameIndex));
    LogColor(trace->output, window.Background());
    std::fprintf(trace->output, " word-width=%.2f gui-fill=", word->Width());
    LogColor(trace->output, gui->Fill());
    std::fputs(" hello-foreground=", trace->output);
    LogColor(trace->output, hello->Foreground());
    std::fputs(" world-foreground=", trace->output);
    LogColor(trace->output, world->Foreground());
    const Aero::Presentation::Size logoSize = logo->RenderSize();
    const Aero::Presentation::Size wordSize = word->RenderSize();
    std::fprintf(trace->output,
        " logo=%.1fx%.1f word=%.1fx%.1f\n",
        logoSize.width, logoSize.height,
        wordSize.width, wordSize.height);
    std::fflush(trace->output);
    return {};
}

Aero::Base::Result<void> VerifyFrame(
    Aero::App::Application& application,
    Aero::App::Window& window,
    std::uint64_t frameIndex,
    void* context) noexcept {
    auto* verification = static_cast<Verification*>(context);
    Aero::View* view = window.HostedView();
    if (verification == nullptr || view == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidState,
            "HelloWorld verification requires a hosted View");
    }
    if (!verification->checked) {
        const auto* logo = view->FindNamed<Aero::Controls::Grid>("Logo");
        const auto* word = view->FindNamed<Aero::Controls::Grid>("Word");
        const auto* gui = view->FindNamed<Aero::Controls::Path>("GUI");
        const auto* hello = view->FindNamed<Aero::Controls::TextBlock>("Hello");
        const auto* world = view->FindNamed<Aero::Controls::TextBlock>("World");
        if (logo == nullptr || word == nullptr || gui == nullptr ||
            hello == nullptr || world == nullptr) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed,
                "HelloWorld named animation targets were not created");
        }
        std::printf(
            "HelloWorld verified frame=%llu word-width=%.1f title=%.*s\n",
            static_cast<unsigned long long>(frameIndex),
            word->Width(),
            static_cast<int>(window.Title().SizeBytes()),
            window.Title().Data());
        verification->checked = true;
    }
    if (frameIndex >= 3U) application.Shutdown(0);
    return {};
}

Aero::Base::Result<void> VerifyResizeFrame(
    Aero::App::Application& application,
    Aero::App::Window& window,
    std::uint64_t frameIndex,
    void* context) noexcept {
    auto* verification =
        static_cast<ResizeVerification*>(context);
    Aero::Platform::IWindow* native = window.NativeWindow();
    if (verification == nullptr || native == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidState,
            "HelloWorld resize verification requires a native Window");
    }
    if (frameIndex == 0U) {
        verification->initialClientSize = {
            static_cast<double>(native->ClientWidth()),
            static_cast<double>(native->ClientHeight())};
        return {};
    }
#if defined(_WIN32)
    if (!verification->resizeRequested) {
        const Aero::Platform::NativeWindowHandle handle =
            native->NativeHandle();
        if (handle.system != Aero::Platform::WindowSystem::Win32 ||
            handle.window == 0U || SetWindowPos(
                reinterpret_cast<HWND>(handle.window),
                nullptr, 0, 0, 1280, 900,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) == FALSE) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InvalidState,
                "HelloWorld could not request its native resize");
        }
        verification->resizeRequested = true;
        return {};
    }
#else
    return Aero::Base::Status::Failure(
        Aero::Base::ErrorCode::Unsupported,
        "HelloWorld resize verification requires Win32");
#endif
    const Aero::Presentation::Size clientSize{
        static_cast<double>(native->ClientWidth()),
        static_cast<double>(native->ClientHeight())};
    const Aero::Presentation::Size layoutSize = window.RenderSize();
    if (clientSize.width <= verification->initialClientSize.width ||
        clientSize.height <= verification->initialClientSize.height ||
        std::fabs(layoutSize.width - clientSize.width) > 0.01 ||
        std::fabs(layoutSize.height - clientSize.height) > 0.01) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::ValidationFailed,
            "HelloWorld resize did not synchronize native and layout sizes");
    }
    std::printf(
        "HelloWorld resized client=%.0fx%.0f layout=%.0fx%.0f\n",
        clientSize.width, clientSize.height,
        layoutSize.width, layoutSize.height);
    application.Shutdown(0);
    return {};
}

Aero::Base::Result<void> PreviewFrame(
    Aero::App::Application&,
    Aero::App::Window& window,
    std::uint64_t,
    void* context) noexcept {
    auto* preview = static_cast<PreviewWindow*>(context);
    Aero::Platform::IWindow* native = window.NativeWindow();
    if (preview == nullptr || native == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidState,
            "HelloWorld preview requires a native Window");
    }
#if defined(_WIN32)
    if (!preview->resizeRequested) {
        const Aero::Platform::NativeWindowHandle handle =
            native->NativeHandle();
        if (handle.system != Aero::Platform::WindowSystem::Win32 ||
            handle.window == 0U || SetWindowPos(
                reinterpret_cast<HWND>(handle.window), nullptr,
                100, 100, 1200, 740,
                SWP_NOZORDER | SWP_NOACTIVATE) == FALSE) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InvalidState,
                "HelloWorld could not size its preview window");
        }
        preview->resizeRequested = true;
    }
    return {};
#else
    return Aero::Base::Status::Failure(
        Aero::Base::ErrorCode::Unsupported,
        "HelloWorld preview requires Win32");
#endif
}

} // namespace

int main(int argc, char** argv) {
    const bool verify = argc == 2 && argv[1] != nullptr &&
        std::strcmp(argv[1], "--verify") == 0;
    const bool trace = argc == 2 && argv[1] != nullptr &&
        std::strcmp(argv[1], "--trace") == 0;
    const bool verifyResize = argc == 2 && argv[1] != nullptr &&
        std::strcmp(argv[1], "--resize-verify") == 0;
    const bool preview = argc == 2 && argv[1] != nullptr &&
        std::strcmp(argv[1], "--preview") == 0;
    if (argc > 1 && !verify && !trace && !verifyResize && !preview) {
        std::fprintf(stderr,
            "usage: AeroHelloWorld [--verify|--trace|--resize-verify|--preview]\n");
        return 2;
    }

    Aero::Core::DiagnosticBag diagnostics;
    Aero::App::ApplicationHostOptions options;
    options.applicationFile = AERO_HELLO_WORLD_APP_FILE;
    options.loadBuiltInTheme = false;
    options.diagnostics = &diagnostics;
#if defined(_WIN32)
    options.graphicsBackend = Aero::App::GraphicsBackend::D3D11;
#endif
    Verification verification;
    if (verify) {
        options.visible = false;
        options.frame = &VerifyFrame;
        options.frameContext = &verification;
    }
    ResizeVerification resizeVerification;
    if (verifyResize) {
        options.frame = &VerifyResizeFrame;
        options.frameContext = &resizeVerification;
    }
    PreviewWindow previewWindow;
    if (preview) {
        options.frame = &PreviewFrame;
        options.frameContext = &previewWindow;
    }
    TraceLog traceLog;
    if (trace) {
        traceLog.output = std::fopen(
            "hello-world-d3d11.log", "wb");
        if (traceLog.output == nullptr) {
            std::fputs("HelloWorld could not open hello-world-d3d11.log\n", stderr);
            return 1;
        }
        std::fputs("backend=d3d11\n", traceLog.output);
        options.frame = &TraceFrame;
        options.frameContext = &traceLog;
    }
    Aero::App::ApplicationHost host(options);
    Aero::Base::Result<int> result = host.Run();
    if (traceLog.output != nullptr) std::fclose(traceLog.output);
    if (result) return result.Value();

    std::fprintf(stderr, "HelloWorld application failed: %s\n",
        result.GetStatus().message);
    for (const Aero::Core::Diagnostic& diagnostic : diagnostics.Items()) {
        std::fprintf(stderr, "  %u:%u: %.*s\n",
            diagnostic.Source().begin.line,
            diagnostic.Source().begin.column,
            static_cast<int>(diagnostic.Message().SizeBytes()),
            diagnostic.Message().Data());
    }
    return 1;
}
