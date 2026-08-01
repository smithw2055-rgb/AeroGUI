#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Input/Values.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Markup.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Module.hpp>

#include <cstdint>

namespace Aero::Core {
class IDiagnosticSink;
}

namespace Aero {

enum class BuiltInTheme : std::uint8_t { Light = 0U, Dark };
enum class ResourceLayer : std::uint8_t { Application = 0U, Theme, System };
enum class ResourceLoadMode : std::uint8_t { Replace = 0U, Merge };

class FrameworkElement;
class View;
namespace Controls { class ContentControl; }
namespace Detail { struct ViewData; }
namespace Markup { class XamlReader; }
namespace App::Detail { class DesktopHost; }

namespace Integration {
class ISourceProvider;
class RenderDevice;
class ReloadCoordinator;
struct ViewOptions;
}

// Process-level GUI composition used by embedded hosts. Application owns this
// object automatically for desktop programs; engine integrations create one,
// register optional modules, initialize it once and create one or more Views.
class AERO_API Gui final {
public:
    explicit Gui(Base::IAllocator* allocator = nullptr) noexcept;
    ~Gui() noexcept;

    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> Initialize() noexcept;
    Base::Result<Base::Ref<View>> CreateView(
        Base::IAllocator* allocator = nullptr) noexcept;
    Base::Result<Base::Ref<View>> CreateView(
        const Integration::ViewOptions& options,
        Base::IAllocator* allocator = nullptr) noexcept;

    bool IsInitialized() const noexcept;

private:
    friend class View;

    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<Base::Object> impl_;
};

// Host-driven retained-mode view. The public surface is intentionally limited
// to content, size, frame update, input and RenderDevice attachment. XAML,
// resource-layer and fragment operations live on Markup::XamlReader.
class AERO_API View final : public Base::Object {
    struct ConstructionToken final {};
    struct FrameResult;

public:
    View(
        ConstructionToken,
        Gui& gui,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~View() noexcept override;

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    Base::Result<void> SetContent(
        UiDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> SetContent(
        Base::Ref<FrameworkElement> root,
        Aero::Size availableSize) noexcept;
    FrameworkElement* GetContent() noexcept;
    const FrameworkElement* GetContent() const noexcept;

    Base::Result<void> SetSize(Aero::Size availableSize) noexcept;
    Base::Result<void> Update(
        std::uint32_t elapsedMilliseconds = 0U) noexcept;

    Base::Result<Input::PointerDispatchResult> DispatchPointer(
        const Input::PointerInput& input) noexcept;
    Base::Result<Input::KeyboardDispatchResult> DispatchKeyboard(
        const Input::KeyboardInput& input) noexcept;
    Base::Result<Input::TextInputDispatchResult> DispatchText(
        const Input::TextInput& input) noexcept;

    Base::Result<void> SetRenderDevice(
        Base::Ref<Integration::RenderDevice> device,
        bool automaticAnimationClock = true) noexcept;

private:
    friend class Gui;
    friend class Aero::Markup::XamlReader;
    friend class Aero::App::Detail::DesktopHost;
    friend class Integration::ReloadCoordinator;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    Base::Result<UiDocument> LoadDocument(
        Base::StringView uri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> ParseDocument(
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> LoadCompiledDocument(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept;
    Base::Result<void> AddSourceProvider(
        Integration::ISourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;

    Base::Result<void> MountContent(
        Controls::ContentControl& host,
        UiDocument&& document) noexcept;
    Base::Result<void> UnmountContent(
        Controls::ContentControl& host) noexcept;
    Base::Result<void> LoadResources(
        ResourceLayer layer,
        Base::StringView uri,
        ResourceLoadMode mode = ResourceLoadMode::Replace,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> LoadCompiledResources(
        ResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept;
    Base::Result<void> SetResourceDictionary(
        ResourceLayer layer,
        Aero::ResourceDictionary& dictionary,
        ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept;
    Base::Result<void> LoadBuiltInTheme(BuiltInTheme theme) noexcept;

    Base::Result<FrameResult> ExecuteFrame() noexcept;
    Base::Result<std::uint32_t> AdvanceClocks(
        std::uint32_t elapsedMilliseconds) noexcept;
    Base::Result<std::uint32_t> AdvanceAnimations(
        std::uint32_t elapsedMilliseconds) noexcept;
    Base::Result<void> Initialize(
        const Integration::ViewOptions& options) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;
    bool IsMounted() const noexcept;
    Base::Result<void> Mount(Aero::Size availableSize) noexcept;
    Base::Result<void> Mount(
        Base::Ref<Base::Object> root,
        Aero::Size availableSize) noexcept;
    Base::Result<void> Mount(
        UiDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> ReplaceMountedDocument(
        UiDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;
    Base::Object* FindNamedObject(
        Base::StringView name,
        Core::TypeId expectedType = Core::InvalidTypeId) noexcept;
    std::uint32_t NamedObjectCount() const noexcept;
    Base::Result<void> QueryReloadSource(
        const Base::ResourceUri& uri,
        std::uint64_t& sourceIdentity,
        std::uint64_t& revision) noexcept;
    bool TryGetCachedReloadRevision(
        const Base::ResourceUri& uri,
        std::uint64_t sourceIdentity,
        std::uint64_t& revision) noexcept;
    Base::Result<std::uint32_t> InvalidateReloadDocuments(
        const Base::ResourceUri& uri,
        bool includeDependents) noexcept;
    bool IsInstanceOf(
        const Base::Object& object,
        Core::TypeId baseType) const noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<Base::Object> gui_;
    Aero::Detail::ViewData* state_ = nullptr;
};

} // namespace Aero
