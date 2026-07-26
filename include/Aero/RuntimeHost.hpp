#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Module.hpp>
#include <Aero/Presentation/Input.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <cstdint>

namespace Aero::Core {
class IDiagnosticSink;
class EffectiveValueEngine;
class MetadataDomain;
class MetadataRuntime;
}

namespace Aero::Controls {
class ControlInteractionManager;
class TemplateManager;
class TextBoxInteractionManager;
class VisualStateManager;
}

namespace Aero::Platform {
class IClipboard;
class ITextInputMethodHost;
}

namespace Aero::Presentation {
class BindingManager;
class CommandManager;
class FocusManager;
class KeyboardInputManager;
class ObjectTree;
class PointerInputManager;
class RenderManager;
class RoutedEventManager;
class TextInputManager;
}

namespace Aero::Markup {
class XamlActivationProviderRegistry;
class XamlCompiledDocument;
class XamlNodeReader;
class XamlSchemaContext;
}

namespace Aero {

struct RuntimeHostOptions final {
    Presentation::IRenderBackend* renderBackend = nullptr;
    Platform::IClipboard* clipboard = nullptr;
    Platform::ITextInputMethodHost* textInputMethodHost = nullptr;
    void* applicationServices = nullptr;
    void* hostContext = nullptr;
    bool attachControlInteractions = true;
    bool attachTextEditing = true;
};

struct RuntimeFrameResult final {
    std::uint64_t frameNumber = 0U;
    std::uint32_t callbackCount = 0U;
    Presentation::LayoutDiagnostics layout;
    Presentation::RenderDiagnostics render;
};

class AERO_API RuntimeHost final {
public:
    explicit RuntimeHost(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RuntimeHost() noexcept;

    RuntimeHost(const RuntimeHost&) = delete;
    RuntimeHost& operator=(const RuntimeHost&) = delete;

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Initialize(
        const RuntimeHostOptions& options) noexcept;
    void Shutdown() noexcept;

    bool IsInitialized() const noexcept;
    bool IsMounted() const noexcept;

    Base::Result<Base::Ref<Base::Object>> Load(
        Markup::XamlNodeReader& reader) noexcept;
    Base::Result<Base::Ref<Base::Object>> Load(
        const Markup::XamlCompiledDocument& document) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadXaml(
        Base::StringView source,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadCompiledXaml(
        Base::Span<const std::uint8_t> bytes) noexcept;
    Base::Result<void> Mount(
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Mount(
        Base::Ref<Base::Object> root,
        Presentation::Size availableSize) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadAndMount(
        Markup::XamlNodeReader& reader,
        Presentation::Size availableSize) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadAndMount(
        const Markup::XamlCompiledDocument& document,
        Presentation::Size availableSize) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadAndMountXaml(
        Base::StringView source,
        Presentation::Size availableSize,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<Base::Ref<Base::Object>> LoadAndMountCompiledXaml(
        Base::Span<const std::uint8_t> bytes,
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Resize(
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;

    Base::Result<RuntimeFrameResult> RunFrame() noexcept;
    Base::Result<Presentation::PointerDispatchResult> DispatchPointer(
        const Presentation::PointerInput& input) noexcept;
    Base::Result<Presentation::KeyboardDispatchResult> DispatchKeyboard(
        const Presentation::KeyboardInput& input) noexcept;
    Base::Result<Presentation::TextInputDispatchResult> DispatchText(
        const Presentation::TextInput& input) noexcept;
    Base::Result<std::uint32_t> AdvanceTime(
        std::uint32_t elapsedMilliseconds) noexcept;

    const Base::Ref<Base::Object>& Root() const noexcept;
    Base::Object* FindNamedObject(
        Base::StringView name,
        Core::TypeId expectedType = Core::InvalidTypeId) noexcept;
    std::uint32_t NamedObjectCount() const noexcept;

    template<class T>
    T* FindNamed(Base::StringView name) noexcept {
        return static_cast<T*>(FindNamedObject(name, Core::TypeOf<T>()));
    }

    Core::MetadataDomain* Metadata() noexcept;
    Core::MetadataRuntime* MetadataRuntime() noexcept;
    Core::EffectiveValueEngine* EffectiveValues() noexcept;
    Presentation::ObjectTree* Tree() noexcept;
    Presentation::LayoutManager* Layout() noexcept;
    Presentation::RenderManager* Renderer() noexcept;
    Presentation::BindingManager* Bindings() noexcept;
    Presentation::CommandManager* Commands() noexcept;
    Presentation::RoutedEventManager* RoutedEvents() noexcept;
    Presentation::FocusManager* Focus() noexcept;
    Controls::TemplateManager* Templates() noexcept;
    Controls::VisualStateManager* VisualStates() noexcept;
    Markup::XamlSchemaContext* Schema() noexcept;
    Markup::XamlActivationProviderRegistry* Activation() noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
