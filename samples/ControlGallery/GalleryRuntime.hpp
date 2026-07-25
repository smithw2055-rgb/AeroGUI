#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <cstdint>
#include <memory>

namespace Aero::Samples::ControlGallery {

enum class GalleryLoadMode : std::uint8_t {
    Runtime = 0U,
    Compiled,
};

enum class GalleryTheme : std::uint8_t {
    Light = 0U,
    Dark,
};

struct GallerySnapshot final {
    std::uint64_t planHash = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t namedObjectCount = 0U;
    std::uint32_t itemCount = 0U;
    std::uint32_t realizedItemCount = 0U;
    std::uint32_t createdContainerCount = 0U;
    GalleryLoadMode loadMode = GalleryLoadMode::Runtime;
    GalleryTheme theme = GalleryTheme::Light;
};

class GalleryRuntime final {
public:
    GalleryRuntime() noexcept;
    ~GalleryRuntime();

    GalleryRuntime(const GalleryRuntime&) = delete;
    GalleryRuntime& operator=(const GalleryRuntime&) = delete;

    Base::Result<void> Initialize(
        Base::StringView assetDirectory,
        GalleryLoadMode loadMode,
        GalleryTheme theme) noexcept;
    void Shutdown() noexcept;

    const GallerySnapshot& Snapshot() const noexcept;
    const Presentation::RenderPlan& Plan() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

Base::Result<void> RunNativeGallery(
    const Presentation::RenderPlan& plan,
    Base::StringView backend,
    bool simulateContextLoss,
    bool interactive) noexcept;

} // namespace Aero::Samples::ControlGallery
