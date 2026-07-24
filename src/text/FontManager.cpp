#include <Aero/Text/FontManager.hpp>

#include <Aero/Base/Utf8.hpp>

#include <cmath>
#include <cstdint>

namespace Aero::Text {
namespace {

Base::Result<void> ValidateGlyphRequest(
    const GlyphRequest& request) noexcept {
    if (!request.face.IsValid() ||
        request.glyph == InvalidGlyphId ||
        !std::isfinite(request.pixelSize) ||
        request.pixelSize <= 0.0F ||
        !std::isfinite(request.dpiScale) ||
        request.dpiScale <= 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Glyph request is invalid");
    }
    return {};
}

Base::Result<void> ValidateOutline(
    const GlyphOutline& outline) noexcept {
    for (const OutlineCommand& command : outline.commands) {
        std::uint8_t expected = 0U;
        switch (command.kind) {
        case OutlineCommandKind::MoveTo:
        case OutlineCommandKind::LineTo:
            expected = 1U;
            break;
        case OutlineCommandKind::QuadraticTo:
            expected = 2U;
            break;
        case OutlineCommandKind::CubicTo:
            expected = 3U;
            break;
        case OutlineCommandKind::Close:
            expected = 0U;
            break;
        }
        if (command.pointCount != expected) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Glyph outline command has an invalid point count");
        }
        for (std::uint8_t index = 0U;
             index < command.pointCount;
             ++index) {
            if (!std::isfinite(command.points[index].x) ||
                !std::isfinite(command.points[index].y)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Glyph outline contains a non-finite point");
            }
        }
    }
    return {};
}

} // namespace

Base::Result<void> FontManager::Initialize() noexcept {
    if (initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "FontManager is already initialized");
    }
    registrations_.Clear();
    initialized_ = true;
    return {};
}

void FontManager::Shutdown() noexcept {
    registrations_.Clear();
    initialized_ = false;
}

Base::Result<void> FontManager::RegisterProvider(
    const TextProviderRegistration& registration) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (registration.fonts == nullptr ||
        registration.shaper == nullptr ||
        registration.rasterizer == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text provider registration requires all three services");
    }

    const FontProviderIdentity identity =
        registration.fonts->Identity();
    if (!identity.IsValid() ||
        !registration.shaper->Supports(identity) ||
        !registration.rasterizer->Supports(identity)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text services do not share a valid provider identity");
    }
    if (FindProvider(identity.id) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Font provider ID is already registered");
    }

    ProviderRecord record;
    record.identity = identity;
    record.fonts = registration.fonts;
    record.shaper = registration.shaper;
    record.rasterizer = registration.rasterizer;
    return registrations_.TryPushBack(record);
}

Base::Result<void> FontManager::UnregisterProvider(
    FontProviderId provider) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    for (std::uint32_t index = 0U;
         index < registrations_.Size();
         ++index) {
        if (registrations_[index].identity.id != provider) continue;
        if (index + 1U != registrations_.Size()) {
            registrations_[index] =
                registrations_[registrations_.Size() - 1U];
        }
        registrations_.PopBack();
        return {};
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Font provider is not registered");
}

Base::Result<void> FontManager::LoadFace(
    FontProviderId provider,
    const FontSource& source,
    const Typeface& typeface,
    FontFace& output) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    ProviderRecord* record = FindProvider(provider);
    if (record == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Font provider is not registered");
    }
    if (source.identifier.Empty() ||
        (source.kind == FontSourceKind::Memory &&
         source.bytes.Empty())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Font source is incomplete");
    }

    output = {};
    Base::Result<void> loaded =
        record->fonts->LoadFace(source, typeface, output);
    if (!loaded) return loaded.GetStatus();
    return ValidateFace(output, record->identity);
}

Base::Result<void> FontManager::ResolveFace(
    const FontQuery& query,
    FontFace& output) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (query.typeface == nullptr ||
        query.typeface->Family().Empty() ||
        (query.requireCodePoint &&
         (query.codePoint > 0x10FFFFU ||
          (query.codePoint >= 0xD800U &&
           query.codePoint <= 0xDFFFU)))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Font query is invalid");
    }

    if (query.preferredProvider != InvalidFontProviderId) {
        ProviderRecord* record =
            FindProvider(query.preferredProvider);
        if (record == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Preferred font provider is not registered");
        }
        output = {};
        Base::Result<void> resolved =
            record->fonts->ResolveFace(query, output);
        if (!resolved) return resolved.GetStatus();
        return ValidateFace(output, record->identity);
    }

    for (ProviderRecord& record : registrations_) {
        output = {};
        Base::Result<void> resolved =
            record.fonts->ResolveFace(query, output);
        if (resolved) {
            return ValidateFace(output, record.identity);
        }
        if (resolved.GetStatus().code != Base::ErrorCode::NotFound) {
            return resolved.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No registered provider resolved the requested font face");
}

Base::Result<void> FontManager::ReleaseFace(
    FontFaceHandle face) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    ProviderRecord* record = FindProvider(face);
    if (record == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Font face provider is missing or its version changed");
    }
    record->fonts->ReleaseFace(face);
    return {};
}

