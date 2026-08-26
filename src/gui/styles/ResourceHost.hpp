#pragma once

// Source-only resource-layer host next to StyleEngine / Resources.cpp.
// Not installed under include/Aero. Included from ViewState.hpp after ViewState.

#include <Aero/Resources.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/Markup/XamlDocument.hpp>
#include <Aero/Diagnostics.hpp>

namespace Aero {

class ResourceHost {
public:
    explicit ResourceHost(ViewState& owner) noexcept;
    void Bind() noexcept;

    ViewState* view = nullptr;

    ResourceDictionary applicationResources;
    ResourceDictionary themeResources;
    ResourceDictionary systemResources;
    ResourceDictionary dynamicResourceEnvironment;

    ResourceEnvironment Environment() const noexcept;
    Base::Result<ResourceDictionary*> ResolveLayer(
        ResourceLayer layer) noexcept;
    Base::Result<void> RebuildDynamicEnvironment() noexcept;

    Base::Result<void> CommitLayer(
        Markup::XamlDocument document,
        ResourceDictionary& target,
        bool merge) noexcept;
    Base::Result<void> LoadLayer(
        Base::StringView uri,
        ResourceDictionary& target,
        Diagnostics::IDiagnosticSink* diagnostics,
        bool merge = false) noexcept;
    Base::Result<void> LoadCompiledLayer(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceDictionary& target,
        bool merge = false) noexcept;
};

} // namespace Aero
