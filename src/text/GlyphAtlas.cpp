#include "GlyphAtlas.hpp"

#include <Aero/Base/HashMap.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Text {
namespace {

constexpr std::uint32_t SdfSpread = 8U;
constexpr float SdfDiagonalStep = 1.41421356237F;

// Deterministic two-pass chamfer transform. Keeping generation in the text
// layer avoids backend-specific font output and external image dependencies.
Base::Result<void> ConvertCoverageToSdf(
    GlyphBitmap& bitmap,
    Base::IAllocator* allocator) noexcept {
    if (bitmap.format == GlyphPixelFormat::Sdf8) return {};
    if (bitmap.format != GlyphPixelFormat::Gray8 || bitmap.width == 0U ||
        bitmap.height == 0U || bitmap.strideBytes < bitmap.width) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Glyph coverage bitmap is invalid for SDF conversion");
    }
    const std::uint64_t width64 =
        static_cast<std::uint64_t>(bitmap.width) + SdfSpread * 2U;
    const std::uint64_t height64 =
        static_cast<std::uint64_t>(bitmap.height) + SdfSpread * 2U;
    const std::uint64_t count64 = width64 * height64;
    if (width64 > UINT32_MAX || height64 > UINT32_MAX || count64 > UINT32_MAX) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "Glyph SDF dimensions exceed Aero container limits");
    }
    const std::uint32_t width = static_cast<std::uint32_t>(width64);
    const std::uint32_t height = static_cast<std::uint32_t>(height64);
    const std::uint32_t count = static_cast<std::uint32_t>(count64);
    Base::Vector<float> foreground(allocator);
    Base::Vector<float> background(allocator);
    Base::Vector<std::uint8_t> pixels(allocator);
    Base::Result<void> status = foreground.Resize(count);
    if (status) status = background.Resize(count);
    if (status) status = pixels.Resize(count);
    if (!status) return status.GetStatus();
    constexpr float Infinity = std::numeric_limits<float>::max() / 8.0F;
    auto at = [width](std::uint32_t x, std::uint32_t y) noexcept {
        return y * width + x;
    };
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const bool inside = x >= SdfSpread && y >= SdfSpread &&
                x - SdfSpread < bitmap.width && y - SdfSpread < bitmap.height &&
                bitmap.pixels[(y - SdfSpread) * bitmap.strideBytes +
                    (x - SdfSpread)] >= 128U;
            foreground[at(x, y)] = inside ? 0.0F : Infinity;
            background[at(x, y)] = inside ? Infinity : 0.0F;
        }
    }
    auto transform = [&](Base::Vector<float>& distances) noexcept {
        for (std::uint32_t y = 0U; y < height; ++y) for (std::uint32_t x = 0U; x < width; ++x) {
            float value = distances[at(x, y)];
            if (x > 0U) value = std::min(value, distances[at(x - 1U, y)] + 1.0F);
            if (y > 0U) value = std::min(value, distances[at(x, y - 1U)] + 1.0F);
            if (x > 0U && y > 0U) value = std::min(value, distances[at(x - 1U, y - 1U)] + SdfDiagonalStep);
            if (x + 1U < width && y > 0U) value = std::min(value, distances[at(x + 1U, y - 1U)] + SdfDiagonalStep);
            distances[at(x, y)] = value;
        }
        for (std::uint32_t y = height; y-- > 0U;) for (std::uint32_t x = width; x-- > 0U;) {
            float value = distances[at(x, y)];
            if (x + 1U < width) value = std::min(value, distances[at(x + 1U, y)] + 1.0F);
            if (y + 1U < height) value = std::min(value, distances[at(x, y + 1U)] + 1.0F);
            if (x + 1U < width && y + 1U < height) value = std::min(value, distances[at(x + 1U, y + 1U)] + SdfDiagonalStep);
            if (x > 0U && y + 1U < height) value = std::min(value, distances[at(x - 1U, y + 1U)] + SdfDiagonalStep);
            distances[at(x, y)] = value;
        }
    };
    transform(foreground);
    transform(background);
    for (std::uint32_t index = 0U; index < count; ++index) {
        const float signedDistance = background[index] - foreground[index];
        const float normalized = std::max(0.0F, std::min(1.0F,
            0.5F + signedDistance / (2.0F * static_cast<float>(SdfSpread))));
        pixels[index] = static_cast<std::uint8_t>(normalized * 255.0F + 0.5F);
    }
    bitmap.pixels = std::move(pixels);
    bitmap.format = GlyphPixelFormat::Sdf8;
    bitmap.width = width;
    bitmap.height = height;
    bitmap.strideBytes = width;
    bitmap.bearingX -= static_cast<std::int32_t>(SdfSpread);
    bitmap.bearingY += static_cast<std::int32_t>(SdfSpread);
    return {};
}

