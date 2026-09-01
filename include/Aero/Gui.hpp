#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Module.hpp>
#include <Aero/ViewOptions.hpp>

#include <type_traits>

namespace Aero {

class View;
class FrameworkElement;
class ResourceDictionary;
namespace Markup {
class XamlDocument;
class XamlProvider;
class XamlReader;
class ReloadCoordinator;
}
namespace Media { class TextureProvider; class FontProvider; }

// Process-level WPF/XAML runtime. Gui owns the frozen schema, providers and
// document cache; Views own presentation-affine layout/input/render state.
class AERO_GUI_API Gui {
public:
    explicit Gui(Base::IAllocator* allocator = nullptr) noexcept;
    ~Gui() noexcept;

    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;

    Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    Result<void> SetXamlProvider(
        Ref<Markup::XamlProvider> provider,
        StringView scheme = {},
        StringView assembly = {}) noexcept;
    Result<void> SetTextureProvider(
        Ref<Media::TextureProvider> provider) noexcept;
    Result<void> SetFontProvider(
        Ref<Media::FontProvider> provider) noexcept;
    Result<void> Initialize() noexcept;
    template<class T = FrameworkElement>
    Result<Ref<T>> LoadXaml(
        StringView uri) noexcept {
        static_assert(std::is_base_of<Base::Object, T>::value,
            "Gui::LoadXaml<T> requires an Aero object type");
        Result<Ref<Object>> loaded =
            LoadXamlRoot(uri, T::StaticTypeId());
        if (!loaded) return loaded.GetStatus();
        T* root = static_cast<T*>(loaded.Value().Get());
        return Ref<T>::FromBorrowed(*root);
    }
    Result<void> LoadComponent(
        Base::Object& component,
        StringView uri,
        ResourceDictionary* resources = nullptr) noexcept;
    Result<Ref<View>> CreateView(
        Base::IAllocator* allocator = nullptr) noexcept;
    Result<Ref<View>> CreateView(
        const ViewOptions& options,
        Base::IAllocator* allocator = nullptr) noexcept;
    Result<Ref<View>> CreateView(
        Ref<FrameworkElement> content,
        const ViewOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    bool IsInitialized() const noexcept;

private:
    friend class View;
    friend class Markup::XamlReader;
    friend class Markup::ReloadCoordinator;

    Result<Ref<Object>> LoadXamlRoot(
        StringView uri,
        Base::MetaTypeId expectedRoot) noexcept;
    Result<bool> TakeLoadedDocument(
        Base::Object& root,
        Markup::XamlDocument& document) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Ref<Object> state_;
};

} // namespace Aero
