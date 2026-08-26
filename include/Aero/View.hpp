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
// Source-only hub state defined in src/gui/ViewState.hpp. Incomplete here so
// View methods can keep a private pointer without installing ViewState.
struct ViewState;

namespace Markup {
class XamlReader;
class XamlDocument;
}
// Host-driven retained-mode view. View::Update() advances UI state; the
// per-View Renderer synchronizes and renders the retained frame. XAML,
// resource-layer and fragment operations live on Markup::XamlReader.
class AERO_GUI_API View final : public Base::Object {
    struct ConstructionToken {};

public:
    View(
        ConstructionToken,
        Gui& gui,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~View() noexcept override;

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    Result<void> SetContent(
        Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    Result<void> SetContent(
        Ref<FrameworkElement> root,
        Aero::Size availableSize) noexcept;
    Result<void> SetContent(
        Ref<FrameworkElement> root) noexcept;
    // Mounts a loaded UI document under a host root (for example a Window
    // wrapping a UserControl StartupUri root). The document's root is
    // attached as a visual child of the host.
    Result<void> SetContent(
        Ref<FrameworkElement> root,
        Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    FrameworkElement* GetContent() noexcept;
    const FrameworkElement* GetContent() const noexcept;
    Gui& GetGui() noexcept;
    const Gui& GetGui() const noexcept;

    Result<void> SetViewport(const ViewViewport& viewport) noexcept;
    void SetSize(Aero::Size availableSize) noexcept;
    void SetSize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    void SetScale(double scale) noexcept;
    // Advances this View to an absolute, monotonically increasing host time.
    // Frame failures are reported through ViewOptions::diagnostics; render
    // device failures are additionally exposed by RenderDevice::State().
    bool Update(double timeInSeconds) noexcept;
    void Activate() noexcept;
    void Deactivate() noexcept;

    bool MouseMove(int x, int y) noexcept;
    bool MouseButtonDown(
        int x,
        int y,
        Input::MouseButton button) noexcept;
    bool MouseButtonUp(
        int x,
        int y,
        Input::MouseButton button) noexcept;
    bool MouseDoubleClick(
        int x,
        int y,
        Input::MouseButton button) noexcept;
    bool MouseWheel(
        int x,
        int y,
        int delta) noexcept;
    bool MouseHWheel(
        int x,
        int y,
        int delta) noexcept;
    bool KeyDown(Input::Key key) noexcept;
    bool KeyUp(Input::Key key) noexcept;
    bool Char(std::uint32_t codePoint) noexcept;
    bool TouchDown(
        int x,
        int y,
        std::uint64_t id) noexcept;
    bool TouchMove(
        int x,
        int y,
        std::uint64_t id) noexcept;
    bool TouchUp(
        int x,
        int y,
        std::uint64_t id) noexcept;
    IRenderer& GetRenderer() noexcept;
    const IRenderer& GetRenderer() const noexcept;

private:
    Result<void> Initialize(const ViewOptions& options) noexcept;

    friend class Gui;
    friend class ViewRenderer;
    friend struct ViewState;
    friend class Media::CompositionTarget;
    friend class Markup::XamlReader;
    friend class App::DesktopHost;
    template<class T, class... Args>
    friend Result<Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    ViewState* state_ = nullptr;
    double updateTimeSeconds_ = 0.0;
    bool hasUpdateTime_ = false;
    bool active_ = true;
};

} // namespace Aero
