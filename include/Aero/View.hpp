#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Input.hpp>
#include <Aero/Layout.hpp>
#include <Aero/IRenderer.hpp>
#include <Aero/Media/CompositionTarget.hpp>
#include <Aero/ViewOptions.hpp>

#include <cstdint>

namespace Aero {

namespace App { class DesktopHost; }

class FrameworkElement;
class Gui;
class View;
class ViewRenderer;
// Source-only hub state defined in src/gui/ViewFrame.hpp. Incomplete here so
// View methods can keep a private pointer without installing ViewFrame.
struct ViewFrame;

namespace Markup {
class XamlReader;
class XamlDocument;
class ReloadCoordinator;
struct ReloadCoordinatorState;
}

// P4.3: read-only committed-frame identity for content-stability gates
// (hosts and conformance tests). version 0 means no frame has been
// committed yet. contentHash is a content hash over nodes/commands (no
// generation markers): identical visuals hash identically, so round-trip
// comparisons detect any commit-path divergence without pixel readback.
struct CommittedFrameInfo {
    std::uint64_t version = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint64_t contentHash = 0U;
};

// Host-driven retained-mode view. View::Update() advances UI state; the
// per-View Renderer synchronizes and renders the retained frame. XAML,
// resource-layer and fragment operations live on Markup::XamlReader.
class AERO_GUI_API View final : public Base::Object {
    struct ConstructionToken {};

public:
    View(ConstructionToken, Gui& gui, Base::IAllocator* allocator = nullptr) noexcept;
    ~View() noexcept override;

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    template<class T = FrameworkElement> Result<Ref<T>> LoadXaml(StringView uri) noexcept {
        static_assert(std::is_base_of<Base::Object, T>::value, "View::LoadXaml<T> requires an Aero object type");
        Result<Ref<Object>> loaded = LoadXamlRoot(uri, T::StaticTypeId());
        if (!loaded) return loaded.GetStatus();
        T* root = static_cast<T*>(loaded.Value().Get());
        return Ref<T>::FromBorrowed(*root);
    }

    bool SetContent(Markup::XamlDocument&& document, Aero::Size availableSize) noexcept;
    bool SetContent(Ref<FrameworkElement> root, Aero::Size availableSize) noexcept;
    // Transitional: prefers the current viewport size. New code calls
    // SetContent(root, size) explicitly; this overload will be removed.
    [[deprecated("Use SetContent(root, availableSize) instead")]]
    bool SetContent(Ref<FrameworkElement> root) noexcept;
    // Mounts a loaded UI document under a host root (for example a Window
    // wrapping a UserControl StartupUri root). The document's root is
    // attached as a visual child of the host. New fragment-style mounts
    // should use Markup::XamlReader::MountFragment instead.
    bool SetContent(Ref<FrameworkElement> root, Markup::XamlDocument&& document, Aero::Size availableSize) noexcept;
    FrameworkElement* GetContent() noexcept;
    const FrameworkElement* GetContent() const noexcept;
    Gui& GetGui() noexcept;
    const Gui& GetGui() const noexcept;

    bool SetViewport(const ViewViewport& viewport) noexcept;
    void SetSize(Aero::Size availableSize) noexcept;
    void SetSize(std::uint32_t width, std::uint32_t height) noexcept;
    Aero::Size GetSize() const noexcept;
    void SetScale(double scale) noexcept;
    // Advances this View to an absolute, monotonically increasing host time.
    // Frame failures are reported through ViewOptions::diagnostics; render
    // device failures are additionally exposed by RenderDevice::State().
    bool Update(double timeInSeconds) noexcept;
    void Activate() noexcept;
    void Deactivate() noexcept;

    bool MouseMove(int x, int y) noexcept;
    bool MouseButtonDown(int x, int y, Input::MouseButton button) noexcept;
    bool MouseButtonUp(int x, int y, Input::MouseButton button) noexcept;
    bool MouseDoubleClick(int x, int y, Input::MouseButton button) noexcept;
    bool MouseWheel(int x, int y, int delta) noexcept;
    bool MouseHWheel(int x, int y, int delta) noexcept;
    bool KeyDown(Input::Key key) noexcept;
    bool KeyUp(Input::Key key) noexcept;
    bool Char(std::uint32_t codePoint) noexcept;
    bool TouchDown(int x, int y, std::uint64_t id) noexcept;
    bool TouchMove(int x, int y, std::uint64_t id) noexcept;
    bool TouchUp(int x, int y, std::uint64_t id) noexcept;
    IRenderer& GetRenderer() noexcept;
    const IRenderer& GetRenderer() const noexcept;
    CommittedFrameInfo GetCommittedFrameInfo() const noexcept;

private:
    Result<void> Initialize(const ViewOptions& options) noexcept;
    Result<Ref<Object>> LoadXamlRoot(StringView uri, Base::MetaTypeId expectedRoot) noexcept;

    friend class Gui;
    friend class ViewRenderer;
    friend struct ViewFrame;
    friend class Media::CompositionTarget;
    friend class Markup::XamlReader;
    friend class App::DesktopHost;
    template<class T, class... Args> friend Ref<T> Base::MakeRefWithAllocator(Base::IAllocator&,
        Args&&...) noexcept;

    ViewFrame* state_ = nullptr;
    double updateTimeSeconds_ = 0.0;
    bool hasUpdateTime_ = false;
    bool active_ = true;
};

} // namespace Aero
