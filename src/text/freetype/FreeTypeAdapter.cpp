#include <Aero/Text/FreeTypeAdapter.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero::Text {
namespace {

constexpr FontProviderIdentity AdapterIdentity{
    0x4654484254455854ULL, 1U};

float From26Dot6(FT_Pos value) noexcept {
    return static_cast<float>(value) / 64.0F;
}

Base::Status FreeTypeFailure(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed, message);
}

std::uint32_t DecodeUtf8CodePoint(
    Base::StringView text,
    std::uint32_t& offset) noexcept {
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(text.Data());
    const unsigned char lead = bytes[offset++];
    if (lead <= 0x7FU) return lead;
    std::uint32_t codePoint = 0U;
    std::uint32_t remaining = 0U;
    if ((lead & 0xE0U) == 0xC0U) {
        codePoint = lead & 0x1FU;
        remaining = 1U;
    } else if ((lead & 0xF0U) == 0xE0U) {
        codePoint = lead & 0x0FU;
        remaining = 2U;
    } else {
        codePoint = lead & 0x07U;
        remaining = 3U;
    }
    while (remaining-- > 0U) {
        codePoint = (codePoint << 6U) |
            static_cast<std::uint32_t>(bytes[offset++] & 0x3FU);
    }
    return codePoint;
}

Base::Result<FT_UInt> ToPixelSize(
    float pixelSize,
    float dpiScale = 1.0F) noexcept {
    const double scaled =
        static_cast<double>(pixelSize) *
        static_cast<double>(dpiScale);
    if (!std::isfinite(scaled) ||
        scaled > static_cast<double>(
            std::numeric_limits<FT_UInt>::max())) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Requested pixel size exceeds FreeType limits");
    }
    return static_cast<FT_UInt>(std::ceil(scaled));
}

struct OutlineBuilder final {
    GlyphOutline* outline = nullptr;
    Base::Status status;
    bool contourOpen = false;
};

bool AppendOutline(
    OutlineBuilder& builder,
    const OutlineCommand& command) noexcept {
    Base::Result<void> appended =
        builder.outline->commands.TryPushBack(command);
    if (!appended) {
        builder.status = appended.GetStatus();
        return false;
    }
    return true;
}

bool CloseContour(OutlineBuilder& builder) noexcept {
    if (!builder.contourOpen) return true;
    OutlineCommand close;
    close.kind = OutlineCommandKind::Close;
    close.pointCount = 0U;
    if (!AppendOutline(builder, close)) return false;
    builder.contourOpen = false;
    return true;
}

int MoveToCallback(
    const FT_Vector* to,
    void* user) {
    auto& builder = *static_cast<OutlineBuilder*>(user);
    if (!CloseContour(builder)) return 1;
    OutlineCommand command;
    command.kind = OutlineCommandKind::MoveTo;
    command.pointCount = 1U;
    command.points[0] = {From26Dot6(to->x), From26Dot6(to->y)};
    if (!AppendOutline(builder, command)) return 1;
    builder.contourOpen = true;
    return 0;
}

int LineToCallback(
    const FT_Vector* to,
    void* user) {
    auto& builder = *static_cast<OutlineBuilder*>(user);
    OutlineCommand command;
    command.kind = OutlineCommandKind::LineTo;
    command.pointCount = 1U;
    command.points[0] = {From26Dot6(to->x), From26Dot6(to->y)};
    return AppendOutline(builder, command) ? 0 : 1;
}

int ConicToCallback(
    const FT_Vector* control,
    const FT_Vector* to,
    void* user) {
    auto& builder = *static_cast<OutlineBuilder*>(user);
    OutlineCommand command;
    command.kind = OutlineCommandKind::QuadraticTo;
    command.pointCount = 2U;
    command.points[0] = {
        From26Dot6(control->x), From26Dot6(control->y)};
    command.points[1] = {From26Dot6(to->x), From26Dot6(to->y)};
    return AppendOutline(builder, command) ? 0 : 1;
}

int CubicToCallback(
    const FT_Vector* first,
    const FT_Vector* second,
    const FT_Vector* to,
    void* user) {
    auto& builder = *static_cast<OutlineBuilder*>(user);
    OutlineCommand command;
    command.kind = OutlineCommandKind::CubicTo;
    command.pointCount = 3U;
    command.points[0] = {
        From26Dot6(first->x), From26Dot6(first->y)};
    command.points[1] = {
        From26Dot6(second->x), From26Dot6(second->y)};
    command.points[2] = {From26Dot6(to->x), From26Dot6(to->y)};
    return AppendOutline(builder, command) ? 0 : 1;
}

} // namespace

