#include <Aero/Text/TextLayout.hpp>

#include <Aero/Base/Utf8.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace Aero::Text {
namespace {

constexpr Base::StringView EllipsisText("\xE2\x80\xA6", 3U);

enum class SegmentKind : std::uint8_t {
    Word = 0U,
    Whitespace,
    Ideograph
};

struct TextSegment final {
    std::uint32_t start = 0U;
    std::uint32_t length = 0U;
    SegmentKind kind = SegmentKind::Word;
};

struct WordBoundary final {
    std::uint32_t runCount = 0U;
    std::uint32_t textEnd = 0U;
    float width = 0.0F;
};

std::uint32_t DecodeCodePoint(
    Base::StringView text,
    std::uint32_t& offset) noexcept {
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(text.Data());
    const unsigned char lead = bytes[offset++];
    if (lead <= 0x7FU) return lead;

    std::uint32_t codePoint = 0U;
    std::uint32_t continuationCount = 0U;
    if ((lead & 0xE0U) == 0xC0U) {
        codePoint = lead & 0x1FU;
        continuationCount = 1U;
    } else if ((lead & 0xF0U) == 0xE0U) {
        codePoint = lead & 0x0FU;
        continuationCount = 2U;
    } else {
        codePoint = lead & 0x07U;
        continuationCount = 3U;
    }
    for (std::uint32_t index = 0U;
         index < continuationCount; ++index) {
        codePoint = (codePoint << 6U) |
            static_cast<std::uint32_t>(bytes[offset++] & 0x3FU);
    }
    return codePoint;
}

bool IsInlineWhitespace(std::uint32_t codePoint) noexcept {
    return codePoint == 0x09U ||
        codePoint == 0x20U ||
        codePoint == 0xA0U ||
        codePoint == 0x1680U ||
        (codePoint >= 0x2000U && codePoint <= 0x200AU) ||
        codePoint == 0x202FU ||
        codePoint == 0x205FU ||
        codePoint == 0x3000U;
}

bool IsIdeographicBreak(std::uint32_t codePoint) noexcept {
    return
        (codePoint >= 0x2E80U && codePoint <= 0x9FFFU) ||
        (codePoint >= 0xF900U && codePoint <= 0xFAFFU) ||
        (codePoint >= 0x20000U && codePoint <= 0x3134FU);
}

Base::Result<void> TokenizeParagraph(
    Base::StringView text,
    std::uint32_t paragraphStart,
    std::uint32_t paragraphEnd,
    Base::Vector<TextSegment>& segments) noexcept {
    std::uint32_t offset = paragraphStart;
    while (offset < paragraphEnd) {
        const std::uint32_t segmentStart = offset;
        const std::uint32_t codePoint = DecodeCodePoint(text, offset);
        SegmentKind kind = SegmentKind::Word;
        if (IsInlineWhitespace(codePoint)) {
            kind = SegmentKind::Whitespace;
        } else if (IsIdeographicBreak(codePoint)) {
            kind = SegmentKind::Ideograph;
        }

        if (kind != SegmentKind::Ideograph) {
            while (offset < paragraphEnd) {
                std::uint32_t next = offset;
                const std::uint32_t nextCodePoint =
                    DecodeCodePoint(text, next);
                const SegmentKind nextKind =
                    IsInlineWhitespace(nextCodePoint)
                        ? SegmentKind::Whitespace
                        : (IsIdeographicBreak(nextCodePoint)
                            ? SegmentKind::Ideograph
                            : SegmentKind::Word);
                if (nextKind != kind) break;
                offset = next;
            }
        }

        TextSegment segment;
        segment.start = segmentStart;
        segment.length = offset - segmentStart;
        segment.kind = kind;
        Base::Result<void> appended =
            segments.TryPushBack(segment);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> ValidateRequest(
    const TextLayoutRequest& request) noexcept {
    auto validFace = [](const FontFace& face) noexcept {
        return face.handle.IsValid() &&
            std::isfinite(face.metrics.unitsPerEm) &&
            face.metrics.unitsPerEm > 0.0F &&
            std::isfinite(face.metrics.ascent) &&
            std::isfinite(face.metrics.descent) &&
            std::isfinite(face.metrics.lineGap);
    };
    if (!validFace(request.face) ||
        !std::isfinite(request.pixelSize) ||
        request.pixelSize <= 0.0F ||
        request.maxWidth < 0.0F ||
        std::isnan(request.maxWidth) ||
        !std::isfinite(request.lineHeight) ||
        request.lineHeight < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text layout request contains invalid face or dimensions");
    }
    for (const FontFace& fallback : request.fallbackFaces) {
        if (!validFace(fallback)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Text layout fallback face is invalid");
        }
    }
    const Base::Utf8Validation utf8 =
        Base::ValidateUtf8(request.text);
    if (!utf8.valid) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidUtf8,
            "Text layout input is not valid UTF-8");
    }
    return {};
}

} // namespace