struct GlyphAtlasKeyHash  {
    Base::HashCode operator()(
        const GlyphAtlasKey& key,
        Base::HashCode seed = 0U) const noexcept {
        Base::HashCode hash = Base::MixHash64(
            key.face.provider.id ^ seed);
        hash = Base::MixHash64(
            hash ^ key.face.provider.version);
        hash = Base::MixHash64(hash ^ key.face.face);
        hash = Base::MixHash64(
            hash ^ key.face.generation);
        hash = Base::MixHash64(hash ^ key.glyph);
        hash = Base::MixHash64(
            hash ^ key.pixelSizeBits);
        return Base::MixHash64(
            hash ^ key.dpiScaleBits);
    }
};

std::uint32_t FloatBits(float value) noexcept {
    std::uint32_t bits = 0U;
    static_assert(sizeof(bits) == sizeof(value),
        "float key representation must be 32-bit");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

bool AddWouldOverflow(
    std::uint32_t left,
    std::uint32_t right) noexcept {
    return right > UINT32_MAX - left;
}

} // namespace

GlyphAtlasKey MakeGlyphAtlasKey(
    const GlyphRequest& request) noexcept {
    GlyphAtlasKey key;
    key.face = request.face;
    key.glyph = request.glyph;
    key.pixelSizeBits = FloatBits(request.pixelSize);
    key.dpiScaleBits = FloatBits(request.dpiScale);
    return key;
}

struct GlyphAtlas::Impl  {
    struct Page  {
        std::uint32_t generation = 1U;
        std::uint32_t cursorX = 0U;
        std::uint32_t cursorY = 0U;
        std::uint32_t rowHeight = 0U;
        std::uint64_t lastUse = 0U;
        GlyphAtlasFence lastSubmittedFence = 0U;
        bool hasPendingUploads = false;
    };

    struct Entry  {
        GlyphAtlasKey key;
        GlyphAtlasPlacement placement;
    };

    explicit Impl(Base::IAllocator* allocator) noexcept
        : pages(allocator),
          entries(allocator),
          entryIndex(allocator),
          uploads(allocator) {}

    GlyphAtlasConfig config;
    Base::Vector<Page> pages;
    Base::Vector<Entry> entries;
    Base::HashMap<
        GlyphAtlasKey, std::uint32_t,
        GlyphAtlasKeyHash> entryIndex;
    Base::Vector<GlyphAtlasUpload> uploads;
    std::uint32_t deviceGeneration = 1U;

    bool Place(
        Page& page,
        std::uint32_t contentWidth,
        std::uint32_t contentHeight,
        std::uint32_t& outputX,
        std::uint32_t& outputY) noexcept {
        const std::uint32_t padding = config.padding;
        const std::uint32_t paddedWidth =
            contentWidth + padding * 2U;
        const std::uint32_t paddedHeight =
            contentHeight + padding * 2U;
        if (paddedWidth > config.pageWidth - page.cursorX) {
            page.cursorX = 0U;
            page.cursorY += page.rowHeight;
            page.rowHeight = 0U;
        }
        if (paddedHeight > config.pageHeight - page.cursorY) {
            return false;
        }
        outputX = page.cursorX + padding;
        outputY = page.cursorY + padding;
        page.cursorX += paddedWidth;
        if (paddedHeight > page.rowHeight) {
            page.rowHeight = paddedHeight;
        }
        return true;
    }

