#pragma once

#include <Aero/App.hpp>
#include <Aero/Metadata.hpp>
#include <Aero/Module.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/RuntimeEnvironment.hpp>

namespace Aero::Samples::Menu3D {

// The original sample is driven by this three-state menu model. Keeping it
// separate from the view means the XAML remains the description of the menu
// while commands own navigation and application lifetime.
class Menu3DModel final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        Menu3DModel,
        Base::Object,
        "clr-namespace:Menu3D",
        "Menu3DModel")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Ref<Presentation::ICommand> Start() const noexcept;
    Base::Ref<Presentation::ICommand> StartCasual() const noexcept;
    Base::Ref<Presentation::ICommand> StartNormal() const noexcept;
    Base::Ref<Presentation::ICommand> StartExpert() const noexcept;
    Base::Ref<Presentation::ICommand> Settings() const noexcept;
    Base::Ref<Presentation::ICommand> Back() const noexcept;
    Base::Ref<Presentation::ICommand> Exit() const noexcept;
    void SetStart(Base::Ref<Presentation::ICommand> value) noexcept;
    void SetStartCasual(Base::Ref<Presentation::ICommand> value) noexcept;
    void SetStartNormal(Base::Ref<Presentation::ICommand> value) noexcept;
    void SetStartExpert(Base::Ref<Presentation::ICommand> value) noexcept;
    void SetSettings(Base::Ref<Presentation::ICommand> value) noexcept;
    void SetBack(Base::Ref<Presentation::ICommand> value) noexcept;
    void SetExit(Base::Ref<Presentation::ICommand> value) noexcept;

    Base::Result<void> Initialize(
        App::Application& application,
        App::Window& window,
        Base::StringView assetRoot,
        bool enableAudio) noexcept;
    Base::Result<void> ActivateMain() noexcept;
    Base::Result<void> ActivateStart() noexcept;
    Base::Result<void> ActivateSettings() noexcept;
    Base::Result<void> SelectDifficulty(
        Base::StringView difficulty) noexcept;
    void PlaySmallEffect() noexcept;
    void PlayLargeEffect() noexcept;
    void ExitApplication() noexcept;

private:
    void PlayEffect(Base::StringView name) noexcept;
    void StartMusic() noexcept;

    App::Application* application_ = nullptr;
    App::Window* window_ = nullptr;
    Base::String assetRoot_;
    bool audioEnabled_ = false;
    Base::Ref<Presentation::ICommand> start_;
    Base::Ref<Presentation::ICommand> startCasual_;
    Base::Ref<Presentation::ICommand> startNormal_;
    Base::Ref<Presentation::ICommand> startExpert_;
    Base::Ref<Presentation::ICommand> settings_;
    Base::Ref<Presentation::ICommand> back_;
    Base::Ref<Presentation::ICommand> exit_;
};

Base::Result<Base::Ref<Menu3DModel>> CreateMenu3DModel(
    App::Application& application,
    App::Window& window,
    Base::StringView assetRoot,
    bool enableAudio) noexcept;
ModuleRegistration MakeMenu3DModuleManifest() noexcept;

} // namespace Aero::Samples::Menu3D
