#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Input.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Gui/IRenderer.hpp>
#include <Aero/ViewOptions.hpp>

#include <cstdint>

namespace Aero {

namespace App { class DesktopHost; }

class FrameworkElement;
class Gui;
class View;
class ViewRenderer;
struct ViewState;

// Global frame notification matching WPF CompositionTarget.Rendering. Hosts
// still own the frame clock through View::Update; subscribers use this event to
// invalidate custom visuals immediately before retained render commit.
using RenderingEventHandler = Base::Delegate<void()>;

namespace Media {

class AERO_GUI_API CompositionTarget final {
public:
    // Explicit View overloads are preferred for multi-view hosts. The legacy
    // overloads remain dispatcher-thread scoped for WPF-shaped source
    // compatibility.
    static void AddRendering(
        ::Aero::View& view,
        const ::Aero::RenderingEventHandler& handler) noexcept;
    static bool RemoveRendering(
        ::Aero::View& view,
        const ::Aero::RenderingEventHandler& handler) noexcept;
    static void AddRendering(
        const ::Aero::RenderingEventHandler& handler) noexcept;
    static bool RemoveRendering(
        const ::Aero::RenderingEventHandler& handler) noexcept;

private:
    friend class ::Aero::View;
    friend struct ::Aero::ViewState;
    static void RaiseRendering(::Aero::View& view) noexcept;
};

} // namespace Media
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

    Base::Result<void> SetContent(
        Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    Base::Result<void> SetContent(
        Base::Ref<FrameworkElement> root,
        Aero::Size availableSize) noexcept;
    Base::Result<void> SetContent(
        Base::Ref<FrameworkElement> root) noexcept;
    FrameworkElement* GetContent() noexcept;
    const FrameworkElement* GetContent() const noexcept;
    Gui& GetGui() noexcept;
    const Gui& GetGui() const noexcept;

    void SetSize(Aero::Size availableSize) noexcept;
    void SetSize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    void SetScale(double scale) noexcept;
    Base::Result<void> Update(
        std::uint32_t elapsedMilliseconds = 0U) noexcept;
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
    bool MouseWheel(
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
    Base::Result<void> Initialize(const ViewOptions& options) noexcept;

    friend class Gui;
    friend class ViewRenderer;
    friend struct ViewState;
    friend class Media::CompositionTarget;
    friend class Markup::XamlReader;
    friend class App::DesktopHost;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    ViewState* state_ = nullptr;
    double updateTimeSeconds_ = 0.0;
    bool hasUpdateTime_ = false;
    bool active_ = true;
};

} // namespace Aero