Base::Result<void> TextLayout::TryAddRun(GlyphRun&& run) noexcept {
    if (!run.face.IsValid() ||
        !std::isfinite(run.pixelSize) ||
        run.pixelSize <= 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Glyph run requires a valid face and positive pixel size");
    }
    for (const PositionedGlyph& glyph : run.glyphs) {
        if (glyph.glyph == InvalidGlyphId ||
            !std::isfinite(glyph.x) ||
            !std::isfinite(glyph.y) ||
            !std::isfinite(glyph.advanceX) ||
            glyph.advanceX < 0.0F) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Glyph run contains invalid positioned glyph data");
        }
    }
    Base::Result<GlyphRun*> appended =
        runs_.TryEmplaceBack(std::move(run));
    return appended
        ? Base::Result<void>()
        : Base::Result<void>(appended.GetStatus());
}

Base::Result<void> TextLayout::TryAddLine(
    const TextLine& line) noexcept {
    if (line.firstRun > runs_.Size() ||
        line.runCount > runs_.Size() - line.firstRun ||
        !std::isfinite(line.width) ||
        !std::isfinite(line.ascent) ||
        !std::isfinite(line.descent) ||
        !std::isfinite(line.baseline) ||
        !std::isfinite(line.y) ||
        !std::isfinite(line.x) ||
        line.width < 0.0F ||
        line.ascent < 0.0F ||
        line.descent < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text line range or metrics are invalid");
    }
    return lines_.TryPushBack(line);
}

Base::Result<void> TextLayout::SetSize(
    TextLayoutSize size) noexcept {
    if (!std::isfinite(size.width) ||
        !std::isfinite(size.height) ||
        size.width < 0.0F ||
        size.height < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text layout size must be finite and nonnegative");
    }
    size_ = size;
    naturalSize_ = size;
    return {};
}

