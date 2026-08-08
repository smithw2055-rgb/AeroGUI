#pragma once

#include <Aero/Gui/View.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Gui/ResourceDictionary.hpp>
#include <Aero/Gui/XamlReader.hpp>

namespace Aero {

// Source-only bridge for Gui, XamlReader, and DesktopHost. The installed View
// declaration remains the Noesis-style host API while these operations stay
// with the View composition root.
struct View::Operations {
    struct FrameResult;

    static Base::Result<void> Initialize(
        View& view, const ViewOptions& options) noexcept;
    static void Shutdown(View& view) noexcept;
    static bool IsInitialized(const View& view) noexcept;
    static bool IsMounted(const View& view) noexcept;

    static Base::Result<void> LoadResources(
        View& view, ResourceLayer layer, Base::StringView uri,
        ResourceLoadMode mode = ResourceLoadMode::Replace,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    static Base::Result<void> LoadCompiledResources(
        View& view, ResourceLayer layer,
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept;
    static void SetResourceDictionary(
        View& view, ResourceLayer layer,
        Aero::ResourceDictionary& dictionary,
        ResourceLoadMode mode) noexcept;
    static Base::Result<void> LoadBuiltInTheme(
        View& view, BuiltInTheme theme) noexcept;

    static Base::Result<void> Mount(
        View& view, Aero::Size availableSize) noexcept;
    static Base::Result<void> Mount(
        View& view, Base::Ref<Base::Object> root,
        Aero::Size availableSize) noexcept;
    static Base::Result<void> Mount(
        View& view, Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    static Base::Result<void> ReplaceMountedDocument(
        View& view, Markup::XamlDocument&& document,
        Aero::Size availableSize) noexcept;
    static Base::Result<void> Unmount(View& view) noexcept;
    static Base::Result<void> MountContent(
        View& view, Controls::ContentControl& host,
        Markup::XamlDocument&& document) noexcept;
    static Base::Result<void> UnmountContent(
        View& view, Controls::ContentControl& host) noexcept;

    static Base::Result<FrameResult> ExecuteFrame(View& view) noexcept;
    static Base::Result<std::uint32_t> AdvanceClocks(
        View& view, std::uint32_t elapsedMilliseconds) noexcept;
    static Base::Result<std::uint32_t> AdvanceAnimations(
        View& view, std::uint32_t elapsedMilliseconds) noexcept;

    static Base::Object* FindNamedObject(
        View& view, Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept;
    static std::uint32_t NamedObjectCount(const View& view) noexcept;
    static bool IsInstanceOf(
        const View& view, const Base::Object& object,
        Meta::TypeId baseType) noexcept;
};

} // namespace Aero
