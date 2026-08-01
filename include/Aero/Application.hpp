#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Meta/TypeRegistry.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Window.hpp>

#include <cstdint>
#include <utility>

namespace Aero {
class Application;
namespace App { struct RunOptions; namespace Detail { class ApplicationAccess; } }

enum class ShutdownMode : std::uint8_t {
    OnLastWindowClose = 0U,
    OnMainWindowClose,
    OnExplicitShutdown
};

struct StartupEventArgs final : EventArgs {
    AERO_DECLARE_TYPE(StartupEventArgs, EventArgs)
    StartupEventArgs() noexcept : EventArgs(StaticTypeId()) {}
    Base::StringView startupUri;
};

struct ExitEventArgs final : EventArgs {
    AERO_DECLARE_TYPE(ExitEventArgs, EventArgs)
    ExitEventArgs() noexcept : EventArgs(StaticTypeId()) {}
    int applicationExitCode = 0;
};

class AERO_API WindowCollection final {
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

    Core::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    static Application* Current() noexcept;

    Base::StringView GetStartupUri() const noexcept { return startupUri_.View(); }
    Base::Result<void> SetStartupUri(Base::StringView value) noexcept { return startupUri_.TryAssign(value); }
    Base::Ref<ResourceDictionary> GetResources() const noexcept { return resources_; }
    Base::Result<void> SetResources(Base::Ref<ResourceDictionary> value) noexcept { resources_ = std::move(value); return {}; }
    Window* GetMainWindow() const noexcept { return mainWindow_; }
    void SetMainWindow(Window* value) noexcept { mainWindow_ = value; }
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
    explicit Application(Core::TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    virtual void OnStartup(StartupEventArgs& args) noexcept;
    virtual void OnExit(ExitEventArgs& args) noexcept;
    virtual void OnActivated(EventArgs& args) noexcept;
    virtual void OnDeactivated(EventArgs& args) noexcept;

private:
    friend class App::Detail::ApplicationAccess;
    friend class Window;
    friend class WindowCollection;

    void Attach(void* runtimeState, Window* mainWindow) noexcept;
    void Detach() noexcept;
    void RaiseStartup() noexcept;
    void RaiseExit(int exitCode) noexcept;
    void RaiseActivated() noexcept;
    void RaiseDeactivated() noexcept;

    Core::TypeId runtimeType_ = StaticTypeId();
    Base::String startupUri_;
    Base::Ref<ResourceDictionary> resources_;
    void* runtimeState_ = nullptr;
    Window* mainWindow_ = nullptr;
    ShutdownMode shutdownMode_ = ShutdownMode::OnLastWindowClose;
};

} // namespace Aero

namespace Aero::Core {

template<>
struct MetaTypeTraits<Aero::ShutdownMode> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("ShutdownMode"); }
    static constexpr Base::StringView Namespace() noexcept { return AeroNamespaceUri(); }
    static constexpr Base::StringView Name() noexcept { return "ShutdownMode"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Core
