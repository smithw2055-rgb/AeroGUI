#pragma once

#include "Providers.hpp"

namespace Aero::Text {

class HarfBuzzAdapter;

// Owns FreeType library/face state. Instances and loaded faces are confined to
// the thread on which the adapter is used.
class AERO_API FreeTypeAdapter
    : public IFontProvider,
      public ITextShaper,
      public IGlyphRasterizer {
public:
    explicit FreeTypeAdapter(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~FreeTypeAdapter() override;

    FreeTypeAdapter(const FreeTypeAdapter&) = delete;
    FreeTypeAdapter& operator=(const FreeTypeAdapter&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    FontProviderIdentity Identity() const noexcept override;
    Base::Result<void> LoadFace(
        const FontSource& source,
        const Typeface& typeface,
        FontFace& output) noexcept override;
    Base::Result<void> ResolveFace(
        const FontQuery& query,
        FontFace& output) noexcept override;
    Base::Result<bool> HasCodePoint(
        FontFaceHandle face,
        std::uint32_t codePoint) noexcept override;
    void ReleaseFace(FontFaceHandle face) noexcept override;

    bool Supports(
        FontProviderIdentity provider) const noexcept override;
    Base::Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output) noexcept override;

    Base::Result<void> GetMetrics(
        const GlyphRequest& request,
        GlyphMetrics& output) noexcept override;
    Base::Result<void> Rasterize(
        const GlyphRequest& request,
        GlyphBitmap& output) noexcept override;
    Base::Result<void> ExtractOutline(
        const GlyphRequest& request,
        GlyphOutline& output) noexcept override;

private:
    friend class HarfBuzzAdapter;

    struct Impl;

    void* FindNativeFace(FontFaceHandle face) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Text
