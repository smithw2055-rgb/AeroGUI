#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Markup/XamlDocument.hpp>

#include <cstdint>

namespace Aero {
class ResourceDictionary;
class View;
enum class BuiltInTheme : std::uint8_t;
enum class ResourceLayer : std::uint8_t;
enum class ResourceLoadMode : std::uint8_t;
namespace Core { class IDiagnosticSink; }
namespace Controls { class ContentControl; }
namespace Integration { class ISourceProvider; }
}

namespace Aero::Markup {

// XAML and resource facade bound to a View. A reader uses the View's frozen
// schema, allocator and source providers while keeping loading concerns out of
// the frame/input/render API.
class AERO_API XamlReader final {
public:
    explicit XamlReader(Aero::View& view) noexcept : view_(&view) {}

    Base::Result<UiDocument> Load(
        Base::StringView uri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> Parse(
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri = {}) noexcept;
    Base::Result<void> RegisterSourceProvider(
        Integration::ISourceProvider& provider,
        Base::StringView scheme = {},
        Base::StringView assembly = {}) noexcept;

    Base::Result<void> Mount(
        Controls::ContentControl& host,
        UiDocument&& document) noexcept;
    Base::Result<void> Unmount(
        Controls::ContentControl& host) noexcept;
    Base::Result<void> LoadResources(
        ResourceLayer layer,
        Base::StringView uri,
        ResourceLoadMode mode,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> LoadCompiledResources(
        ResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceLoadMode mode) noexcept;
    Base::Result<void> SetResources(
        ResourceLayer layer,
        Aero::ResourceDictionary& dictionary,
        ResourceLoadMode mode) noexcept;
    Base::Result<void> LoadTheme(BuiltInTheme theme) noexcept;

    Aero::View& GetView() const noexcept { return *view_; }

private:
    Aero::View* view_ = nullptr;
};

} // namespace Aero::Markup
