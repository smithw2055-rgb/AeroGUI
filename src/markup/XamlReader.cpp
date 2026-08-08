#include <Aero/Gui/XamlReader.hpp>
#include <Aero/Gui.hpp>

#include "gui/GuiData.hpp"
#include "gui/PropertyRuntime.hpp"

#include <utility>

namespace Aero::Markup {
namespace {

struct XamlLoadScope {
    XamlLoadScope(
        Threading::Dispatcher& dispatcher,
        GuiSchema& schema,
        DocumentCache& documents) noexcept
        : factory(
              dispatcher,
              ::Aero::MetadataPrivate::DependencyProperties(
                  schema.Metadata()),
              schema.Metadata()) {
        load.documentCache = &documents;
        load.dispatcher = &dispatcher;
        load.dependencyProperties =
            &::Aero::MetadataPrivate::DependencyProperties(
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
    GuiState& state = static_cast<GuiState&>(*gui_->state_);
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
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
    GuiState& state = static_cast<GuiState&>(*gui_->state_);
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
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
    const GuiState& state = static_cast<const GuiState&>(*gui_->state_);
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
    GuiState& state = static_cast<GuiState&>(*gui_->state_);
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
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
    GuiState& state = static_cast<GuiState&>(*gui_->state_);
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
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
    GuiState& state = static_cast<GuiState&>(*gui_->state_);
    XamlLoadScope scope(state.dispatcher, state.schema, state.documents);
    return state.xaml.LoadCompiled(
        state.xamlProviders, &scope.load, state.allocator,
        bytes, originUri, XamlReaderSettings{});
}

} // namespace Aero::Markup
