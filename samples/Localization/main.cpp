#include "LocalizationModel.hpp"

#include <Aero/App.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#include <cstdio>
#include <cstring>

namespace {

struct Runtime final {
    Aero::Base::Ref<Aero::Samples::Localization::ViewModel> model;
    std::uint32_t initialWaitFrames = 0U;
    bool initialStateVerified = false;
    bool selectionRequested = false;
    bool verify = false;
};

Aero::Base::Result<void> SynchronizeSelection(
    Runtime& runtime,
    Aero::Controls::ComboBox& selector) noexcept {
    const Aero::Base::Ref<Aero::Base::Object> selected =
        selector.SelectedItem();
    if (!selected || selected->RuntimeType() !=
            Aero::Samples::Localization::Language::StaticTypeId()) {
        return {};
    }
    runtime.model->SetSelectedLanguage(
        Aero::Base::Ref<Aero::Samples::Localization::Language>::
            FromBorrowed(*static_cast<
                Aero::Samples::Localization::Language*>(
                selected.Get())));
    return {};
}

Aero::Base::Result<void> Initialize(
    Aero::App::Application&,
    Aero::App::Window& window,
    void* context) noexcept {
    auto* runtime = static_cast<Runtime*>(context);
    if (runtime == nullptr) return Aero::Base::Status::Failure(
        Aero::Base::ErrorCode::InvalidState, "Localization startup context is unavailable");
    auto made = Aero::Samples::Localization::CreateLocalizationViewModel();
    if (!made) return made.GetStatus();
    runtime->model = std::move(made).Value();
    return window.SetDataContext(Aero::Base::Ref<Aero::Base::Object>(runtime->model));
}

Aero::Base::Result<void> VerifyFrame(
    Aero::App::Application& application,
    Aero::App::Window& window,
    std::uint64_t frame,
    void* context) noexcept {
    auto* runtime = static_cast<Runtime*>(context);
    Aero::View* view = window.HostedView();
    if (runtime == nullptr || view == nullptr || !runtime->model) return Aero::Base::Status::Failure(
        Aero::Base::ErrorCode::InvalidState, "Localization verification requires a ViewModel and hosted View");
    auto* selector = view->FindNamed<Aero::Controls::ComboBox>("LanguageSelector");
    auto* title = view->FindNamed<Aero::Controls::TextBlock>("LanguageTitle");
    auto* sound = view->FindNamed<Aero::Controls::Slider>("SoundSlider");
    auto* music = view->FindNamed<Aero::Controls::Slider>("MusicSlider");
    if (selector == nullptr || title == nullptr || sound == nullptr || music == nullptr) return Aero::Base::Status::Failure(
        Aero::Base::ErrorCode::ValidationFailed, "Localization named controls were not created");
    auto synchronized = SynchronizeSelection(*runtime, *selector);
    if (!synchronized) return synchronized.GetStatus();
    if (!runtime->verify) return {};
    if (!runtime->initialStateVerified) {
        if (selector->ItemCount() == 0U && runtime->initialWaitFrames < 8U) {
            ++runtime->initialWaitFrames;
            return {};
        }
        if (selector->ItemCount() != 4U || !runtime->model->SelectedLanguage() ||
            runtime->model->SelectedLanguage()->Name() != "English" ||
            sound->Value() != 100.0 || music->Value() != 70.0) return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::ValidationFailed, "Localization initial ViewModel state differs from the reference");
        runtime->initialStateVerified = true;
        return {};
    }
    if (!runtime->selectionRequested) {
        auto selected = selector->SetSelectedIndex(3U);
        if (!selected) return selected.GetStatus();
        runtime->selectionRequested = true;
        return {};
    }
    if (frame < 3U) return {};
    const auto language = runtime->model->SelectedLanguage();
    const auto selected = selector->SelectedItem();
    if (language && selected.Get() == language.Get() &&
        selector->SelectedIndex() == 3U &&
        title->Text() == language->TitleLabel()) {
        std::printf("Localization verified languages=%u selected=%.*s sound=%.0f music=%.0f\n",
            selector->ItemCount(), static_cast<int>(language->Name().SizeBytes()),
            language->Name().Data(), sound->Value(), music->Value());
        application.Shutdown(0);
        return {};
    }
    return Aero::Base::Status::Failure(
        Aero::Base::ErrorCode::ValidationFailed,
        "Localization selection did not refresh bound content");
}

} // namespace

int main(int argc, char** argv) {
    const bool verify = argc == 2 && argv[1] != nullptr && std::strcmp(argv[1], "--verify") == 0;
    if (argc > 1 && !verify) { std::fputs("usage: AeroLocalization [--verify]\n", stderr); return 2; }
    Aero::Core::DiagnosticBag diagnostics;
    Runtime runtime;
    runtime.verify = verify;
    Aero::App::ApplicationHostOptions options;
    options.applicationFile = AERO_LOCALIZATION_APP_FILE;
    options.diagnostics = &diagnostics;
    options.startup = &Initialize;
    options.startupContext = &runtime;
    options.frame = &VerifyFrame;
    options.frameContext = &runtime;
    if (verify) options.visible = false;
    Aero::App::ApplicationHost host(options);
    auto module = host.AddModule(Aero::Samples::Localization::MakeLocalizationModuleManifest());
    if (!module) { std::fprintf(stderr, "Localization module registration failed: %s\n", module.GetStatus().message); return 1; }
    auto result = host.Run();
    if (result) return result.Value();
    std::fprintf(stderr, "Localization application failed: %s\n", result.GetStatus().message);
    for (const Aero::Core::Diagnostic& diagnostic : diagnostics.Items()) std::fprintf(stderr, "  %u:%u: %.*s\n", diagnostic.Source().begin.line, diagnostic.Source().begin.column, static_cast<int>(diagnostic.Message().SizeBytes()), diagnostic.Message().Data());
    return 1;
}
