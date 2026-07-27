#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Module.hpp>
#include <Aero/UiDocument.hpp>
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
class ResourceDictionary;
class RoutedEventManager;
class StyleManager;
class TextInputManager;
}

namespace Aero::Markup {
class EmbeddedSourceProvider;
class DocumentCache;
class ISourceProvider;
class Schema;
class SourceProviderRegistry;
}

namespace Aero {

class SchemaBundle;

enum class BuiltInTheme : std::uint8_t {
    Light = 0U,
    Dark
};

enum class RuntimeResourceLayer : std::uint8_t {
    Application = 0U,
    Theme,
    System
};

enum class RuntimeResourceLoadMode : std::uint8_t {
    Replace = 0U,
    Merge
};

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
    RuntimeHost(
        SchemaBundle& schema,
        Base::IAllocator* allocator = nullptr) noexcept;
    RuntimeHost(
        SchemaBundle& schema,
        Markup::DocumentCache& documentCache,
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

    Base::Result<UiDocument> Load(
        Base::StringView uri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> Parse(
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept;
    Base::Result<void> RegisterSourceProvider(
        Markup::ISourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;
    Base::Result<void> LoadResources(
        RuntimeResourceLayer layer,
        Base::StringView uri,
        RuntimeResourceLoadMode mode =
            RuntimeResourceLoadMode::Replace,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> LoadCompiledResources(
        RuntimeResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        RuntimeResourceLoadMode mode =
            RuntimeResourceLoadMode::Replace) noexcept;
    Base::Result<void> LoadBuiltInTheme(
        BuiltInTheme theme) noexcept;
    Base::Result<void> Mount(
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Mount(
        Base::Ref<Base::Object> root,
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Mount(
        UiDocument&& document,
        Presentation::Size availableSize) noexcept;
    Base::Result<void> ReplaceMountedDocument(
        UiDocument&& document,
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
    Markup::Schema* Schema() noexcept;
    Markup::SourceProviderRegistry* Sources() noexcept;
    Markup::EmbeddedSourceProvider* EmbeddedSources() noexcept;
    Markup::DocumentCache* DocumentCache() noexcept;
    const Base::ResourceUri& CurrentDocumentUri() const noexcept;
    Base::Span<const Base::ResourceUri> CurrentDocumentDependencies() const noexcept;
    Presentation::ResourceDictionary* ApplicationResources() noexcept;
    Presentation::ResourceDictionary* ThemeResources() noexcept;
    Presentation::ResourceDictionary* SystemResources() noexcept;
    Presentation::StyleManager* Styles() noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
