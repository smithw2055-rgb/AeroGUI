#include <Aero/Gui/XamlReader.hpp>
#include <Aero/Gui.hpp>
#include <Aero/Gui/View.hpp>

#include "gui/GuiData.hpp"
#include "gui/ViewOperations.hpp"
#include "gui/PropertyInternal.hpp"

#include <utility>

namespace Aero::Markup {
namespace {

struct GuiLoadScope {
    GuiLoadScope(
        Threading::Dispatcher& dispatcher,
        GuiSchema& schema,
        DocumentCache& documents) noexcept
        : factory(
              dispatcher,
              ::Aero::GuiPrivate::Detail::MetadataPrivate::DependencyProperties(
                  schema.Metadata()),
              schema.Metadata()) {
        load.documentCache = &documents;
        load.dispatcher = &dispatcher;
        load.dependencyProperties =
            &::Aero::GuiPrivate::Detail::MetadataPrivate::DependencyProperties(
                schema.Metadata());
        load.effectCommitMode = EffectCommitMode::Deferred;
    }

    Meta::ObjectFactoryScope factory;
    LoadState load;
};

} // namespace

Base::Result<XamlDocument> XamlReader::Load(
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    Gui::Impl& state = static_cast<Gui::Impl&>(*gui_->impl_);
    GuiLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.xaml.Load(
        state.xamlProviders, &scope.load, state.allocator,
        uri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::Load(
    Base::Stream& source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    Gui::Impl& state = static_cast<Gui::Impl&>(*gui_->impl_);
    GuiLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.xaml.Parse(
        state.xamlProviders, &scope.load, state.allocator,
        source, baseUri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::LoadComponentCore(
    Base::StringView uri,
    Meta::TypeId expectedRoot,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<XamlDocument> loaded = Load(uri, settings, diagnostics);
    if (!loaded) return loaded.GetStatus();
    const Base::Ref<Base::Object>& root = loaded.Value().Root();
    if (!root || gui_ == nullptr || expectedRoot == Meta::InvalidTypeId) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    const Gui::Impl& state = static_cast<const Gui::Impl&>(*gui_->impl_);
    if (!state.schema.Metadata().Types().IsDerivedFrom(
            root->RuntimeType(), expectedRoot)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "XAML component root is incompatible with the requested type");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::LoadComponentInto(
    Base::Ref<Base::Object> existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML component loading");
    }
    if (!existingRoot) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML component requires an existing root object");
    }
    Gui::Impl& state = static_cast<Gui::Impl&>(*gui_->impl_);
    GuiLoadScope scope(state.dispatcher, state.schema, state.documents);
    Base::Result<XamlDocument> loaded = state.xaml.LoadComponentInto(
        state.xamlProviders,
        &scope.load,
        state.allocator,
        *existingRoot,
        uri,
        settings,
        diagnostics);
    if (!loaded) return loaded.GetStatus();
    if (loaded.Value().Root().Get() != existingRoot.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML component replaced its existing root object");
    }
    return std::move(loaded).Value();
}

Base::Result<XamlDocument> XamlReader::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& settings,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML parsing");
    }
    Gui::Impl& state = static_cast<Gui::Impl&>(*gui_->impl_);
    GuiLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.xaml.Parse(
        state.xamlProviders, &scope.load, state.allocator,
        source, baseUri, settings, diagnostics);
}

Base::Result<XamlDocument> XamlReader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    if (gui_ == nullptr || !gui_->IsInitialized()) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Gui must be initialized before compiled XAML loading");
    }
    Gui::Impl& state = static_cast<Gui::Impl&>(*gui_->impl_);
    GuiLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.xaml.LoadCompiled(
        state.xamlProviders, &scope.load, state.allocator,
        bytes, originUri, XamlReaderSettings{});
}

Base::Result<void> XamlReader::Mount(
    Aero::View& view,
    Controls::ContentControl& host,
    XamlDocument&& document) noexcept {
    return View::Operations::MountContent(
        view, host, std::move(document));
}

Base::Result<void> XamlReader::Unmount(
    Aero::View& view,
    Controls::ContentControl& host) noexcept {
    return View::Operations::UnmountContent(view, host);
}

Base::Result<void> XamlReader::LoadResources(
    Aero::View& view,
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    return View::Operations::LoadResources(
        view, layer, uri, mode, diagnostics);
}

Base::Result<void> XamlReader::LoadCompiledResources(
    Aero::View& view,
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    return View::Operations::LoadCompiledResources(
        view, layer, bytes, originUri, mode);
}

void XamlReader::SetResources(
    Aero::View& view,
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    View::Operations::SetResourceDictionary(
        view, layer, dictionary, mode);
}

Base::Result<void> XamlReader::LoadTheme(
    Aero::View& view,
    BuiltInTheme theme) noexcept {
    return View::Operations::LoadBuiltInTheme(view, theme);
}

} // namespace Aero::Markup