Base::Result<void> FontManager::Shape(
    const ShapingRequest& request,
    ShapedTextRun& output) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (!request.face.IsValid() ||
        !std::isfinite(request.pixelSize) ||
        request.pixelSize <= 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Shaping request is invalid");
    }
    const Base::Utf8Validation utf8 =
        Base::ValidateUtf8(request.text);
    if (!utf8.valid) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidUtf8,
            "Shaping input is not valid UTF-8");
    }
    ProviderRecord* record = FindProvider(request.face);
    if (record == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Font face provider is missing or its version changed");
    }

    output.glyphs.Clear();
    output.face = {};
    output.direction = TextDirection::Auto;
    output.script = Script::Unknown;
    Base::Result<void> shaped =
        record->shaper->Shape(request, output);
    if (!shaped) return shaped.GetStatus();
    if (output.face != request.face ||
        output.direction == TextDirection::Auto) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Text shaper returned a mismatched face or unresolved direction");
    }
    for (const ShapedGlyph& glyph : output.glyphs) {
        if (glyph.glyph == InvalidGlyphId ||
            glyph.cluster >= request.text.SizeBytes() ||
            !std::isfinite(glyph.advanceX) ||
            !std::isfinite(glyph.offsetX) ||
            !std::isfinite(glyph.offsetY)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Text shaper returned invalid glyph data");
        }
    }
    return {};
}

Base::Result<void> FontManager::GetGlyphMetrics(
    const GlyphRequest& request,
    GlyphMetrics& output) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> valid = ValidateGlyphRequest(request);
    if (!valid) return valid.GetStatus();
    ProviderRecord* record = FindProvider(request.face);
    if (record == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Font face provider is missing or its version changed");
    }
    output = {};
    Base::Result<void> measured =
        record->rasterizer->GetMetrics(request, output);
    if (!measured) return measured.GetStatus();
    if (!std::isfinite(output.width) ||
        !std::isfinite(output.height) ||
        !std::isfinite(output.bearingX) ||
        !std::isfinite(output.bearingY) ||
        !std::isfinite(output.advanceX) ||
        output.width < 0.0F ||
        output.height < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Glyph rasterizer returned invalid metrics");
    }
    return {};
}

Base::Result<void> FontManager::RasterizeGlyph(
    const GlyphRequest& request,
    GlyphBitmap& output) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> valid = ValidateGlyphRequest(request);
    if (!valid) return valid.GetStatus();
    ProviderRecord* record = FindProvider(request.face);
    if (record == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Font face provider is missing or its version changed");
    }

    output.pixels.Clear();
    output.width = 0U;
    output.height = 0U;
    output.strideBytes = 0U;
    Base::Result<void> rasterized =
        record->rasterizer->Rasterize(request, output);
    if (!rasterized) return rasterized.GetStatus();
    const std::uint64_t required =
        static_cast<std::uint64_t>(output.strideBytes) *
        static_cast<std::uint64_t>(output.height);
    if ((output.width != 0U && output.strideBytes < output.width) ||
        required > output.pixels.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Glyph rasterizer returned an invalid bitmap");
    }
    return {};
}

Base::Result<void> FontManager::ExtractGlyphOutline(
    const GlyphRequest& request,
    GlyphOutline& output) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> valid = ValidateGlyphRequest(request);
    if (!valid) return valid.GetStatus();
    ProviderRecord* record = FindProvider(request.face);
    if (record == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Font face provider is missing or its version changed");
    }
    output.commands.Clear();
    Base::Result<void> extracted =
        record->rasterizer->ExtractOutline(request, output);
    if (!extracted) return extracted.GetStatus();
    return ValidateOutline(output);
}

FontManager::ProviderRecord* FontManager::FindProvider(
    FontProviderId provider) noexcept {
    for (ProviderRecord& record : registrations_) {
        if (record.identity.id == provider) return &record;
    }
    return nullptr;
}

const FontManager::ProviderRecord* FontManager::FindProvider(
    FontProviderId provider) const noexcept {
    for (const ProviderRecord& record : registrations_) {
        if (record.identity.id == provider) return &record;
    }
    return nullptr;
}

FontManager::ProviderRecord* FontManager::FindProvider(
    FontFaceHandle face) noexcept {
    ProviderRecord* record = FindProvider(face.provider.id);
    return record != nullptr &&
        record->identity == face.provider
        ? record : nullptr;
}

Base::Result<void> FontManager::VerifyReady() const noexcept {
    if (!initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "FontManager is not initialized");
    }
    return {};
}

Base::Result<void> FontManager::ValidateFace(
    const FontFace& face,
    FontProviderIdentity expected) noexcept {
    const FontMetrics& metrics = face.metrics;
    if (!face.handle.IsValid() ||
        face.handle.provider != expected ||
        !std::isfinite(metrics.unitsPerEm) ||
        !std::isfinite(metrics.ascent) ||
        !std::isfinite(metrics.descent) ||
        !std::isfinite(metrics.lineGap) ||
        !std::isfinite(metrics.underlinePosition) ||
        !std::isfinite(metrics.underlineThickness) ||
        metrics.unitsPerEm <= 0.0F ||
        metrics.underlineThickness < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Font provider returned an invalid face descriptor");
    }
    return {};
}

} // namespace Aero::Text