struct FreeTypeAdapter::Impl final {
    struct FaceRecord final {
        explicit FaceRecord(
            Base::IAllocator* allocator = nullptr) noexcept
            : family(allocator), sourceName(allocator), sourceBytes(allocator) {}

        FontFaceId id = InvalidFontFaceId;
        std::uint32_t generation = 1U;
        std::uint32_t referenceCount = 1U;
        std::uint32_t faceIndex = 0U;
        FontSourceKind sourceKind = FontSourceKind::File;
        std::uint16_t weight = 400U;
        FontStyle style = FontStyle::Normal;
        FontStretch stretch = FontStretch::Normal;
        Base::String family;
        Base::String sourceName;
        Base::Vector<std::uint8_t> sourceBytes;
        FT_Face freeTypeFace = nullptr;
    };

    explicit Impl(Base::IAllocator* allocator) noexcept
        : faces(allocator) {}

    FT_Library library = nullptr;
    Base::Vector<FaceRecord> faces;
    FontFaceId nextFace = 1U;

    static void Describe(
        const FaceRecord& record,
        FontFace& output) noexcept {
        output.handle = {
            AdapterIdentity, record.id, record.generation};
        output.metrics.unitsPerEm =
            static_cast<float>(record.freeTypeFace->units_per_EM);
        output.metrics.ascent =
            static_cast<float>(record.freeTypeFace->ascender);
        output.metrics.descent =
            static_cast<float>(record.freeTypeFace->descender);
        output.metrics.lineGap = static_cast<float>(
            record.freeTypeFace->height -
            record.freeTypeFace->ascender +
            record.freeTypeFace->descender);
        output.metrics.underlinePosition =
            static_cast<float>(
                record.freeTypeFace->underline_position);
        output.metrics.underlineThickness =
            static_cast<float>(
                record.freeTypeFace->underline_thickness);
        output.hasColorGlyphs =
            FT_HAS_COLOR(record.freeTypeFace) != 0;
    }

    static bool Matches(
        const FaceRecord& record,
        const FontSource& source,
        const Typeface& typeface) noexcept {
        if (record.sourceKind != source.kind ||
            record.faceIndex != source.faceIndex ||
            record.family.View() != typeface.Family() ||
            record.weight != typeface.Weight() ||
            record.style != typeface.Style() ||
            record.stretch != typeface.Stretch() ||
            record.sourceName.View() != source.identifier) {
            return false;
        }
        return source.kind != FontSourceKind::Memory ||
            (record.sourceBytes.Size() == source.bytes.Size() &&
             (source.bytes.Empty() ||
              std::memcmp(
                  record.sourceBytes.Data(),
                  source.bytes.Data(),
                  source.bytes.Size()) == 0));
    }

    FaceRecord* Find(FontFaceHandle handle) noexcept {
        if (handle.provider != AdapterIdentity) return nullptr;
        for (FaceRecord& face : faces) {
            if (face.id == handle.face &&
                face.generation == handle.generation) {
                return &face;
            }
        }
        return nullptr;
    }
};

FreeTypeAdapter::FreeTypeAdapter(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator : &Base::GetDefaultAllocator()) {}

FreeTypeAdapter::~FreeTypeAdapter() {
    Shutdown();
}

Base::Result<void>
FreeTypeAdapter::Initialize() noexcept {
    if (impl_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "FreeType adapter is already initialized");
    }
    void* memory = allocator_->Allocate(
        {sizeof(Impl), alignof(Impl), Base::MemoryTag::General});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "FreeType adapter allocation failed");
    }
    impl_ = new (memory) Impl(allocator_);
    if (FT_Init_FreeType(&impl_->library) != 0) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl),
            Base::MemoryTag::General);
        impl_ = nullptr;
        return FreeTypeFailure("FreeType initialization failed");
    }
    return {};
}

void FreeTypeAdapter::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    for (Impl::FaceRecord& face : impl_->faces) {
        if (face.freeTypeFace != nullptr) {
            FT_Done_Face(face.freeTypeFace);
        }
    }
    impl_->faces.Clear();
    if (impl_->library != nullptr) {
        FT_Done_FreeType(impl_->library);
    }
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl),
        Base::MemoryTag::General);
    impl_ = nullptr;
}