Base::Result<void> TextLayout::ShapeAndMeasure(
    FontManager& fonts,
    const TextLayoutRequest& request) noexcept {
    Base::Result<void> valid = ValidateRequest(request);
    if (!valid) return valid.GetStatus();

    Base::IAllocator* const allocator = &runs_.Allocator();
    TextLayout pending(allocator);
    pending.alignment_ = request.alignment;
    if (request.text.Empty()) {
        *this = std::move(pending);
        return {};
    }

    float ascent = 0.0F;
    float descent = 0.0F;
    float naturalLineHeight = 0.0F;
    auto includeMetrics = [&](
        const FontFace& face) noexcept {
        const float scale =
            request.pixelSize / face.metrics.unitsPerEm;
        const float faceAscent =
            std::max(0.0F, face.metrics.ascent * scale);
        const float faceDescent =
            std::max(0.0F, -face.metrics.descent * scale);
        const float faceHeight =
            faceAscent + faceDescent +
            std::max(0.0F, face.metrics.lineGap * scale);
        ascent = std::max(ascent, faceAscent);
        descent = std::max(descent, faceDescent);
        naturalLineHeight =
            std::max(naturalLineHeight, faceHeight);
    };
    includeMetrics(request.face);
    for (const FontFace& fallback : request.fallbackFaces) {
        includeMetrics(fallback);
    }
    naturalLineHeight =
        std::max(naturalLineHeight, ascent + descent);
    const float lineHeight = request.lineHeight > 0.0F
        ? request.lineHeight
        : naturalLineHeight;
    if (!std::isfinite(lineHeight) || lineHeight <= 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text layout line height must be positive");
    }
    const float baseline =
        ascent + (lineHeight - ascent - descent) * 0.5F;
    const bool constrained = std::isfinite(request.maxWidth);
    const bool wrap =
        request.wrapping == TextWrapping::Wrap && constrained;

    float lineY = 0.0F;
    float maximumWidth = 0.0F;
    std::uint32_t paragraphStart = 0U;

    auto selectFace = [&](
        std::uint32_t codePoint) noexcept
            -> Base::Result<FontFaceHandle> {
        if (request.fallbackFaces.Empty()) {
            return request.face.handle;
        }
        Base::Result<bool> primary =
            fonts.HasCodePoint(
                request.face.handle, codePoint);
        if (!primary) return primary.GetStatus();
        if (primary.Value()) return request.face.handle;
        for (const FontFace& fallback :
             request.fallbackFaces) {
            Base::Result<bool> covered =
                fonts.HasCodePoint(
                    fallback.handle, codePoint);
            if (!covered) return covered.GetStatus();
            if (covered.Value()) return fallback.handle;
        }
        return request.face.handle;
    };

    auto shapeText = [&](
        Base::StringView text,
        std::uint32_t clusterBase,
        float penX,
        Base::Vector<GlyphRun>& runs,
        float& width) noexcept -> Base::Result<void> {
        runs.Clear();
        width = 0.0F;
        if (text.Empty()) return {};

        auto appendSpan = [&](
            FontFaceHandle face,
            std::uint32_t spanStart,
            std::uint32_t spanEnd) noexcept
                -> Base::Result<void> {
            ShapingRequest shaping;
            shaping.face = face;
            shaping.text =
                text.Substr(spanStart, spanEnd - spanStart);
            shaping.pixelSize = request.pixelSize;
            shaping.direction = request.direction;
            shaping.script = request.script;
            shaping.language = request.language;
            ShapedTextRun shaped(allocator);
            Base::Result<void> shapedResult =
                fonts.Shape(shaping, shaped);
            if (!shapedResult) return shapedResult.GetStatus();

            GlyphRun run(allocator);
            run.face = shaped.face;
            run.pixelSize = request.pixelSize;
            run.direction = shaped.direction;
            run.script = shaped.script;
            Base::Result<void> reserved =
                run.glyphs.TryReserve(shaped.glyphs.Size());
            if (!reserved) return reserved.GetStatus();
            for (const ShapedGlyph& source : shaped.glyphs) {
                if (source.advanceX < 0.0F) {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "Horizontal layout requires nonnegative advances");
                }
                if (spanStart >
                        UINT32_MAX - source.cluster ||
                    clusterBase >
                        UINT32_MAX -
                            (spanStart + source.cluster)) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Text layout cluster offset overflowed");
                }
                PositionedGlyph glyph;
                glyph.glyph = source.glyph;
                glyph.cluster =
                    clusterBase + spanStart + source.cluster;
                glyph.x =
                    penX + width + source.offsetX;
                glyph.y =
                    lineY + baseline - source.offsetY;
                glyph.advanceX = source.advanceX;
                Base::Result<void> appended =
                    run.glyphs.TryPushBack(glyph);
                if (!appended) return appended.GetStatus();
                width += source.advanceX;
            }
            Base::Result<GlyphRun*> appended =
                runs.TryEmplaceBack(std::move(run));
            return appended
                ? Base::Result<void>()
                : Base::Result<void>(
                    appended.GetStatus());
        };

        std::uint32_t offset = 0U;
        std::uint32_t spanStart = 0U;
        FontFaceHandle spanFace;
        while (offset < text.SizeBytes()) {
            const std::uint32_t characterStart = offset;
            const std::uint32_t codePoint =
                DecodeCodePoint(text, offset);
            Base::Result<FontFaceHandle> selected =
                selectFace(codePoint);
            if (!selected) return selected.GetStatus();
            if (!spanFace.IsValid()) {
                spanFace = selected.Value();
                spanStart = characterStart;
                continue;
            }
            if (selected.Value() == spanFace) continue;
            Base::Result<void> appended =
                appendSpan(
                    spanFace, spanStart, characterStart);
            if (!appended) return appended.GetStatus();
            spanFace = selected.Value();
            spanStart = characterStart;
        }
        return appendSpan(
            spanFace, spanStart, text.SizeBytes());
    };

    auto shapeSegment = [&](
        std::uint32_t start,
        std::uint32_t length,
        float penX,
        Base::Vector<GlyphRun>& runs,
        float& width) noexcept -> Base::Result<void> {
        return shapeText(
            request.text.Substr(start, length),
            start, penX, runs, width);
    };

    auto appendEllipsis = [&](
        TextLine& line,
        float penX) noexcept -> Base::Result<float> {
        Base::Vector<GlyphRun> runs(allocator);
        float width = 0.0F;
        Base::Result<void> shaped =
            shapeText(
                EllipsisText,
                line.textStart + line.textLength,
                penX, runs, width);
        if (!shaped) return shaped.GetStatus();
        if (width > request.maxWidth) {
            return 0.0F;
        }
        for (GlyphRun& run : runs) {
            Base::Result<void> appended =
                pending.TryAddRun(std::move(run));
            if (!appended) return appended.GetStatus();
            ++line.runCount;
        }
        return width;
    };

    auto trimLine = [&](
        TextLine& line,
        Base::Span<const WordBoundary> boundaries) noexcept
            -> Base::Result<void> {
        if (!constrained ||
            request.trimming == TextTrimming::None ||
            line.width <= request.maxWidth) {
            return {};
        }

        Base::Vector<GlyphRun> ellipsisRuns(allocator);
        float ellipsisWidth = 0.0F;
        Base::Result<void> ellipsisResult =
            shapeText(
                EllipsisText,
                line.textStart + line.textLength,
                0.0F, ellipsisRuns, ellipsisWidth);
        if (!ellipsisResult) return ellipsisResult.GetStatus();
        const float contentLimit =
            std::max(0.0F, request.maxWidth - ellipsisWidth);

        std::uint32_t keepRunCount = 0U;
        std::uint32_t keepGlyphCount = 0U;
        float keptWidth = 0.0F;
        bool foundPartialRun = false;

        if (request.trimming == TextTrimming::WordEllipsis &&
            request.direction != TextDirection::RightToLeft) {
            for (const WordBoundary& boundary : boundaries) {
                if (boundary.width <= contentLimit) {
                    keepRunCount = boundary.runCount;
                    keptWidth = boundary.width;
                }
            }
        }

        if (keepRunCount == 0U) {
            keptWidth = 0.0F;
            for (std::uint32_t runOffset = 0U;
                 runOffset < line.runCount; ++runOffset) {
                GlyphRun& run =
                    pending.runs_[line.firstRun + runOffset];
                std::uint32_t glyphCount = 0U;
                for (const PositionedGlyph& glyph : run.glyphs) {
                    if (keptWidth + glyph.advanceX >
                        contentLimit) {
                        foundPartialRun = true;
                        break;
                    }
                    keptWidth += glyph.advanceX;
                    ++glyphCount;
                }
                if (glyphCount == run.glyphs.Size()) {
                    ++keepRunCount;
                    continue;
                }
                keepGlyphCount = glyphCount;
                foundPartialRun = true;
                break;
            }
        }

        std::uint32_t targetRunCount = keepRunCount;
        if (foundPartialRun && keepGlyphCount > 0U) {
            GlyphRun& partial =
                pending.runs_[line.firstRun + keepRunCount];
            Base::Result<void> resized =
                partial.glyphs.TryResize(keepGlyphCount);
            if (!resized) return resized.GetStatus();
            ++targetRunCount;
        }
        Base::Result<void> runsResized =
            pending.runs_.TryResize(line.firstRun + targetRunCount);
        if (!runsResized) return runsResized.GetStatus();
        line.runCount = targetRunCount;
        line.width = keptWidth;

        if (ellipsisWidth <= request.maxWidth) {
            Base::Result<float> appended =
                appendEllipsis(line, keptWidth);
            if (!appended) return appended.GetStatus();
            line.width += appended.Value();
        }
        pending.trimmed_ = true;
        return {};
    };

    auto layoutParagraph = [&](
        std::uint32_t start,
        std::uint32_t end) noexcept -> Base::Result<void> {
        Base::Vector<TextSegment> segments(allocator);
        Base::Result<void> tokenized =
            TokenizeParagraph(request.text, start, end, segments);
        if (!tokenized) return tokenized.GetStatus();

        std::uint32_t lineStart = start;
        std::uint32_t lineEnd = start;
        std::uint32_t firstRun = pending.runs_.Size();
        float lineWidth = 0.0F;
        bool lineEndsWhitespace = false;
        Base::Vector<WordBoundary> boundaries(allocator);

        auto finishLine = [&]() noexcept -> Base::Result<void> {
            TextLine line;
            line.firstRun = firstRun;
            line.runCount = pending.runs_.Size() - firstRun;
            line.textStart = lineStart;
            line.textLength = lineEnd - lineStart;
            line.width = lineWidth;
            line.ascent = ascent;
            line.descent = descent;
            line.baseline = baseline;
            line.y = lineY;
            Base::Result<void> trimmed =
                trimLine(line, boundaries.AsSpan());
            if (!trimmed) return trimmed.GetStatus();
            maximumWidth = std::max(maximumWidth, line.width);
            Base::Result<void> appended =
                pending.TryAddLine(line);
            if (!appended) return appended.GetStatus();
            lineY += lineHeight;
            firstRun = pending.runs_.Size();
            lineWidth = 0.0F;
            lineEndsWhitespace = false;
            boundaries.Clear();
            return {};
        };

        auto appendPiece = [&](
            const TextSegment& piece) noexcept -> Base::Result<void> {
            Base::Vector<GlyphRun> runs(allocator);
            float pieceWidth = 0.0F;
            Base::Result<void> shaped =
                shapeSegment(
                    piece.start, piece.length,
                    lineWidth, runs, pieceWidth);
            if (!shaped) return shaped.GetStatus();

            if (wrap &&
                lineWidth > 0.0F &&
                lineWidth + pieceWidth > request.maxWidth) {
                if (lineEndsWhitespace && !boundaries.Empty()) {
                    const WordBoundary& boundary =
                        boundaries.Back();
                    Base::Result<void> resized =
                        pending.runs_.TryResize(
                            firstRun + boundary.runCount);
                    if (!resized) return resized.GetStatus();
                    lineWidth = boundary.width;
                    lineEnd = boundary.textEnd;
                }
                Base::Result<void> finished = finishLine();
                if (!finished) return finished.GetStatus();
                lineStart = piece.start;
                lineEnd = piece.start;
                if (piece.kind == SegmentKind::Whitespace) {
                    lineStart = piece.start + piece.length;
                    lineEnd = lineStart;
                    return {};
                }
                runs.Clear();
                shaped = shapeSegment(
                    piece.start, piece.length,
                    0.0F, runs, pieceWidth);
                if (!shaped) return shaped.GetStatus();
            }

            if (piece.kind == SegmentKind::Whitespace &&
                lineWidth > 0.0F) {
                WordBoundary boundary;
                boundary.runCount =
                    pending.runs_.Size() - firstRun;
                boundary.textEnd = piece.start;
                boundary.width = lineWidth;
                Base::Result<void> added =
                    boundaries.TryPushBack(boundary);
                if (!added) return added.GetStatus();
            }

            for (GlyphRun& run : runs) {
                Base::Result<void> appended =
                    pending.TryAddRun(std::move(run));
                if (!appended) return appended.GetStatus();
            }
            lineWidth += pieceWidth;
            lineEnd = piece.start + piece.length;
            lineEndsWhitespace =
                piece.kind == SegmentKind::Whitespace;

            if (piece.kind == SegmentKind::Ideograph) {
                WordBoundary boundary;
                boundary.runCount =
                    pending.runs_.Size() - firstRun;
                boundary.textEnd = lineEnd;
                boundary.width = lineWidth;
                Base::Result<void> added =
                    boundaries.TryPushBack(boundary);
                if (!added) return added.GetStatus();
            }
            return {};
        };

        for (const TextSegment& segment : segments) {
            Base::Vector<GlyphRun> probe(allocator);
            float probeWidth = 0.0F;
            Base::Result<void> probed =
                shapeSegment(
                    segment.start, segment.length,
                    0.0F, probe, probeWidth);
            if (!probed) return probed.GetStatus();

            if (wrap &&
                segment.kind == SegmentKind::Word &&
                probeWidth > request.maxWidth) {
                std::uint32_t offset = segment.start;
                const std::uint32_t segmentEnd =
                    segment.start + segment.length;
                while (offset < segmentEnd) {
                    const std::uint32_t characterStart = offset;
                    (void)DecodeCodePoint(request.text, offset);
                    TextSegment character;
                    character.start = characterStart;
                    character.length = offset - characterStart;
                    character.kind = SegmentKind::Word;
                    Base::Result<void> appended =
                        appendPiece(character);
                    if (!appended) return appended.GetStatus();
                }
            } else {
                Base::Result<void> appended =
                    appendPiece(segment);
                if (!appended) return appended.GetStatus();
            }
        }

        lineEnd = std::max(lineEnd, lineStart);
        return finishLine();
    };

    const std::uint32_t textSize = request.text.SizeBytes();
    std::uint32_t offset = 0U;
    while (offset < textSize) {
        std::uint32_t paragraphEnd = offset;
        while (paragraphEnd < textSize &&
               request.text[paragraphEnd] != '\n' &&
               request.text[paragraphEnd] != '\r') {
            ++paragraphEnd;
        }
        Base::Result<void> laidOut =
            layoutParagraph(paragraphStart, paragraphEnd);
        if (!laidOut) return laidOut.GetStatus();
        if (paragraphEnd == textSize) {
            offset = textSize;
            break;
        }
        offset = paragraphEnd + 1U;
        if (request.text[paragraphEnd] == '\r' &&
            offset < textSize &&
            request.text[offset] == '\n') {
            ++offset;
        }
        paragraphStart = offset;
    }
    if (paragraphStart == textSize &&
        textSize > 0U &&
        (request.text[textSize - 1U] == '\n' ||
         request.text[textSize - 1U] == '\r')) {
        Base::Result<void> trailing =
            layoutParagraph(textSize, textSize);
        if (!trailing) return trailing.GetStatus();
    }

    pending.naturalSize_ = {maximumWidth, lineY};
    pending.size_ = pending.naturalSize_;
    *this = std::move(pending);
    return {};
}

Base::Result<void> TextLayout::Arrange(
    float finalWidth) noexcept {
    if (!std::isfinite(finalWidth) || finalWidth < 0.0F) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Text layout arrange width must be finite and nonnegative");
    }
    for (TextLine& line : lines_) {
        const float available =
            std::max(0.0F, finalWidth - line.width);
        float targetX = 0.0F;
        if (alignment_ == TextAlignment::Center) {
            targetX = available * 0.5F;
        } else if (alignment_ == TextAlignment::End) {
            targetX = available;
        }
        const float delta = targetX - line.x;
        for (std::uint32_t runOffset = 0U;
             runOffset < line.runCount; ++runOffset) {
            GlyphRun& run = runs_[line.firstRun + runOffset];
            for (PositionedGlyph& glyph : run.glyphs) {
                glyph.x += delta;
            }
        }
        line.x = targetX;
    }
    size_.width = finalWidth;
    size_.height = naturalSize_.height;
    return {};
}

void TextLayout::Clear() noexcept {
    runs_.Clear();
    lines_.Clear();
    size_ = {};
    naturalSize_ = {};
    alignment_ = TextAlignment::Start;
    trimmed_ = false;
}

} // namespace Aero::Text
