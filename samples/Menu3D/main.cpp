#include "Menu3DModel.hpp"

#include <Aero/Core/Diagnostics.hpp>

#include <cstdio>
#include <cstring>

namespace {

struct StartupContext final {
    Aero::Base::StringView assetRoot;
    bool audioEnabled = false;
};

Aero::Base::Result<void> InitializeMenu3D(
    Aero::App::Application& application,
    Aero::App::Window& window,
    void* context) noexcept {
    auto* startup = static_cast<StartupContext*>(context);
    if (startup == nullptr) {
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::InvalidState,
            "Menu3D startup context is missing");
    }
    Aero::Base::Result<Aero::Base::Ref<Aero::Samples::Menu3D::Menu3DModel>>
        model = Aero::Samples::Menu3D::CreateMenu3DModel(
            application, window, startup->assetRoot, startup->audioEnabled);
    if (!model) return model.GetStatus();
    return window.SetDataContext(Aero::Base::Ref<Aero::Base::Object>(
        std::move(model).Value()));
}

struct Verification final { bool checked = false; };

Aero::Base::Result<void> VerifyFrame(
    Aero::App::Application& application,
    Aero::App::Window& window,
    std::uint64_t frame,
    void* context) noexcept {
    auto* verification = static_cast<Verification*>(context);
    Aero::View* view = window.HostedView();
    if (verification == nullptr || view == nullptr) return Aero::Base::Status::Failure(Aero::Base::ErrorCode::InvalidState, "Menu3D verification has no view");
    auto* main = view->FindNamed<Aero::Presentation::UIElement>("MainMenu");
    auto* start = view->FindNamed<Aero::Presentation::UIElement>("StartMenu");
    auto* settings = view->FindNamed<Aero::Presentation::UIElement>("SettingsMenu");
    auto* sky = view->FindNamed<Aero::Controls::Image>("SkyAndSun");
    if (main == nullptr || start == nullptr || settings == nullptr || sky == nullptr ||
        main->GetVisibility() != Aero::Presentation::Visibility::Visible ||
        start->GetVisibility() != Aero::Presentation::Visibility::Collapsed ||
        settings->GetVisibility() != Aero::Presentation::Visibility::Collapsed) {
        return Aero::Base::Status::Failure(Aero::Base::ErrorCode::ValidationFailed, "Menu3D initial state or parallax layers differ from the XAML");
    }
    if (!verification->checked) {
        std::printf("Menu3D verified frame=%llu title=%.*s\n", static_cast<unsigned long long>(frame), static_cast<int>(window.Title().SizeBytes()), window.Title().Data());
        verification->checked = true;
    }
    if (frame >= 3U) application.Shutdown(0);
    return {};
}

} // namespace

int main(int argc, char** argv) {
    const bool verify = argc == 2 && argv[1] != nullptr && std::strcmp(argv[1], "--verify") == 0;
    if (argc > 1 && !verify) {
        std::fputs("usage: AeroMenu3D [--verify]\n", stderr);
        return 2;
    }
    Aero::Core::DiagnosticBag diagnostics;
    Aero::App::ApplicationHostOptions options;
    options.applicationFile = AERO_MENU3D_APP_FILE;
    options.diagnostics = &diagnostics;
    options.startup = &InitializeMenu3D;
    StartupContext startup{AERO_MENU3D_ASSET_DIR, !verify};
    options.startupContext = &startup;
    Verification verification;
    if (verify) { options.visible = false; options.frame = &VerifyFrame; options.frameContext = &verification; }
    Aero::App::ApplicationHost host(options);
    Aero::Base::Result<void> module = host.AddModule(Aero::Samples::Menu3D::MakeMenu3DModuleManifest());
    if (!module) { std::fprintf(stderr, "Menu3D module registration failed: %s\n", module.GetStatus().message); return 1; }
    Aero::Base::Result<int> result = host.Run();
    if (result) return result.Value();
    std::fprintf(stderr, "Menu3D application failed: %s\n", result.GetStatus().message);
    for (const Aero::Core::Diagnostic& diagnostic : diagnostics.Items()) std::fprintf(stderr, "  %u:%u: %.*s\n", diagnostic.Source().begin.line, diagnostic.Source().begin.column, static_cast<int>(diagnostic.Message().SizeBytes()), diagnostic.Message().Data());
    return 1;
}
