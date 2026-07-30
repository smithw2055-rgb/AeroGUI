#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Resources.hpp>

#include <utility>

namespace Aero {
class Window;
}

namespace Aero::App {
class ApplicationHost;
namespace Detail {

class IApplicationPeer {
public:
    virtual ~IApplicationPeer() = default;
    virtual void RequestExit(int exitCode) noexcept = 0;
};

} // namespace Detail
} // namespace Aero::App

namespace Aero {

// Process-level WPF/XAML application object. It owns application resources and
// startup policy; Aero::App::Launcher supplies the default native event/render
// lifetime. Optional platform services are owned by Aero::App::Services rather
// than Application, preserving headless and embedded use.
class AERO_API Application final : public Base::Object {
    AERO_DECLARE_TYPE(Application, Base::Object)
public:
    Application() noexcept = default;
    ~Application() noexcept override = default;

    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
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

    void Shutdown(int exitCode = 0) noexcept;

private:
    friend class App::ApplicationHost;

    void Attach(
        App::Detail::IApplicationPeer* peer,
        Window* mainWindow) noexcept;
    void Detach() noexcept;

    Base::String startupUri_;
    Base::Ref<ResourceDictionary> resources_;
    App::Detail::IApplicationPeer* peer_ = nullptr;
    Window* mainWindow_ = nullptr;
};

} // namespace Aero
