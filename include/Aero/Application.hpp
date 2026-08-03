#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Events/ApplicationEventArgs.hpp>
#include <Aero/Value.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Window.hpp>

#include <cstdint>
#include <utility>

namespace Aero {
class Application;
namespace App { struct RunOptions; }
namespace Internal { class DesktopPrivate; }

enum class ShutdownMode : std::uint8_t {
    OnLastWindowClose = 0U,
    OnMainWindowClose,
    OnExplicitShutdown
};

class AERO_API WindowCollection {
public:
    std::uint32_t GetCount() const noexcept;
    Window* GetItem(std::uint32_t index) const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }

private:
    friend class Application;
    explicit WindowCollection(const Application& owner) noexcept : owner_(&owner) {}
    const Application* owner_ = nullptr;
};

class AERO_API Application : public Base::Object {
    AERO_DECLARE_TYPE(Application, Base::Object)
public:
    Application() noexcept : Application(StaticTypeId()) {}
    ~Application() noexcept override = default;

    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    static Application* Current() noexcept;

    Base::StringView GetStartupUri() const noexcept { return startupUri_.View(); }
    void SetStartupUri(Base::StringView value) noexcept {
        (void)startupUri_.Assign(value);
    }
    Base::Ref<ResourceDictionary> GetResources() const noexcept { return resources_; }
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept { resources_ = std::move(value); return; }
    Window* GetMainWindow() const noexcept { return mainWindow_; }
    void SetMainWindow(Window* value) noexcept;
    WindowCollection GetWindows() const noexcept { return WindowCollection(*this); }
    ShutdownMode GetShutdownMode() const noexcept { return shutdownMode_; }
    void SetShutdownMode(ShutdownMode value) noexcept { shutdownMode_ = value; }

    // Runs this application through the optional default desktop host.
    // A StartupUri is required unless an explicit Window is supplied.
    int Run() noexcept;
    int Run(const App::RunOptions& options) noexcept;
    int Run(Base::Ref<Window> window) noexcept;
    int Run(
        Base::Ref<Window> window,
        const App::RunOptions& options) noexcept;

    void Shutdown(int exitCode = 0) noexcept;

protected:
    explicit Application(Meta::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    virtual void OnStartup(StartupEventArgs& args) noexcept;
    virtual void OnExit(ExitEventArgs& args) noexcept;
    virtual void OnActivated(EventArgs& args) noexcept;
    virtual void OnDeactivated(EventArgs& args) noexcept;

private:
    friend class ::Aero::Internal::DesktopPrivate;
    friend class Window;
    friend class WindowCollection;

    void Attach(void* hostState, Window* mainWindow) noexcept;
    void Detach() noexcept;
    void RaiseStartup() noexcept;
    void RaiseExit(int exitCode) noexcept;
    void RaiseActivated() noexcept;
    void RaiseDeactivated() noexcept;

    Meta::TypeId runtimeType_ = StaticTypeId();
    Base::String startupUri_;
    Base::Ref<ResourceDictionary> resources_;
    void* hostState_ = nullptr;
    Base::Ref<Base::Object> mainWindowOwner_;
    Window* mainWindow_ = nullptr;
    ShutdownMode shutdownMode_ = ShutdownMode::OnLastWindowClose;
};

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::ShutdownMode)
