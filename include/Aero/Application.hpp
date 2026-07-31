#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Resources.hpp>

#include <cstdint>
#include <utility>

namespace Aero {
class Window;
namespace App { class Launcher; }

enum class ShutdownMode : std::uint8_t {
    OnLastWindowClose = 0U,
    OnMainWindowClose,
    OnExplicitShutdown
};

// Process-level WPF/XAML application object. It owns application resources and
// startup policy, but not native windows, graphics devices, audio or the event
// loop. Aero::App::Launcher is an optional default host.
class AERO_API Application : public Base::Object {
    AERO_DECLARE_TYPE(Application, Base::Object)
public:
    Application() noexcept
        : Application(StaticTypeId()) {}
    ~Application() noexcept override = default;

    Core::TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
    static Application* Current() noexcept;

    Base::StringView StartupUri() const noexcept {
        return startupUri_.View();
    }
    Base::Result<void> SetStartupUri(
        Base::StringView value) noexcept {
        return startupUri_.TryAssign(value);
    }

    Base::Ref<ResourceDictionary> Resources() const noexcept {
        return resources_;
    }
    Base::Result<void> SetResources(
        Base::Ref<ResourceDictionary> value) noexcept {
        resources_ = std::move(value);
        return {};
    }

    Window* MainWindow() const noexcept {
        return mainWindow_;
    }
    void SetMainWindow(Window* value) noexcept {
        mainWindow_ = value;
    }

    ShutdownMode GetShutdownMode() const noexcept {
        return shutdownMode_;
    }
    void SetShutdownMode(ShutdownMode value) noexcept {
        shutdownMode_ = value;
    }

    void Shutdown(int exitCode = 0) noexcept;

protected:
    explicit Application(Core::TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}

private:
    friend class App::Launcher;

    void Attach(void* runtimeState, Window* mainWindow) noexcept;
    void Detach() noexcept;

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
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ShutdownMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ShutdownMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
