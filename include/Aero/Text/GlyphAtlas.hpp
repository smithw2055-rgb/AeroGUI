#pragma once

#include <Aero/Text/FontManager.hpp>

namespace Aero::Text {

using GlyphAtlasFence = std::uint64_t;

struct GlyphAtlasConfig final {
    std::uint32_t pageWidth = 1024U;
    std::uint32_t pageHeight = 1024U;
    std::uint32_t maxPages = 4U;
    std::uint32_t padding = 1U;
};

struct GlyphAtlasKey final {
    FontFaceHandle face;
    GlyphId glyph = InvalidGlyphId;
    std::uint32_t pixelSizeBits = 0U;
    std::uint32_t dpiScaleBits = 0U;
};

constexpr bool operator==(
    const GlyphAtlasKey& left,
    const GlyphAtlasKey& right) noexcept {
    return left.face == right.face &&
        left.glyph == right.glyph &&
        left.pixelSizeBits == right.pixelSizeBits &&
        left.dpiScaleBits == right.dpiScaleBits;
}

constexpr bool operator!=(
    const GlyphAtlasKey& left,
    const GlyphAtlasKey& right) noexcept {
    return !(left == right);
}

AERO_API GlyphAtlasKey MakeGlyphAtlasKey(
    const GlyphRequest& request) noexcept;

struct GlyphAtlasPlacement final {
    GlyphAtlasKey key;
    std::uint32_t deviceGeneration = 0U;
    std::uint32_t page = 0U;
    std::uint32_t pageGeneration = 0U;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::int32_t bearingX = 0;
    std::int32_t bearingY = 0;
    float advanceX = 0.0F;
};

struct GlyphAtlasUpload final {
    explicit GlyphAtlasUpload(
        Base::IAllocator* allocator = nullptr) noexcept
        : pixels(allocator) {}

    std::uint32_t deviceGeneration = 0U;
    std::uint32_t page = 0U;
    std::uint32_t pageGeneration = 0U;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t strideBytes = 0U;
    Base::Vector<std::uint8_t> pixels;
};

class AERO_API GlyphAtlas final {
public:
    explicit GlyphAtlas(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~GlyphAtlas();

    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    Base::Result<void> Initialize(
        const GlyphAtlasConfig& config) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> EnsureGlyph(
        FontManager& fonts,
        const GlyphRequest& request,
        std::uint64_t useStamp,
        GlyphAtlasFence completedFence,
        GlyphAtlasPlacement& output) noexcept;
    Base::Result<void> MarkSubmitted(
        const GlyphAtlasPlacement& placement,
        GlyphAtlasFence fence) noexcept;
    bool IsValid(
        const GlyphAtlasPlacement& placement) const noexcept;

    Base::Span<const GlyphAtlasUpload>
    PendingUploads() const noexcept;
    void ClearPendingUploads() noexcept;
    void NotifyDeviceLost() noexcept;

    std::uint32_t PageCount() const noexcept;
    std::uint32_t EntryCount() const noexcept;
    std::uint32_t DeviceGeneration() const noexcept;
    GlyphAtlasConfig Config() const noexcept;

private:
    struct Impl;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Text