bool FreeTypeAdapter::IsInitialized() const noexcept {
    return impl_ != nullptr;
}

FontProviderIdentity
FreeTypeAdapter::Identity() const noexcept {
    return AdapterIdentity;
}

Base::Result<void> FreeTypeAdapter::LoadFace(
    const FontSource& source,
    const Typeface& typeface,
    FontFace& output) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FreeType adapter is not initialized");
    }
    if (typeface.Family().Empty() ||
        source.identifier.Empty() ||
        (source.kind != FontSourceKind::File &&
         source.kind != FontSourceKind::Memory) ||
        (source.kind == FontSourceKind::Memory &&
         source.bytes.Empty())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FreeType font source or typeface is incomplete");
    }
    if (static_cast<std::uint64_t>(source.faceIndex) >
            static_cast<std::uint64_t>(
                std::numeric_limits<FT_Long>::max()) ||
        static_cast<std::uint64_t>(source.bytes.Size()) >
            static_cast<std::uint64_t>(
                std::numeric_limits<FT_Long>::max())) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "FreeType font source exceeds signed size limits");
    }
    output = {};
    for (Impl::FaceRecord& cached : impl_->faces) {
        if (!Impl::Matches(cached, source, typeface)) continue;
        if (cached.referenceCount == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "FreeType face reference count reached its limit");
        }
        ++cached.referenceCount;
        Impl::Describe(cached, output);
        return {};
    }
    Base::Result<Impl::FaceRecord*> appended =
        impl_->faces.TryEmplaceBack(allocator_);
    if (!appended) return appended.GetStatus();
    Impl::FaceRecord& record = *appended.Value();
    record.faceIndex = source.faceIndex;
    record.sourceKind = source.kind;
    record.weight = typeface.Weight();
    record.style = typeface.Style();
    record.stretch = typeface.Stretch();
    Base::Result<void> assigned =
        record.family.TryAssign(typeface.Family());
    if (assigned) assigned = record.sourceName.TryAssign(source.identifier);
    if (assigned && source.kind == FontSourceKind::Memory) {
        assigned = record.sourceBytes.TryAppend(source.bytes);
    }
    if (!assigned) {
        impl_->faces.PopBack();
        return assigned.GetStatus();
    }

    FT_Error error = 0;
    if (source.kind == FontSourceKind::Memory) {
        error = FT_New_Memory_Face(
            impl_->library,
            record.sourceBytes.Data(),
            static_cast<FT_Long>(record.sourceBytes.Size()),
            static_cast<FT_Long>(source.faceIndex),
            &record.freeTypeFace);
    } else {
        error = FT_New_Face(
            impl_->library,
            record.sourceName.CStr(),
            static_cast<FT_Long>(source.faceIndex),
            &record.freeTypeFace);
    }
    if (error != 0) {
        impl_->faces.PopBack();
        return FreeTypeFailure("FreeType could not load the font face");
    }
    (void)FT_Select_Charmap(record.freeTypeFace, FT_ENCODING_UNICODE);
    record.id = impl_->nextFace++;
    Impl::Describe(record, output);
    return {};
}

Base::Result<void> FreeTypeAdapter::ResolveFace(
    const FontQuery& query,
    FontFace& output) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FreeType adapter is not initialized");
    }
    for (Impl::FaceRecord& record : impl_->faces) {
        if (query.typeface == nullptr ||
            record.family.View() != query.typeface->Family() ||
            record.weight != query.typeface->Weight() ||
            record.style != query.typeface->Style() ||
            record.stretch != query.typeface->Stretch()) {
            continue;
        }
        if (query.requireCodePoint &&
            FT_Get_Char_Index(
                record.freeTypeFace, query.codePoint) == 0U) {
            continue;
        }
        if (record.referenceCount == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "FreeType face reference count reached its limit");
        }
        ++record.referenceCount;
        Impl::Describe(record, output);
        return {};
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No loaded FreeType face matches the query");
}

Base::Result<bool> FreeTypeAdapter::HasCodePoint(
    FontFaceHandle handle,
    std::uint32_t codePoint) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FreeType adapter is not initialized");
    }
    if (!handle.IsValid() ||
        handle.provider != AdapterIdentity ||
        codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FreeType coverage query is invalid");
    }
    FT_Face face = static_cast<FT_Face>(
        FindNativeFace(handle));
    if (face == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "FreeType coverage query references a stale face");
    }
    return FT_Get_Char_Index(face, codePoint) != 0U;
}

