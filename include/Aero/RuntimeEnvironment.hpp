#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Module.hpp>
#include <Aero/RuntimeTypes.hpp>
#include <Aero/UiDocument.hpp>

#include <cstdint>

namespace Aero::Core {
class IDiagnosticSink;
}

namespace Aero {

class View;
namespace Detail {
class ViewAccess;
}

namespace Integration {
class ISourceProvider;
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
        Presentation::Size availableSize) noexcept;
    Base::Result<void> LoadContent(
        Base::StringView uri,
        Presentation::Size availableSize,
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
    Base::Result<void> LoadBuiltInTheme(
        BuiltInTheme theme) noexcept;

    Base::Result<void> Resize(
        Presentation::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;
    Base::Result<ViewFrameResult> RunFrame() noexcept;
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
        return static_cast<T*>(
            FindNamedObject(name, T::StaticTypeId()));
    }

private:
    friend class RuntimeEnvironment;
    friend class Detail::ViewAccess;
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
