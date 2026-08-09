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
#include <AeroApp/Window.hpp>

#include <cstdint>

namespace Aero {
class Application;
namespace App {
struct RunOptions;
class DesktopHost;
}

enum class ShutdownMode : std::uint8_t {
    OnLastWindowClose = 0U,
    OnMainWindowClose,
    OnExplicitShutdown
};

class AERO_APP_API WindowCollection {
public:
    std::uint32_t GetCount() const noexcept;
    Window* GetItem(std::uint32_t index) const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }

private:
    friend class Application;
    explicit WindowCollection(const Application& owner) noexcept : owner_(&owner) {}
    const Application* owner_ = nullptr;
};

class AERO_APP_API Application : public Base::Object {
    AERO_DECLARE_TYPE(Application, Base::Object)
public:
    Application() noexcept : Application(StaticTypeId()) {}
    ~Application() noexcept override;

    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    static Application* Current() noexcept;

    StringView GetStartupUri() const noexcept {
        return startupUri_.View();
    }
    void SetStartupUri(StringView value) noexcept {
        (void)startupUri_.Assign(value);
    }
    ResourceDictionary& GetResources() noexcept { return resources_; }
    const ResourceDictionary& GetResources() const noexcept {
        return resources_;
    }
    void SetResources(Ref<ResourceDictionary> value) noexcept;
    Window* GetMainWindow() const noexcept { return mainWindow_; }
    void SetMainWindow(Ref<Window> value) noexcept;
    WindowCollection GetWindows() const noexcept { return WindowCollection(*this); }
    ShutdownMode GetShutdownMode() const noexcept { return shutdownMode_; }
    void SetShutdownMode(ShutdownMode value) noexcept { shutdownMode_ = value; }

    // Runs this application through the optional default desktop host. Set an
    // explicit main Window with SetMainWindow(); otherwise StartupUri is used.
    Result<int> Run() noexcept;
    Result<int> Run(const App::RunOptions& options) noexcept;

    void Shutdown(int exitCode = 0) noexcept;

protected:
    explicit Application(Meta::TypeId runtimeType) noexcept;
    virtual void OnStartup(StartupEventArgs& args) noexcept;
    virtual void OnExit(ExitEventArgs& args) noexcept;
    virtual void OnActivated(EventArgs& args) noexcept;
    virtual void OnDeactivated(EventArgs& args) noexcept;

private:
    friend class App::DesktopHost;
    friend class Window;
    friend class WindowCollection;

    Result<void> Attach(
        void* hostState,
        Window* mainWindow) noexcept;
    void Detach() noexcept;
    void RaiseStartup() noexcept;
    void RaiseExit(int exitCode) noexcept;
    void RaiseActivated() noexcept;
    void RaiseDeactivated() noexcept;
    void AttachMainWindow(Window* value) noexcept;
    Ref<Window> MainWindowOwner() noexcept;
    void AdoptResources(ResourceDictionary&& resources) noexcept;

    Meta::TypeId runtimeType_ = StaticTypeId();
    String startupUri_;
    ResourceDictionary resources_;
    Ref<Object> mainWindowOwner_;
    Window* mainWindow_ = nullptr;
    ShutdownMode shutdownMode_ = ShutdownMode::OnLastWindowClose;
    void* hostState_ = nullptr;
};

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::ShutdownMode)