    void RemoveEntriesForPage(
        std::uint32_t pageIndex) noexcept {
        std::uint32_t index = 0U;
        while (index < entries.Size()) {
            if (entries[index].placement.page != pageIndex) {
                ++index;
                continue;
            }
            static_cast<void>(
                entryIndex.Erase(entries[index].key));
            const std::uint32_t last = entries.Size() - 1U;
            if (index != last) {
                entries[index] = std::move(entries[last]);
                std::uint32_t* moved =
                    entryIndex.Find(entries[index].key);
                if (moved != nullptr) *moved = index;
            }
            entries.PopBack();
        }
    }

    void ResetPage(std::uint32_t pageIndex) noexcept {
        RemoveEntriesForPage(pageIndex);
        Page& page = pages[pageIndex];
        ++page.generation;
        if (page.generation == 0U) page.generation = 1U;
        page.cursorX = 0U;
        page.cursorY = 0U;
        page.rowHeight = 0U;
        page.lastUse = 0U;
        page.lastSubmittedFence = 0U;
        page.hasPendingUploads = false;
    }
};

GlyphAtlas::GlyphAtlas(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator : &Base::GetDefaultAllocator()) {}

GlyphAtlas::~GlyphAtlas() {
    Shutdown();
}

Base::Result<void> GlyphAtlas::Initialize(
    const GlyphAtlasConfig& config) noexcept {
    if (impl_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Glyph atlas is already initialized");
    }
    if (config.pageWidth == 0U ||
        config.pageHeight == 0U ||
        config.maxPages == 0U ||
        AddWouldOverflow(config.padding, config.padding) ||
        config.padding * 2U >= config.pageWidth ||
        config.padding * 2U >= config.pageHeight) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Glyph atlas configuration is invalid");
    }
    void* memory = allocator_->Allocate(
        {sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Glyph atlas allocation failed");
    }
    impl_ = new (memory) Impl(allocator_);
    impl_->config = config;
    return {};
}

void GlyphAtlas::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool GlyphAtlas::IsInitialized() const noexcept {
    return impl_ != nullptr;
}

