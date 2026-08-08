#pragma once

#include "Providers.hpp"

namespace Aero::Text {

class FontManager  {
public:
    explicit FontManager(
        Base::IAllocator* allocator = nullptr) noexcept
        : registrations_(allocator) {}

    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<void> RegisterProvider(
        const TextProviderRegistration& registration) noexcept;
    Base::Result<void> UnregisterProvider(
        FontProviderId provider) noexcept;

    Base::Result<void> LoadFace(
        FontProviderId provider,
        const FontSource& source,
        const Typeface& typeface,
        FontFace& output) noexcept;
    Base::Result<void> ResolveFace(
        const FontQuery& query,
        FontFace& output) noexcept;
    Base::Result<bool> HasCodePoint(
        FontFaceHandle face,
        std::uint32_t codePoint) noexcept;
    Base::Result<void> ReleaseFace(FontFaceHandle face) noexcept;

    Base::Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output) noexcept;
    Base::Result<void> GetGlyphMetrics(
        const GlyphRequest& request,
        GlyphMetrics& output) noexcept;
    Base::Result<void> RasterizeGlyph(
        const GlyphRequest& request,
        GlyphBitmap& output) noexcept;
    Base::Result<void> ExtractGlyphOutline(
        const GlyphRequest& request,
        GlyphOutline& output) noexcept;

    std::uint32_t ProviderCount() const noexcept {
        return registrations_.Size();
    }
    bool IsInitialized() const noexcept { return initialized_; }

private:
    struct ProviderRecord  {
        FontProviderIdentity identity;
        IFontProvider* fonts = nullptr;
        ITextShaper* shaper = nullptr;
        IGlyphRasterizer* rasterizer = nullptr;
    };

    Base::Vector<ProviderRecord> registrations_;
    bool initialized_ = false;

    ProviderRecord* FindProvider(FontProviderId provider) noexcept;
    const ProviderRecord* FindProvider(
        FontProviderId provider) const noexcept;
    ProviderRecord* FindProvider(FontFaceHandle face) noexcept;

    Base::Result<void> VerifyReady() const noexcept;
    static Base::Result<void> ValidateFace(
        const FontFace& face,
        FontProviderIdentity expected) noexcept;
};

} // namespace Aero::Text
