#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Module.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Input/Values.hpp>
#include <Aero/Markup/XamlDocument.hpp>

#include <cstdint>

namespace Aero::Core {
class IDiagnosticSink;
}

namespace Aero {

enum class BuiltInTheme : std::uint8_t { Light = 0U, Dark };

enum class RuntimeResourceLayer : std::uint8_t { Application = 0U, Theme, System };

enum class RuntimeResourceLoadMode : std::uint8_t { Replace = 0U, Merge };

struct ViewLayoutDiagnostics final {
    std::uint64_t passVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
};

struct ViewRenderDiagnostics final {
    std::uint64_t snapshotVersion = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t glyphCommandCount = 0U;
    std::uint32_t dirtyCount = 0U;
    std::uint64_t snapshotHash = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t stateBindingCount = 0U;
    bool batchingEnabled = true;
};

struct ViewFrameResult final {
    std::uint64_t frameNumber = 0U;
    std::uint32_t callbackCount = 0U;
    ViewLayoutDiagnostics layout;
    ViewRenderDiagnostics render;
};

} // namespace Aero

namespace Aero {

class View;
namespace Controls {
class ContentControl;
}
namespace Detail {
class ViewAccess;
}

namespace Integration {
class ISourceProvider;
class RenderEndpoint;
class ReloadCoordinator;
class ViewHost;
struct ViewHostOptions;
}

// Process/application-level immutable composition. Its internal state is
// reference counted so views remain valid when the lightweight environment
// facade is released. Modules and schemas are still frozen exactly once.
class AERO_API RuntimeEnvironment final {
public:
    explicit RuntimeEnvironment(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RuntimeEnvironment() noexcept;

    RuntimeEnvironment(const RuntimeEnvironment&) = delete;
    RuntimeEnvironment& operator=(const RuntimeEnvironment&) = delete;

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> Initialize() noexcept;
    Base::Result<Base::Ref<View>> CreateView(
        Base::IAllocator* allocator = nullptr) noexcept;

    bool IsInitialized() const noexcept;

private:
    friend class Integration::ViewHost;
    friend class View;

    struct Impl;
    Base::Result<Base::Ref<View>> CreateIntegratedView(
        const Integration::ViewHostOptions& options,
        Base::IAllocator* allocator) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<Base::Object> impl_;
};

class AERO_API View final : public Base::Object {
    struct ConstructionToken final {};

public:
    // Factory-only construction: ConstructionToken is private and can only be
    // produced by RuntimeEnvironment. The declaration remains public because
    // Base::MakeRefWithAllocator verifies nothrow construction with a standard
    // type trait, which cannot inspect private constructors.
    View(
        ConstructionToken,
        RuntimeEnvironment& environment,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~View() noexcept override;

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    Base::Result<UiDocument> Load(
        Base::StringView uri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> Parse(
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> SetContent(
        UiDocument&& document,
        Aero::Size availableSize) noexcept;
    // Mounts a separately loaded XAML document into an already mounted
    // ContentControl. The document keeps its own names, resources and
    // deferred effects until UnmountContent() is called.
    Base::Result<void> MountContent(
        Controls::ContentControl& host,
        UiDocument&& document) noexcept;
    Base::Result<void> UnmountContent(
        Controls::ContentControl& host) noexcept;
    Base::Result<void> LoadContent(
        Base::StringView uri,
        Aero::Size availableSize,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;

    Base::Result<UiDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept;
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

    Base::Result<void> Resize(
        Aero::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;
    Base::Result<ViewFrameResult> RunFrame() noexcept;
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
    Base::Result<void> SetRenderEndpoint(
        Base::Ref<Integration::RenderEndpoint> endpoint,
        bool automaticAnimationClock = true) noexcept;

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

private:
    friend class RuntimeEnvironment;
    friend class Aero::Detail::ViewAccess;
    friend class Integration::ReloadCoordinator;
    friend class Integration::ViewHost;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    struct Impl;
    Base::Result<void> Initialize(
        const Integration::ViewHostOptions& options) noexcept;
    Base::Result<void> RegisterSourceProvider(
        Integration::ISourceProvider& provider,
        Base::StringView scheme,
        Base::StringView assembly) noexcept;
    void* IntegrationRuntime() noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