void FreeTypeAdapter::ReleaseFace(
    FontFaceHandle handle) noexcept {
    if (impl_ == nullptr) return;
    for (std::uint32_t index = 0U;
         index < impl_->faces.Size();
         ++index) {
        Impl::FaceRecord& face = impl_->faces[index];
        if (face.id != handle.face ||
            face.generation != handle.generation ||
            handle.provider != AdapterIdentity) {
            continue;
        }
        if (face.referenceCount > 1U) {
            --face.referenceCount;
            return;
        }
        FT_Done_Face(face.freeTypeFace);
        if (index + 1U != impl_->faces.Size()) {
            impl_->faces[index] =
                std::move(impl_->faces[impl_->faces.Size() - 1U]);
        }
        impl_->faces.PopBack();
        return;
    }
}

bool FreeTypeAdapter::Supports(
    FontProviderIdentity provider) const noexcept {
    return impl_ != nullptr && provider == AdapterIdentity;
}

Base::Result<void> FreeTypeAdapter::Shape(
    const ShapingRequest& request,
    ShapedTextRun& output) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FreeType adapter is not initialized");
    }
    if (request.direction == TextDirection::RightToLeft ||
        request.script == Script::Arabic ||
        request.script == Script::Hebrew) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "FreeType-only shaping supports simple scripts only");
    }
    Impl::FaceRecord* face = impl_->Find(request.face);
    if (face == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Font face is not loaded");
    }
    const Base::Result<FT_UInt> pixelSize =
        ToPixelSize(request.pixelSize);
    if (!pixelSize) return pixelSize.GetStatus();
    if (FT_Set_Pixel_Sizes(
            face->freeTypeFace, 0U, pixelSize.Value()) != 0) {
        return FreeTypeFailure("FreeType pixel-size selection failed");
    }
    output.face = request.face;
    output.direction = request.direction == TextDirection::Auto
        ? TextDirection::LeftToRight
        : request.direction;
    output.script = request.script;
    std::uint32_t offset = 0U;
    while (offset < request.text.SizeBytes()) {
        const std::uint32_t cluster = offset;
        const std::uint32_t codePoint =
            DecodeUtf8CodePoint(request.text, offset);
        const FT_UInt glyphIndex =
            FT_Get_Char_Index(face->freeTypeFace, codePoint);
        if (FT_Load_Glyph(
                face->freeTypeFace, glyphIndex,
                FT_LOAD_DEFAULT) != 0) {
            return FreeTypeFailure(
                "FreeType simple shaping could not load a glyph");
        }
        ShapedGlyph glyph;
        glyph.glyph = glyphIndex;
        glyph.cluster = cluster;
        glyph.advanceX = From26Dot6(
            face->freeTypeFace->glyph->advance.x);
        Base::Result<void> appended =
            output.glyphs.TryPushBack(glyph);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> FreeTypeAdapter::GetMetrics(
    const GlyphRequest& request,
    GlyphMetrics& output) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FreeType adapter is not initialized");
    }
    Impl::FaceRecord* face = impl_->Find(request.face);
    if (face == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Font face is not loaded");
    }
    const Base::Result<FT_UInt> pixelSize =
        ToPixelSize(request.pixelSize, request.dpiScale);
    if (!pixelSize) return pixelSize.GetStatus();
    if (FT_Set_Pixel_Sizes(
            face->freeTypeFace, 0U, pixelSize.Value()) != 0 ||
        FT_Load_Glyph(
            face->freeTypeFace, request.glyph,
            FT_LOAD_DEFAULT) != 0) {
        return FreeTypeFailure("FreeType glyph metrics failed");
    }
    const FT_Glyph_Metrics& metrics =
        face->freeTypeFace->glyph->metrics;
    output.width = From26Dot6(metrics.width);
    output.height = From26Dot6(metrics.height);
    output.bearingX = From26Dot6(metrics.horiBearingX);
    output.bearingY = From26Dot6(metrics.horiBearingY);
    output.advanceX = From26Dot6(metrics.horiAdvance);
    return {};
}