Base::Result<void> GlyphAtlas::EnsureGlyph(
    FontManager& fonts,
    const GlyphRequest& request,
    std::uint64_t useStamp,
    GlyphAtlasFence completedFence,
    GlyphAtlasPlacement& output) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Glyph atlas is not initialized");
    }
    const GlyphAtlasKey key = MakeGlyphAtlasKey(request);
    std::uint32_t* existing = impl_->entryIndex.Find(key);
    if (existing != nullptr) {
        Impl::Entry& entry = impl_->entries[*existing];
        impl_->pages[entry.placement.page].lastUse = useStamp;
        output = entry.placement;
        return {};
    }

    GlyphBitmap bitmap(allocator_);
    Base::Result<void> rasterized =
        fonts.RasterizeGlyph(request, bitmap);
    if (!rasterized) return rasterized.GetStatus();
    Base::Result<void> sdf = ConvertCoverageToSdf(bitmap, allocator_);
    if (!sdf) return sdf.GetStatus();
    GlyphMetrics metrics;
    Base::Result<void> measured =
        fonts.GetGlyphMetrics(request, metrics);
    if (!measured) return measured.GetStatus();
    if (bitmap.width == 0U || bitmap.height == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Glyph atlas cannot place an empty bitmap");
    }

    const std::uint32_t padding = impl_->config.padding;
    if (AddWouldOverflow(bitmap.width, padding * 2U) ||
        AddWouldOverflow(bitmap.height, padding * 2U) ||
        bitmap.width + padding * 2U > impl_->config.pageWidth ||
        bitmap.height + padding * 2U > impl_->config.pageHeight) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Glyph bitmap exceeds atlas page dimensions");
    }

    std::uint32_t pageIndex = UINT32_MAX;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    for (std::uint32_t index = 0U;
         index < impl_->pages.Size(); ++index) {
        if (impl_->Place(
                impl_->pages[index],
                bitmap.width, bitmap.height, x, y)) {
            pageIndex = index;
            break;
        }
    }
    if (pageIndex == UINT32_MAX &&
        impl_->pages.Size() < impl_->config.maxPages) {
        Base::Result<Impl::Page*> appended =
            impl_->pages.EmplaceBack();
        if (!appended) return appended.GetStatus();
        pageIndex = impl_->pages.Size() - 1U;
        if (!impl_->Place(
                impl_->pages[pageIndex],
                bitmap.width, bitmap.height, x, y)) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Fresh glyph atlas page rejected a fitting bitmap");
        }
    }
    if (pageIndex == UINT32_MAX) {
        std::uint32_t victim = UINT32_MAX;
        std::uint64_t oldestUse = UINT64_MAX;
        for (std::uint32_t index = 0U;
             index < impl_->pages.Size(); ++index) {
            const Impl::Page& page = impl_->pages[index];
            if (page.hasPendingUploads ||
                page.lastSubmittedFence > completedFence ||
                page.lastUse >= oldestUse) {
                continue;
            }
            victim = index;
            oldestUse = page.lastUse;
        }
        if (victim == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "Glyph atlas pages are full or still referenced by GPU work");
        }
        impl_->ResetPage(victim);
        pageIndex = victim;
        if (!impl_->Place(
                impl_->pages[pageIndex],
                bitmap.width, bitmap.height, x, y)) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "Reset glyph atlas page rejected a fitting bitmap");
        }
    }

    Impl::Page& page = impl_->pages[pageIndex];
    page.lastUse = useStamp;
    GlyphAtlasPlacement placement;
    placement.key = key;
    placement.deviceGeneration = impl_->deviceGeneration;
    placement.page = pageIndex;
    placement.pageGeneration = page.generation;
    placement.x = x;
    placement.y = y;
    placement.width = bitmap.width;
    placement.height = bitmap.height;
    placement.bearingX = bitmap.bearingX;
    placement.bearingY = bitmap.bearingY;
    placement.advanceX = metrics.advanceX;

    Impl::Entry entry;
    entry.key = key;
    entry.placement = placement;
    Base::Result<Impl::Entry*> stored =
        impl_->entries.EmplaceBack(entry);
    if (!stored) return stored.GetStatus();
    const std::uint32_t entryIndex =
        impl_->entries.Size() - 1U;
    Base::Result<
        Base::HashMap<
            GlyphAtlasKey, std::uint32_t,
            GlyphAtlasKeyHash>::InsertResult> indexed =
        impl_->entryIndex.Insert(key, entryIndex);
    if (!indexed) {
        impl_->entries.PopBack();
        return indexed.GetStatus();
    }
    if (!indexed.Value().inserted) {
        impl_->entries.PopBack();
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Glyph atlas cache key was inserted concurrently");
    }

    GlyphAtlasUpload upload(allocator_);
    upload.deviceGeneration = impl_->deviceGeneration;
    upload.page = pageIndex;
    upload.pageGeneration = page.generation;
    upload.x = x;
    upload.y = y;
    upload.width = bitmap.width;
    upload.height = bitmap.height;
    upload.strideBytes = bitmap.width;
    const std::uint64_t byteCount =
        static_cast<std::uint64_t>(bitmap.width) *
        static_cast<std::uint64_t>(bitmap.height);
    Base::Result<void> resized = upload.pixels.Resize(
        static_cast<std::uint32_t>(byteCount));
    if (!resized) {
        static_cast<void>(impl_->entryIndex.Erase(key));
        impl_->entries.PopBack();
        return resized.GetStatus();
    }
    for (std::uint32_t row = 0U;
         row < bitmap.height; ++row) {
        std::memcpy(
            upload.pixels.Data() + row * bitmap.width,
            bitmap.pixels.Data() + row * bitmap.strideBytes,
            bitmap.width);
    }
    Base::Result<GlyphAtlasUpload*> queued =
        impl_->uploads.EmplaceBack(std::move(upload));
    if (!queued) {
        static_cast<void>(impl_->entryIndex.Erase(key));
        impl_->entries.PopBack();
        return queued.GetStatus();
    }
    page.hasPendingUploads = true;
    output = placement;
    return {};
}

