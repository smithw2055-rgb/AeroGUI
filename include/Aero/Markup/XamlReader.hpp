#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Markup/XamlDocument.hpp>

#include <cstdint>

namespace Aero {
class View;
namespace Core { class IDiagnosticSink; }
namespace Integration { class ISourceProvider; }
}

namespace Aero::Markup {

// WPF-shaped XAML loading facade. A reader is bound to one View so loaded
// objects use the same frozen schema, allocator and source-provider set as the
// visual tree that will consume them. View itself remains focused on update,
// input, layout and rendering.
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

    Aero::View& GetView() const noexcept { return *view_; }

private:
    Aero::View* view_ = nullptr;
};

} // namespace Aero::Markup
