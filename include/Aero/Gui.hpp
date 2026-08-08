#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Module.hpp>
#include <Aero/ViewOptions.hpp>

namespace Aero {

class View;
class FrameworkElement;
namespace Markup {
class XamlProvider;
class XamlReader;
class ReloadCoordinator;
}
namespace Media { class TextureProvider; }
namespace Text { class FontProvider; }

// Process-level WPF/XAML runtime. Gui owns the frozen schema, providers and
// document cache; Views own presentation-affine layout/input/render state.
class AERO_GUI_API Gui {
public:
    explicit Gui(Base::IAllocator* allocator = nullptr) noexcept;
    ~Gui() noexcept;

    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> AddXamlProvider(
        Markup::XamlProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;
    Base::Result<void> AddTextureProvider(
        Media::TextureProvider& provider) noexcept;
    Base::Result<void> AddFontProvider(
        Text::FontProvider& provider) noexcept;
    Base::Result<void> Initialize() noexcept;
    Base::Result<Base::Ref<View>> CreateView(
        Base::IAllocator* allocator = nullptr) noexcept;
    Base::Result<Base::Ref<View>> CreateView(
        const ViewOptions& options,
        Base::IAllocator* allocator = nullptr) noexcept;
    Base::Result<Base::Ref<View>> CreateView(
        Base::Ref<FrameworkElement> content,
        const ViewOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    bool IsInitialized() const noexcept;

private:
    friend class View;
    friend class Markup::XamlReader;
    friend class Markup::ReloadCoordinator;

    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<Base::Object> state_;
};

} // namespace Aero