Base::Result<void> FreeTypeAdapter::Rasterize(
    const GlyphRequest& request,
    GlyphBitmap& output) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FreeType adapter is not initialized");
    }
    Impl::FaceRecord* face = impl_->Find(request.face);
    if (face == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Font face is not loaded");
    }
    const Base::Result<FT_UInt> pixelSize =
        ToPixelSize(request.pixelSize, request.dpiScale);
    if (!pixelSize) return pixelSize.GetStatus();
    if (FT_Set_Pixel_Sizes(
            face->freeTypeFace, 0U, pixelSize.Value()) != 0 ||
        FT_Load_Glyph(
            face->freeTypeFace, request.glyph,
            FT_LOAD_DEFAULT) != 0 ||
        FT_Render_Glyph(
            face->freeTypeFace->glyph,
            FT_RENDER_MODE_NORMAL) != 0) {
        return FreeTypeFailure("FreeType glyph rasterization failed");
    }
    const FT_Bitmap& bitmap =
        face->freeTypeFace->glyph->bitmap;
    if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "FreeType returned a non-grayscale glyph bitmap");
    }
    const std::uint64_t byteCount =
        static_cast<std::uint64_t>(bitmap.width) *
        static_cast<std::uint64_t>(bitmap.rows);
    if (byteCount > UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Glyph bitmap exceeds Aero container limits");
    }
    Base::Result<void> resized =
        output.pixels.TryResize(
            static_cast<std::uint32_t>(byteCount));
    if (!resized) return resized.GetStatus();
    const int pitch = bitmap.pitch < 0
        ? -bitmap.pitch : bitmap.pitch;
    for (std::uint32_t row = 0U; row < bitmap.rows; ++row) {
        const std::uint32_t sourceRow = bitmap.pitch < 0
            ? bitmap.rows - 1U - row : row;
        const unsigned char* source =
            bitmap.buffer + sourceRow * static_cast<std::uint32_t>(pitch);
        for (std::uint32_t column = 0U;
             column < bitmap.width;
             ++column) {
            output.pixels[row * bitmap.width + column] =
                source[column];
        }
    }
    output.format = GlyphPixelFormat::Gray8;
    output.width = bitmap.width;
    output.height = bitmap.rows;
    output.strideBytes = bitmap.width;
    output.bearingX = face->freeTypeFace->glyph->bitmap_left;
    output.bearingY = face->freeTypeFace->glyph->bitmap_top;
    return {};
}

Base::Result<void> FreeTypeAdapter::ExtractOutline(
    const GlyphRequest& request,
    GlyphOutline& output) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FreeType adapter is not initialized");
    }
    Impl::FaceRecord* face = impl_->Find(request.face);
    if (face == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Font face is not loaded");
    }
    const Base::Result<FT_UInt> pixelSize =
        ToPixelSize(request.pixelSize, request.dpiScale);
    if (!pixelSize) return pixelSize.GetStatus();
    if (FT_Set_Pixel_Sizes(
            face->freeTypeFace, 0U, pixelSize.Value()) != 0 ||
        FT_Load_Glyph(
            face->freeTypeFace, request.glyph,
            FT_LOAD_NO_BITMAP) != 0) {
        return FreeTypeFailure("FreeType glyph outline loading failed");
    }
    if (face->freeTypeFace->glyph->format !=
        FT_GLYPH_FORMAT_OUTLINE) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Glyph has no scalable outline");
    }
    FT_Outline_Funcs functions{};
    functions.move_to = &MoveToCallback;
    functions.line_to = &LineToCallback;
    functions.conic_to = &ConicToCallback;
    functions.cubic_to = &CubicToCallback;
    functions.shift = 0;
    functions.delta = 0;
    OutlineBuilder builder;
    builder.outline = &output;
    const int decomposed = FT_Outline_Decompose(
        &face->freeTypeFace->glyph->outline,
        &functions, &builder);
    if (decomposed != 0) {
        return builder.status.IsOk()
            ? FreeTypeFailure("FreeType outline decomposition failed")
            : builder.status;
    }
    if (!CloseContour(builder)) return builder.status;
    return {};
}

void* FreeTypeAdapter::FindNativeFace(
    FontFaceHandle face) noexcept {
    if (impl_ == nullptr) return nullptr;
    Impl::FaceRecord* record = impl_->Find(face);
    return record != nullptr ? record->freeTypeFace : nullptr;
}

} // namespace Aero::Text
