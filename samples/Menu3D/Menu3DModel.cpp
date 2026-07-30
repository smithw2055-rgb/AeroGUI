#include "Menu3DModel.hpp"

#include <Aero/Audio/Audio.hpp>
#include <Aero/Controls/Controls.hpp>

namespace Aero::Samples::Menu3D {
namespace {

enum class Action : std::uint8_t {
    Start,
    Casual,
    Normal,
    Expert,
    Settings,
    Back,
    Exit,
};

class MenuCommand final : public Presentation::ICommand {
public:
    MenuCommand(Menu3DModel& owner, Action action) noexcept
        : owner_(&owner), action_(action) {}

    Core::TypeId RuntimeType() const noexcept override {
        return Presentation::ICommand::StaticTypeId();
    }

    Base::Result<bool> CanExecute(
        Presentation::CommandManager&,
        const Core::Value&,
        Presentation::UIElement&) noexcept override {
        return owner_ != nullptr;
    }

    Base::Result<void> Execute(
        Presentation::CommandManager&,
        const Core::Value&,
        Presentation::UIElement&) noexcept override {
        if (owner_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Menu3D command has no model");
        }
        switch (action_) {
        case Action::Start:
            owner_->PlaySmallEffect();
            return owner_->ActivateStart();
        case Action::Casual:
            owner_->PlaySmallEffect();
            return owner_->SelectDifficulty("CASUAL");
        case Action::Normal:
            owner_->PlaySmallEffect();
            return owner_->SelectDifficulty("NORMAL");
        case Action::Expert:
            owner_->PlaySmallEffect();
            return owner_->SelectDifficulty("EXPERT");
        case Action::Settings:
            owner_->PlaySmallEffect();
            return owner_->ActivateSettings();
        case Action::Back:
            owner_->PlayLargeEffect();
            return owner_->ActivateMain();
        case Action::Exit:
            owner_->PlayLargeEffect();
            owner_->ExitApplication();
            return {};
        }
        return {};
    }

private:
    Menu3DModel* owner_ = nullptr;
    Action action_ = Action::Start;
};

Base::Result<void> RegisterMetadata(
    Core::MetadataContext& context) noexcept {
    auto model = Describe<Menu3DModel>(context);
    model
        .Property("Start", &Menu3DModel::Start, &Menu3DModel::SetStart)
        .Property("StartCasual", &Menu3DModel::StartCasual, &Menu3DModel::SetStartCasual)
        .Property("StartNormal", &Menu3DModel::StartNormal, &Menu3DModel::SetStartNormal)
        .Property("StartExpert", &Menu3DModel::StartExpert, &Menu3DModel::SetStartExpert)
        .Property("Settings", &Menu3DModel::Settings, &Menu3DModel::SetSettings)
        .Property("Back", &Menu3DModel::Back, &Menu3DModel::SetBack)
        .Property("Exit", &Menu3DModel::Exit, &Menu3DModel::SetExit);
    return model.Result();
}

Base::Result<void> SetVisible(
    View& view,
    Base::StringView name,
    bool visible) noexcept {
    auto* element = view.FindNamed<Presentation::UIElement>(name);
    if (element == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Menu3D state panel was not created");
    }
    return element->SetVisibility(
        visible ? Presentation::Visibility::Visible
                : Presentation::Visibility::Collapsed);
}

Base::Result<Base::String> MakeSoundPath(
    Base::StringView assetRoot,
    Base::StringView fileName) noexcept {
    Base::String path;
    Base::Result<void> assigned = path.TryAssign(assetRoot);
    if (!assigned) return assigned.GetStatus();
    if (!assetRoot.Empty()) {
        const char last = assetRoot[assetRoot.SizeBytes() - 1U];
        if (last != '/' && last != '\\') {
            Base::Result<void> separator = path.TryAppend("/");
            if (!separator) return separator.GetStatus();
        }
    }
    Base::Result<void> sounds = path.TryAppend("Sounds/");
    if (!sounds) return sounds.GetStatus();
    Base::Result<void> name = path.TryAppend(fileName);
    if (!name) return name.GetStatus();
    return path;
}

} // namespace

Base::Ref<Presentation::ICommand> Menu3DModel::Start() const noexcept { return start_; }
Base::Ref<Presentation::ICommand> Menu3DModel::StartCasual() const noexcept { return startCasual_; }
Base::Ref<Presentation::ICommand> Menu3DModel::StartNormal() const noexcept { return startNormal_; }
Base::Ref<Presentation::ICommand> Menu3DModel::StartExpert() const noexcept { return startExpert_; }
Base::Ref<Presentation::ICommand> Menu3DModel::Settings() const noexcept { return settings_; }
Base::Ref<Presentation::ICommand> Menu3DModel::Back() const noexcept { return back_; }
Base::Ref<Presentation::ICommand> Menu3DModel::Exit() const noexcept { return exit_; }
void Menu3DModel::SetStart(Base::Ref<Presentation::ICommand> value) noexcept { start_ = std::move(value); }
void Menu3DModel::SetStartCasual(Base::Ref<Presentation::ICommand> value) noexcept { startCasual_ = std::move(value); }
void Menu3DModel::SetStartNormal(Base::Ref<Presentation::ICommand> value) noexcept { startNormal_ = std::move(value); }
void Menu3DModel::SetStartExpert(Base::Ref<Presentation::ICommand> value) noexcept { startExpert_ = std::move(value); }
void Menu3DModel::SetSettings(Base::Ref<Presentation::ICommand> value) noexcept { settings_ = std::move(value); }
void Menu3DModel::SetBack(Base::Ref<Presentation::ICommand> value) noexcept { back_ = std::move(value); }
void Menu3DModel::SetExit(Base::Ref<Presentation::ICommand> value) noexcept { exit_ = std::move(value); }

Base::Result<void> Menu3DModel::Initialize(
    App::Application& application,
    App::Window& window,
    Base::StringView assetRoot,
    bool enableAudio) noexcept {
    application_ = &application;
    window_ = &window;
    audioEnabled_ = enableAudio;
    Base::Result<void> rootAssigned = assetRoot_.TryAssign(assetRoot);
    if (!rootAssigned) return rootAssigned.GetStatus();
    const Action actions[] = {Action::Start, Action::Casual, Action::Normal,
        Action::Expert, Action::Settings, Action::Back, Action::Exit};
    Base::Ref<Presentation::ICommand>* commands[] = {&start_, &startCasual_,
        &startNormal_, &startExpert_, &settings_, &back_, &exit_};
    for (std::uint32_t i = 0; i < 7U; ++i) {
        Base::Result<Base::Ref<MenuCommand>> command =
            Base::MakeRef<MenuCommand>(*this, actions[i]);
        if (!command) return command.GetStatus();
        *commands[i] = Base::Ref<Presentation::ICommand>(
            std::move(command).Value());
    }
    Base::Result<void> activated = ActivateMain();
    if (activated) StartMusic();
    return activated;
}

Base::Result<void> Menu3DModel::ActivateMain() noexcept {
    if (window_ == nullptr || window_->HostedView() == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Menu3D window has no hosted view");
    }
    View& view = *window_->HostedView();
    Base::Result<void> result = SetVisible(view, "MainMenu", true);
    if (result) result = SetVisible(view, "StartMenu", false);
    if (result) result = SetVisible(view, "SettingsMenu", false);
    return result;
}

Base::Result<void> Menu3DModel::ActivateStart() noexcept {
    if (window_ == nullptr || window_->HostedView() == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "Menu3D window has no hosted view");
    View& view = *window_->HostedView();
    Base::Result<void> result = SetVisible(view, "MainMenu", false);
    if (result) result = SetVisible(view, "StartMenu", true);
    if (result) result = SetVisible(view, "SettingsMenu", false);
    return result;
}

Base::Result<void> Menu3DModel::ActivateSettings() noexcept {
    if (window_ == nullptr || window_->HostedView() == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "Menu3D window has no hosted view");
    View& view = *window_->HostedView();
    Base::Result<void> result = SetVisible(view, "MainMenu", false);
    if (result) result = SetVisible(view, "StartMenu", false);
    if (result) result = SetVisible(view, "SettingsMenu", true);
    return result;
}

Base::Result<void> Menu3DModel::SelectDifficulty(
    Base::StringView difficulty) noexcept {
    if (window_ == nullptr || window_->HostedView() == nullptr) return Base::Status::Failure(Base::ErrorCode::InvalidState, "Menu3D window has no hosted view");
    auto* status = window_->HostedView()->FindNamed<Controls::TextBlock>("StartStatus");
    if (status == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Menu3D difficulty status was not created");
    Base::String message;
    Base::Result<void> assigned = message.TryAssign("READY: ");
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> appended = message.TryAppend(difficulty);
    if (!appended) return appended.GetStatus();
    return status->SetText(message.View());
}

void Menu3DModel::PlaySmallEffect() noexcept {
    PlayEffect("WaterDropSmall.mp3");
}

void Menu3DModel::PlayLargeEffect() noexcept {
    PlayEffect("WaterDropBig.mp3");
}

void Menu3DModel::PlayEffect(Base::StringView name) noexcept {
    if (!audioEnabled_ || application_ == nullptr) return;
    Base::Result<Base::String> path = MakeSoundPath(assetRoot_.View(), name);
    if (!path) return;
    Base::Result<Audio::Engine*> audio = application_->GetAudioEngine();
    if (!audio) return;
    static_cast<void>(audio.Value()->PlayEffect(path.Value().View()));
}

void Menu3DModel::StartMusic() noexcept {
    if (!audioEnabled_ || application_ == nullptr) return;
    Base::Result<Base::String> path =
        MakeSoundPath(assetRoot_.View(), "SoundTrack.mp3");
    if (!path) return;
    Base::Result<Audio::Engine*> audio = application_->GetAudioEngine();
    if (!audio) return;
    static_cast<void>(audio.Value()->SetMusicVolume(0.65F));
    static_cast<void>(audio.Value()->SetEffectsVolume(0.85F));
    static_cast<void>(audio.Value()->PlayMusic(path.Value().View()));
}

void Menu3DModel::ExitApplication() noexcept {
    if (application_ != nullptr) application_->Shutdown(0);
}

Base::Result<Base::Ref<Menu3DModel>> CreateMenu3DModel(
    App::Application& application,
    App::Window& window,
    Base::StringView assetRoot,
    bool enableAudio) noexcept {
    Base::Result<Base::Ref<Menu3DModel>> made = Base::MakeRef<Menu3DModel>();
    if (!made) return made.GetStatus();
    Base::Ref<Menu3DModel> model = std::move(made).Value();
    Base::Result<void> initialized = model->Initialize(
        application, window, assetRoot, enableAudio);
    if (!initialized) return initialized.GetStatus();
    return model;
}

ModuleRegistration MakeMenu3DModuleManifest() noexcept {
    return DefineModule("Aero.Samples.Menu3D", &RegisterMetadata);
}

} // namespace Aero::Samples::Menu3D
