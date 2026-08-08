#include <Aero/Gui.hpp>
#include <Aero/Gui/View.hpp>

#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Text/FontProvider.hpp>
#include <Aero/ViewOptions.hpp>
#include "gui/GuiData.hpp"
#include <Aero/BuiltinThemes.generated.hpp>

#include <new>
#include <utility>


namespace Aero {

Base::Result<Base::ResourceUri> BuiltInThemeUri(
    Base::StringView name) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(
        Base::StringView("pack://application:,,,/Aero.Themes;component/"));
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> appended = text.Append(name);
    if (!appended) return appended.GetStatus();
    return Base::ResourceUri::Parse(text.View());
}

Base::Result<void> RegisterDefaultXamlProviders(
    Markup::XamlProviderRegistry& providers,
    Markup::EmbeddedXamlProvider& embedded,
    Markup::FileXamlProvider& file) noexcept {
    Base::Result<Base::ResourceUri> light = BuiltInThemeUri("Light.xaml");
    if (!light) return light.GetStatus();
    Base::Result<void> status = embedded.Add(
        light.Value(), {::Aero::AeroThemeLightSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeLightSource))});
    if (!status) return status.GetStatus();
    Base::Result<Base::ResourceUri> dark = BuiltInThemeUri("Dark.xaml");
    if (!dark) return dark.GetStatus();
    status = embedded.Add(
        dark.Value(), {::Aero::AeroThemeDarkSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeDarkSource))});
    if (!status) return status.GetStatus();
    Base::Result<Base::ResourceUri> generic = BuiltInThemeUri("Generic.xaml");
    if (!generic) return generic.GetStatus();
    status = embedded.Add(
        generic.Value(), {::Aero::AeroThemeGenericSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeGenericSource))});
    if (!status) return status.GetStatus();
    Base::Result<Base::ResourceUri> lightBlue = Base::ResourceUri::Parse(
        "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.LightBlue.xaml");
    if (!lightBlue) return lightBlue.GetStatus();
    status = embedded.Add(
        lightBlue.Value(), {::Aero::AeroThemeLightSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeLightSource))});
    if (!status) return status.GetStatus();
    Base::Result<Base::ResourceUri> darkBlue = Base::ResourceUri::Parse(
        "pack://application:,,,/Aero.GUI.Extensions;component/Theme/AeroTheme.DarkBlue.xaml");
    if (!darkBlue) return darkBlue.GetStatus();
    status = embedded.Add(
        darkBlue.Value(), {::Aero::AeroThemeDarkSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeDarkSource))});
    if (!status) return status.GetStatus();

    auto registerProvider = [&](Markup::XamlProvider& provider,
                                Base::StringView scheme) noexcept -> Base::Result<void> {
        Base::Result<void> registered = providers.Register(provider, scheme);
        return !registered && registered.GetStatus().code != Base::ErrorCode::AlreadyExists
            ? Base::Result<void>(registered.GetStatus()) : Base::Result<void>();
    };
    status = registerProvider(embedded, "pack");
    if (!status) return status.GetStatus();
    status = registerProvider(file, "file");
    if (!status) return status.GetStatus();
    return registerProvider(file, {});
}

} // namespace Aero


namespace Aero {


Gui::Gui(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    Base::Result<Base::Ref<GuiState>> made =
        Base::MakeRefWithAllocator<GuiState>(
            *allocator_, *allocator_);
    if (!made) {
        Base::ReportOutOfMemory(
            sizeof(GuiState),
            alignof(GuiState),
            Base::MemoryTag::Object);
    }
    state_ = Base::Ref<Base::Object>(std::move(made).Value());
}

Gui::~Gui() noexcept {
    state_.Reset();
}

Base::Result<void> Gui::AddModule(
    const ModuleRegistration& registration) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui modules are frozen");
    }
    return state.modules.Add(registration);
}

Base::Result<void> Gui::AddXamlProvider(
    Markup::XamlProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui providers are frozen after Initialize");
    }
    return state.xamlProviders.Register(provider, scheme, assembly);
}

Base::Result<void> Gui::AddTextureProvider(
    Media::TextureProvider& provider) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui providers are frozen after Initialize");
    }
    state.textureProvider = &provider;
    return {};
}

Base::Result<void> Gui::AddFontProvider(
    Text::FontProvider& provider) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui providers are frozen after Initialize");
    }
    state.fontProvider = &provider;
    return {};
}

Base::Result<void> Gui::Initialize() noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) return {};
    Base::Result<void> prepared = state.schema.Prepare(state.modules);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> finalized = state.schema.Finalize(
        GuiSchemaOptions{state.allocator});
    if (!finalized) return finalized.GetStatus();
    Base::Result<void> providers =
        Aero::RegisterDefaultXamlProviders(
            state.xamlProviders, state.embeddedXaml, state.fileXaml);
    if (!providers) return providers.GetStatus();
    Base::Result<void> frozen = state.modules.Freeze();
    if (!frozen) return frozen.GetStatus();
    state.initialized = true;
    return {};
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    Base::IAllocator* allocator) noexcept {
    return CreateView(ViewOptions{}, allocator);
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    const ViewOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before creating a view");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : *allocator_;
    Base::Result<Base::Ref<View>> made =
        Base::MakeRefWithAllocator<View>(
            selected,
            View::ConstructionToken{},
            *this,
            &selected);
    if (!made) return made.GetStatus();
    Base::Result<void> initialized =
        made.Value()->Initialize(options);
    if (!initialized) return initialized.GetStatus();
    return std::move(made).Value();
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    Base::Ref<FrameworkElement> content,
    const ViewOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!content) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View content is required");
    }
    Base::Result<Base::Ref<View>> made =
        CreateView(options, allocator);
    if (!made) return made.GetStatus();
    Base::Result<void> mounted =
        made.Value()->SetContent(std::move(content));
    if (!mounted) return mounted.GetStatus();
    return std::move(made).Value();
}

bool Gui::IsInitialized() const noexcept {
    const GuiState& state = static_cast<const GuiState&>(*state_);
    return state.initialized && state.schema.IsFrozen();
}

} // namespace Aero
