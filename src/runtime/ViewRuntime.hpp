#pragma once

#include "RuntimeFwd.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Integration/ViewHost.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Module.hpp>
#include <Aero/RuntimeTypes.hpp>
#include <Aero/UiDocument.hpp>

#include <cstdint>

namespace Aero::Core {
class IDiagnosticSink;
class EffectiveValueEngine;
class MetadataDomain;
class MetadataRuntime;
}

namespace Aero::Controls {
class ContentControl;
class VisualStateManager;
}

namespace Aero::Platform {
class IClipboard;
class ITextInputMethodHost;
}

namespace Aero {
class ObjectTree;
class ResourceDictionary;
}
namespace Aero::Render {
class RenderManager;
}

namespace Aero::Markup {
class EmbeddedSourceProvider;
class DocumentCache;
class Schema;
class SourceProviderRegistry;
}

namespace Aero {

class SchemaBundle;

using ViewRuntimeOptions = Integration::ViewHostOptions;
using RuntimeFrameResult = ViewFrameResult;

class ViewRuntime final {
public:
    explicit ViewRuntime(
        Base::IAllocator* allocator = nullptr) noexcept;
    ViewRuntime(
        SchemaBundle& schema,
        Base::IAllocator* allocator = nullptr) noexcept;
    ViewRuntime(
        SchemaBundle& schema,
        Markup::DocumentCache& documentCache,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~ViewRuntime() noexcept;

    ViewRuntime(const ViewRuntime&) = delete;
    ViewRuntime& operator=(const ViewRuntime&) = delete;

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Initialize(
        const ViewRuntimeOptions& options) noexcept;
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
        Integration::ISourceProvider& provider,
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
    Base::Result<void> SetResourceDictionary(
        RuntimeResourceLayer layer,
        Aero::ResourceDictionary& dictionary,
        RuntimeResourceLoadMode mode =
            RuntimeResourceLoadMode::Replace) noexcept;
    Base::Result<void> LoadBuiltInTheme(
        BuiltInTheme theme) noexcept;
    Base::Result<void> Mount(
        Aero::Size availableSize) noexcept;
    Base::Result<void> Mount(
        Base::Ref<Base::Object> root,
        Aero::Size availableSize) noexcept;
    Base::Result<void> Mount(
        UiDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> ReplaceMountedDocument(
        UiDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> MountContent(
        Controls::ContentControl& host,
        UiDocument&& document) noexcept;
    Base::Result<void> UnmountContent(
        Controls::ContentControl& host) noexcept;
    Base::Result<void> Resize(
        Aero::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;

    Base::Result<RuntimeFrameResult> RunFrame() noexcept;
    Base::Result<Input::PointerDispatchResult> DispatchPointer(
        const Input::PointerInput& input) noexcept;
    Base::Result<Input::KeyboardDispatchResult> DispatchKeyboard(
        const Input::KeyboardInput& input) noexcept;
    Base::Result<Input::TextInputDispatchResult> DispatchText(
        const Input::TextInput& input) noexcept;
    Base::Result<std::uint32_t> AdvanceTime(
        std::uint32_t elapsedMilliseconds) noexcept;
    Base::Result<std::uint32_t> AdvanceAnimationTime(
        std::uint32_t elapsedMilliseconds) noexcept;
    Base::Result<void>
        SetRenderBatchingEnabledForTesting(
            bool enabled) noexcept;
    Base::Result<void> SetRenderEndpoint(
        Base::Ref<Integration::RenderEndpoint> endpoint,
        bool automaticAnimationClock) noexcept;

    const Base::Ref<Base::Object>& Root() const noexcept;
    Base::Object* FindNamedObject(
        Base::StringView name,
        Core::TypeId expectedType = Core::InvalidTypeId) noexcept;
    std::uint32_t NamedObjectCount() const noexcept;

    template<class T>
    T* FindNamed(Base::StringView name) noexcept {
        return static_cast<T*>(
            FindNamedObject(name, T::StaticTypeId()));
    }

    Core::MetadataDomain* Metadata() noexcept;
    Core::MetadataRuntime* MetadataRuntime() noexcept;
    Core::EffectiveValueEngine* EffectiveValues() noexcept;
    Aero::Detail::AnimationManager* Animations() noexcept;
    Aero::ObjectTree* Tree() noexcept;
    Aero::Detail::LayoutManager* Layout() noexcept;
    Render::RenderManager* Renderer() noexcept;
    Aero::Detail::BindingManager* Bindings() noexcept;
    Aero::Detail::CommandManager* Commands() noexcept;
    Aero::Detail::RoutedEventManager* RoutedEvents() noexcept;
    Aero::Detail::FocusManager* Focus() noexcept;
    Controls::TemplateManager* Templates() noexcept;
    Controls::VisualStateManager* VisualStates() noexcept;
    Markup::Schema* Schema() noexcept;
    Markup::SourceProviderRegistry* Sources() noexcept;
    Markup::EmbeddedSourceProvider* EmbeddedSources() noexcept;
    Markup::DocumentCache* DocumentCache() noexcept;
    const Base::ResourceUri& CurrentDocumentUri() const noexcept;
    Base::Span<const Base::ResourceUri> CurrentDocumentDependencies() const noexcept;
    Aero::ResourceDictionary* ApplicationResources() noexcept;
    Aero::ResourceDictionary* ThemeResources() noexcept;
    Aero::ResourceDictionary* SystemResources() noexcept;
    Aero::Detail::StyleManager* Styles() noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
