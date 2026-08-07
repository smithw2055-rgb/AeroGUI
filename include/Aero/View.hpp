#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Input.hpp>
#include <Aero/Layout.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/Markup.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Module.hpp>

#include <cstdint>

namespace Aero::Diagnostics {
class IDiagnosticSink;
}

namespace Aero {

namespace App::Detail { class DesktopHost; }
namespace Runtime::Detail { class ViewRenderer; }

enum class BuiltInTheme : std::uint8_t { Light = 0U, Dark };
enum class ResourceLayer : std::uint8_t { Application = 0U, Theme, System };
enum class ResourceLoadMode : std::uint8_t { Replace = 0U, Merge };

class FrameworkElement;
class Gui;
class View;
namespace Controls { class ContentControl; }

// Global frame notification matching WPF CompositionTarget.Rendering. Hosts
// still own the frame clock through View::Update; subscribers use this event to
// invalidate custom visuals immediately before retained render commit.
using RenderingEventHandler = Base::Delegate<void()>;

class AERO_API CompositionTarget final {
public:
    // Explicit View overloads are preferred for multi-view hosts. The legacy
    // overloads remain dispatcher-thread scoped for WPF-shaped source
    // compatibility.
    static void AddRendering(
        View& view,
        const RenderingEventHandler& handler) noexcept;
    static bool RemoveRendering(
        View& view,
        const RenderingEventHandler& handler) noexcept;
    static void AddRendering(
        const RenderingEventHandler& handler) noexcept;
    static bool RemoveRendering(
        const RenderingEventHandler& handler) noexcept;

private:
    friend class View;
    static void RaiseRendering(View& view) noexcept;
};
namespace Markup {
class XamlReader;
class XamlDocument;
class XamlProvider;
class ReloadCoordinator;
}
namespace Media { class TextureProvider; }
namespace Text { class FontProvider; }
struct ViewOptions;

// Host-driven retained-mode view. View::Update() advances UI state; the
// per-View Renderer synchronizes and renders the retained frame. XAML,
// resource-layer and fragment operations live on Markup::XamlReader.
class AERO_API View final : public Base::Object {
    struct ConstructionToken {};
    struct FrameResult;

public:
    struct Impl;
    struct Viewport {
        Aero::Size logicalSize{};
        std::uint32_t pixelWidth = 0U;
        std::uint32_t pixelHeight = 0U;
        double dpiScale = 1.0;
    };

    View(
        ConstructionToken,
        Gui& gui,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~View() noexcept override;

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    Base::Result<void> SetContent(
        Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> SetContent(
        Base::Ref<FrameworkElement> root,
        Aero::Size availableSize) noexcept;
    FrameworkElement* GetContent() noexcept;
    const FrameworkElement* GetContent() const noexcept;
    Gui& GetGui() noexcept;
    const Gui& GetGui() const noexcept;

    void SetSize(Aero::Size availableSize) noexcept;
    void SetViewport(const Viewport& viewport) noexcept;
    Viewport GetViewport() const noexcept;
    Base::Result<void> Update(
        std::uint32_t elapsedMilliseconds = 0U) noexcept;
    IRenderer& GetRenderer() noexcept;
    const IRenderer& GetRenderer() const noexcept;

    Base::Result<Input::PointerDispatchResult> DispatchPointer(
        const Input::PointerInput& input) noexcept;
    Base::Result<Input::KeyboardDispatchResult> DispatchKeyboard(
        const Input::KeyboardInput& input) noexcept;
    Base::Result<Input::TextInputDispatchResult> DispatchText(
        const Input::TextInput& input) noexcept;

private:
    friend class Gui;
    friend class Runtime::Detail::ViewRenderer;
    friend class CompositionTarget;
    friend class Markup::XamlReader;
    friend class Markup::ReloadCoordinator;
    friend class App::Detail::DesktopHost;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    Base::Result<void> MountContent(
        Controls::ContentControl& host,
        Markup::XamlDocument&& document) noexcept;
    Base::Result<void> UnmountContent(
        Controls::ContentControl& host) noexcept;
    Base::Result<void> LoadResources(
        ResourceLayer layer,
        Base::StringView uri,
        ResourceLoadMode mode = ResourceLoadMode::Replace,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> LoadCompiledResources(
        ResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept;
    void SetResourceDictionary(
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
        const ::Aero::ViewOptions& options) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;
    bool IsMounted() const noexcept;
    Base::Result<void> Mount(Aero::Size availableSize) noexcept;
    Base::Result<void> Mount(
        Base::Ref<Base::Object> root,
        Aero::Size availableSize) noexcept;
    Base::Result<void> Mount(
        Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> ReplaceMountedDocument(
        Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> Unmount() noexcept;
    Base::Object* FindNamedObject(
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept;
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
        Meta::TypeId baseType) const noexcept;

    Impl* state_ = nullptr;
};

} // namespace Aero