Base::Result<void> GlyphAtlas::MarkSubmitted(
    const GlyphAtlasPlacement& placement,
    GlyphAtlasFence fence) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Glyph atlas is not initialized");
    }
    if (fence == 0U || !IsValid(placement)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Glyph atlas submission references a stale placement");
    }
    Impl::Page& page = impl_->pages[placement.page];
    if (fence > page.lastSubmittedFence) {
        page.lastSubmittedFence = fence;
    }
    return {};
}

bool GlyphAtlas::IsValid(
    const GlyphAtlasPlacement& placement) const noexcept {
    if (impl_ == nullptr ||
        placement.deviceGeneration != impl_->deviceGeneration ||
        placement.page >= impl_->pages.Size() ||
        placement.pageGeneration !=
            impl_->pages[placement.page].generation) {
        return false;
    }
    const std::uint32_t* index =
        impl_->entryIndex.Find(placement.key);
    if (index == nullptr) return false;
    const GlyphAtlasPlacement& stored =
        impl_->entries[*index].placement;
    return stored.deviceGeneration == placement.deviceGeneration &&
        stored.page == placement.page &&
        stored.pageGeneration == placement.pageGeneration &&
        stored.x == placement.x &&
        stored.y == placement.y &&
        stored.width == placement.width &&
        stored.height == placement.height;
}

Base::Span<const GlyphAtlasUpload>
GlyphAtlas::PendingUploads() const noexcept {
    return impl_ != nullptr
        ? impl_->uploads.AsSpan()
        : Base::Span<const GlyphAtlasUpload>();
}

void GlyphAtlas::ClearPendingUploads() noexcept {
    if (impl_ == nullptr) return;
    impl_->uploads.Clear();
    for (Impl::Page& page : impl_->pages) {
        page.hasPendingUploads = false;
    }
}

void GlyphAtlas::NotifyDeviceLost() noexcept {
    if (impl_ == nullptr) return;
    impl_->entries.Clear();
    impl_->entryIndex.Clear();
    impl_->uploads.Clear();
    ++impl_->deviceGeneration;
    if (impl_->deviceGeneration == 0U) {
        impl_->deviceGeneration = 1U;
    }
    for (std::uint32_t index = 0U;
         index < impl_->pages.Size(); ++index) {
        Impl::Page& page = impl_->pages[index];
        ++page.generation;
        if (page.generation == 0U) page.generation = 1U;
        page.cursorX = 0U;
        page.cursorY = 0U;
        page.rowHeight = 0U;
        page.lastUse = 0U;
        page.lastSubmittedFence = 0U;
        page.hasPendingUploads = false;
    }
}

std::uint32_t GlyphAtlas::PageCount() const noexcept {
    return impl_ != nullptr ? impl_->pages.Size() : 0U;
}

std::uint32_t GlyphAtlas::EntryCount() const noexcept {
    return impl_ != nullptr ? impl_->entries.Size() : 0U;
}

std::uint32_t GlyphAtlas::DeviceGeneration() const noexcept {
    return impl_ != nullptr ? impl_->deviceGeneration : 0U;
}

GlyphAtlasConfig GlyphAtlas::Config() const noexcept {
    return impl_ != nullptr ? impl_->config : GlyphAtlasConfig{};
}

} // namespace Aero::Text
