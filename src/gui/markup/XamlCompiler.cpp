#include "gui/markup/XamlSchema.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/core/PropertyStore.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/media/AnimationModel.hpp"
#include "render/RenderTree.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/HashSet.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Documents.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>
#include <Aero/VisualStateManager.hpp>

#if AERO_WITH_EXPAT
#include <expat.h>
#endif

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

// ============================================================================
// SECTION: XamlParser / XmlTokenizer / NodeReader
// Source: src/gui/markup/XamlParser.cpp
// ============================================================================
// Consolidated implementation. Keep sections ordered by dependency.

// ===== XmlTokenizer =====


// Canonical XML tokenizer implementation.



namespace Aero::Markup {
namespace {

constexpr Base::StringView MessageInvalidUtf8("XML input is not valid UTF-8");
constexpr Base::StringView MessageInputLimit("XML input exceeds the configured byte limit");
constexpr Base::StringView MessageUnexpectedEnd("XML input ended before the current construct was complete");
constexpr Base::StringView MessageMalformed("XML markup is malformed");
constexpr Base::StringView MessageUnsupportedDeclaration(
    "DTD and non-CDATA XML declarations are not supported");
constexpr Base::StringView MessageDepthLimit("XML element depth exceeds the configured limit");
constexpr Base::StringView MessageAttributeLimit(
    "XML element attribute count exceeds the configured limit");
constexpr Base::StringView MessageNameLimit("XML name exceeds the configured byte limit");
constexpr Base::StringView MessageTextLimit("XML text exceeds the configured byte limit");
constexpr Base::StringView MessageMismatchedEnd("XML end element does not match the open element");
constexpr Base::StringView MessageMultipleRoots("XML document contains more than one root element");
constexpr Base::StringView MessageUnknownEntity("XML entity reference is not one of the predefined entities");
constexpr Base::StringView MessageInvalidCharacter("XML contains a character that is not permitted by XML 1.0");
constexpr Base::StringView MessageDuplicateAttribute("XML element contains a duplicate attribute");
constexpr Base::StringView MessageNotInitialized("XML tokenizer has not been initialized");
constexpr Base::StringView MessageFailedState("XML tokenizer cannot continue after a parse failure");
constexpr Base::StringView MessageInvalidLimits("XML tokenizer limits are invalid");

bool IsWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool IsAsciiNameStart(unsigned char value) noexcept {
    return (value >= static_cast<unsigned char>('A') &&
            value <= static_cast<unsigned char>('Z')) ||
        (value >= static_cast<unsigned char>('a') &&
            value <= static_cast<unsigned char>('z')) ||
        value == static_cast<unsigned char>('_');
}

bool IsAsciiNameContinue(unsigned char value) noexcept {
    return IsAsciiNameStart(value) ||
        (value >= static_cast<unsigned char>('0') &&
            value <= static_cast<unsigned char>('9')) ||
        value == static_cast<unsigned char>('.') ||
        value == static_cast<unsigned char>('-') ||
        value == static_cast<unsigned char>(':');
}

bool IsDecimalDigit(char value) noexcept {
    return value >= '0' && value <= '9';
}

bool IsHexDigit(char value) noexcept {
    return IsDecimalDigit(value) ||
        (value >= 'a' && value <= 'f') ||
        (value >= 'A' && value <= 'F');
}

std::uint32_t HexValue(char value) noexcept {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint32_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return 10U + static_cast<std::uint32_t>(value - 'a');
    }
    return 10U + static_cast<std::uint32_t>(value - 'A');
}

bool IsXmlCharacter(std::uint32_t codePoint) noexcept {
    return codePoint == 0x9U || codePoint == 0xAU || codePoint == 0xDU ||
        (codePoint >= 0x20U && codePoint <= 0xD7FFU) ||
        (codePoint >= 0xE000U && codePoint <= 0xFFFDU) ||
        (codePoint >= 0x10000U && codePoint <= 0x10FFFFU);
}

bool Equals(Base::StringView left, const char* literal, std::uint32_t length) noexcept {
    return left.SizeBytes() == length &&
        (length == 0U || std::memcmp(left.Data(), literal, length) == 0);
}

} // namespace

void XmlToken::Clear() noexcept {
    kind_ = XmlTokenKind::None;
    name_.Clear();
    text_.Clear();
    attributes_.Clear();
    source_ = {};
    nameSource_ = {};
    emptyElement_ = false;
}

Utf8XmlTokenizer::Utf8XmlTokenizer(XmlTokenizerLimits limits) noexcept
    : limits_(limits),
      openElements_() {}

Base::Result<void> Utf8XmlTokenizer::Reset(
    Base::StringView utf8,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    input_ = utf8;
    diagnostics_ = diagnostics;
    openElements_.Clear();
    offset_ = 0U;
    line_ = 1U;
    column_ = 1U;
    rootSeen_ = false;
    rootClosed_ = false;
    endEmitted_ = false;
    initialized_ = false;
    failed_ = false;

    if (limits_.maxDepth == 0U || limits_.maxNameBytes == 0U) {
        failed_ = true;
        return Failure(
            Base::ErrorCode::InvalidArgument,
            XmlDiagnosticCodes::MalformedMarkup,
            MessageInvalidLimits,
            {});
    }

    if (static_cast<std::uint64_t>(utf8.SizeBytes()) > limits_.maxInputBytes) {
        failed_ = true;
        return Failure(
            Base::ErrorCode::OutOfRange,
            XmlDiagnosticCodes::InputLimitExceeded,
            MessageInputLimit,
            {{0U, 0U, 0U}, {0U, 0U, utf8.SizeBytes()}});
    }

    const Base::Utf8Validation validation = Base::ValidateUtf8(utf8);
    if (!validation.valid) {
        failed_ = true;
        const std::uint64_t errorOffset = validation.errorOffset;
        return Failure(
            Base::ErrorCode::InvalidUtf8,
            XmlDiagnosticCodes::InvalidUtf8,
            MessageInvalidUtf8,
            {{0U, 0U, errorOffset}, {0U, 0U, errorOffset}});
    }

    if (utf8.SizeBytes() >= 3U &&
        static_cast<unsigned char>(utf8.Data()[0]) == 0xEFU &&
        static_cast<unsigned char>(utf8.Data()[1]) == 0xBBU &&
        static_cast<unsigned char>(utf8.Data()[2]) == 0xBFU) {
        offset_ = 3U;
    }

    initialized_ = true;
    return {};
}

Base::Result<void> Utf8XmlTokenizer::Reset(
    Base::Stream& stream,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    ownedInput_.Clear();
    std::uint8_t buffer[4096];
    std::uint64_t total = 0U;
    for (;;) {
        Base::Result<std::uint32_t> read =
            stream.Read({buffer, sizeof(buffer)});
        if (!read) return read.GetStatus();
        if (read.Value() == 0U) break;
        if (read.Value() > limits_.maxInputBytes ||
            total > limits_.maxInputBytes - read.Value()) {
            return Failure(
                Base::ErrorCode::OutOfRange,
                XmlDiagnosticCodes::InputLimitExceeded,
                MessageInputLimit,
                {{0U, 0U, total}, {0U, 0U, total}});
        }
        Base::Result<void> appended = ownedInput_.Append(
            Base::StringView(
                reinterpret_cast<const char*>(buffer),
                read.Value()));
        if (!appended) return appended.GetStatus();
        total += read.Value();
    }
    return Reset(ownedInput_.View(), diagnostics);
}

Base::Result<XmlTokenKind> Utf8XmlTokenizer::Read(XmlToken& token) noexcept {
    token.Clear();

    if (!initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageNotInitialized.Data());
    }
    if (failed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageFailedState.Data());
    }
    if (endEmitted_) {
        token.kind_ = XmlTokenKind::EndOfDocument;
        token.source_ = {Position(), Position()};
        return token.kind_;
    }

    for (;;) {
        if (AtEnd()) {
            if (!openElements_.Empty()) {
                failed_ = true;
                return Failure(
                    Base::ErrorCode::ValidationFailed,
                    XmlDiagnosticCodes::UnexpectedEndOfInput,
                    MessageUnexpectedEnd,
                    {Position(), Position()});
            }
            if (!rootSeen_) {
                failed_ = true;
                return Failure(
                    Base::ErrorCode::ValidationFailed,
                    XmlDiagnosticCodes::MalformedMarkup,
                    MessageMalformed,
                    {Position(), Position()});
            }

            token.kind_ = XmlTokenKind::EndOfDocument;
            token.source_ = {Position(), Position()};
            endEmitted_ = true;
            return token.kind_;
        }

        if (input_.Data()[offset_] != '<') {
            if (openElements_.Empty()) {
                if (IsWhitespace(input_.Data()[offset_])) {
                    AdvanceCodePoint();
                    continue;
                }

                failed_ = true;
                const ::Aero::Diagnostics::SourcePosition begin = Position();
                AdvanceCodePoint();
                return Failure(
                    Base::ErrorCode::ValidationFailed,
                    XmlDiagnosticCodes::MalformedMarkup,
                    MessageMalformed,
                    SpanFrom(begin));
            }

            Base::Result<XmlTokenKind> textResult = ParseText(token);
            if (!textResult) {
                failed_ = true;
            }
            return textResult;
        }

        if (StartsWith("<!--", 4U)) {
            Base::Result<void> commentResult = SkipComment();
            if (!commentResult) {
                failed_ = true;
                return commentResult.GetStatus();
            }
            continue;
        }

        if (StartsWith("<?", 2U)) {
            Base::Result<void> processingResult = SkipProcessingInstruction();
            if (!processingResult) {
                failed_ = true;
                return processingResult.GetStatus();
            }
            continue;
        }

        if (StartsWith("<![CDATA[", 9U)) {
            if (openElements_.Empty()) {
                failed_ = true;
                return Failure(
                    Base::ErrorCode::ValidationFailed,
                    XmlDiagnosticCodes::MalformedMarkup,
                    MessageMalformed,
                    {Position(), Position()});
            }
            Base::Result<XmlTokenKind> cdataResult = ParseCdata(token);
            if (!cdataResult) {
                failed_ = true;
            }
            return cdataResult;
        }

        if (StartsWith("<!", 2U)) {
            failed_ = true;
            const ::Aero::Diagnostics::SourcePosition begin = Position();
            ConsumeAscii(2U);
            return Failure(
                Base::ErrorCode::Unsupported,
                XmlDiagnosticCodes::UnsupportedDeclaration,
                MessageUnsupportedDeclaration,
                SpanFrom(begin));
        }

        Base::Result<XmlTokenKind> result = StartsWith("</", 2U)
            ? ParseEndElement(token)
            : ParseStartElement(token);
        if (!result) {
            failed_ = true;
        }
        return result;
    }
}

bool Utf8XmlTokenizer::AtEnd() const noexcept {
    return offset_ >= input_.SizeBytes();
}

bool Utf8XmlTokenizer::StartsWith(
    const char* literal,
    std::uint32_t length) const noexcept {
    if (literal == nullptr || length > input_.SizeBytes() - offset_) {
        return false;
    }
    return length == 0U ||
        std::memcmp(input_.Data() + offset_, literal, length) == 0;
}

::Aero::Diagnostics::SourcePosition Utf8XmlTokenizer::Position() const noexcept {
    return {line_, column_, offset_};
}

::Aero::Diagnostics::SourceSpan Utf8XmlTokenizer::SpanFrom(
    ::Aero::Diagnostics::SourcePosition begin) const noexcept {
    return {begin, Position()};
}

std::uint32_t Utf8XmlTokenizer::CurrentCodePointLength() const noexcept {
    if (AtEnd()) {
        return 0U;
    }

    const unsigned char lead = static_cast<unsigned char>(input_.Data()[offset_]);
    if (lead < 0x80U) {
        return 1U;
    }
    if (lead < 0xE0U) {
        return 2U;
    }
    if (lead < 0xF0U) {
        return 3U;
    }
    return 4U;
}

std::uint32_t Utf8XmlTokenizer::CurrentCodePoint() const noexcept {
    if (AtEnd()) {
        return 0U;
    }

    const auto* bytes = reinterpret_cast<const unsigned char*>(
        input_.Data() + offset_);
    const std::uint32_t length = CurrentCodePointLength();
    if (length == 1U) {
        return bytes[0];
    }
    if (length == 2U) {
        return ((bytes[0] & 0x1FU) << 6U) |
            (bytes[1] & 0x3FU);
    }
    if (length == 3U) {
        return ((bytes[0] & 0x0FU) << 12U) |
            ((bytes[1] & 0x3FU) << 6U) |
            (bytes[2] & 0x3FU);
    }
    return ((bytes[0] & 0x07U) << 18U) |
        ((bytes[1] & 0x3FU) << 12U) |
        ((bytes[2] & 0x3FU) << 6U) |
        (bytes[3] & 0x3FU);
}

void Utf8XmlTokenizer::AdvanceCodePoint() noexcept {
    if (AtEnd()) {
        return;
    }

    const char current = input_.Data()[offset_];
    if (current == '\r') {
        ++offset_;
        if (!AtEnd() && input_.Data()[offset_] == '\n') {
            ++offset_;
        }
        ++line_;
        column_ = 1U;
        return;
    }
    if (current == '\n') {
        ++offset_;
        ++line_;
        column_ = 1U;
        return;
    }

    offset_ += CurrentCodePointLength();
    ++column_;
}

void Utf8XmlTokenizer::ConsumeAscii(std::uint32_t count) noexcept {
    for (std::uint32_t index = 0U; index < count && !AtEnd(); ++index) {
        AdvanceCodePoint();
    }
}

bool Utf8XmlTokenizer::SkipWhitespace() noexcept {
    bool skipped = false;
    while (!AtEnd() && IsWhitespace(input_.Data()[offset_])) {
        skipped = true;
        AdvanceCodePoint();
    }
    return skipped;
}

Base::Result<XmlTokenKind> Utf8XmlTokenizer::ParseStartElement(
    XmlToken& token) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    ConsumeAscii(1U);

    if (rootClosed_) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::MultipleDocumentElements,
            MessageMultipleRoots,
            SpanFrom(begin));
    }

    Base::Result<void> nameResult = ParseName(token.name_, token.nameSource_);
    if (!nameResult) {
        return nameResult.GetStatus();
    }
    if (openElements_.Size() >= limits_.maxDepth) {
        return Failure(
            Base::ErrorCode::OutOfRange,
            XmlDiagnosticCodes::DepthLimitExceeded,
            MessageDepthLimit,
            SpanFrom(begin));
    }

    for (;;) {
        const bool hadWhitespace = SkipWhitespace();
        if (AtEnd()) {
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::UnexpectedEndOfInput,
                MessageUnexpectedEnd,
                SpanFrom(begin));
        }

        if (StartsWith("/>", 2U)) {
            ConsumeAscii(2U);
            token.emptyElement_ = true;
            break;
        }
        if (input_.Data()[offset_] == '>') {
            ConsumeAscii(1U);
            break;
        }
        if (!hadWhitespace) {
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed,
                SpanFrom(begin));
        }
        if (token.attributes_.Size() >= limits_.maxAttributesPerElement) {
            return Failure(
                Base::ErrorCode::OutOfRange,
                XmlDiagnosticCodes::AttributeLimitExceeded,
                MessageAttributeLimit,
                SpanFrom(begin));
        }

        XmlAttribute attribute;
        const ::Aero::Diagnostics::SourcePosition attributeBegin = Position();
        Base::Result<void> attributeNameResult = ParseName(
            attribute.name_, attribute.nameSource_);
        if (!attributeNameResult) {
            return attributeNameResult.GetStatus();
        }

        for (const XmlAttribute& existing : token.attributes_) {
            if (existing.Name() == attribute.Name()) {
                return Failure(
                    Base::ErrorCode::AlreadyExists,
                    XmlDiagnosticCodes::DuplicateAttribute,
                    MessageDuplicateAttribute,
                    attribute.nameSource_);
            }
        }

        (void)SkipWhitespace();
        if (AtEnd() || input_.Data()[offset_] != '=') {
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed,
                SpanFrom(attributeBegin));
        }
        ConsumeAscii(1U);
        (void)SkipWhitespace();
        if (AtEnd() ||
            (input_.Data()[offset_] != '\'' && input_.Data()[offset_] != '"')) {
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed,
                SpanFrom(attributeBegin));
        }

        const char quote = input_.Data()[offset_];
        ConsumeAscii(1U);
        Base::Result<void> valueResult = ParseAttributeValue(
            quote,
            attribute.value_,
            attribute.valueSource_);
        if (!valueResult) {
            return valueResult.GetStatus();
        }
        attribute.source_ = SpanFrom(attributeBegin);

        token.attributes_.PushBack(
            std::move(attribute));
    }

    token.kind_ = XmlTokenKind::StartElement;
    token.source_ = SpanFrom(begin);
    rootSeen_ = true;

    if (token.emptyElement_) {
        if (openElements_.Empty()) {
            rootClosed_ = true;
        }
        return token.kind_;
    }

    Base::String openName;
    Base::Result<void> copyResult = openName.AssignUnchecked(token.Name());
    if (!copyResult) {
        return copyResult.GetStatus();
    }
    openElements_.PushBack(std::move(openName));
    return token.kind_;
}

Base::Result<XmlTokenKind> Utf8XmlTokenizer::ParseEndElement(
    XmlToken& token) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    ConsumeAscii(2U);

    Base::Result<void> nameResult = ParseName(token.name_, token.nameSource_);
    if (!nameResult) {
        return nameResult.GetStatus();
    }
    (void)SkipWhitespace();
    if (AtEnd()) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::UnexpectedEndOfInput,
            MessageUnexpectedEnd,
            SpanFrom(begin));
    }
    if (input_.Data()[offset_] != '>') {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed,
            SpanFrom(begin));
    }
    ConsumeAscii(1U);

    if (openElements_.Empty() || openElements_.Back().View() != token.Name()) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::MismatchedEndElement,
            MessageMismatchedEnd,
            SpanFrom(begin));
    }

    openElements_.PopBack();
    if (openElements_.Empty()) {
        rootClosed_ = true;
    }

    token.kind_ = XmlTokenKind::EndElement;
    token.source_ = SpanFrom(begin);
    return token.kind_;
}

Base::Result<XmlTokenKind> Utf8XmlTokenizer::ParseText(
    XmlToken& token) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();

    while (!AtEnd() && input_.Data()[offset_] != '<') {
        Base::Result<void> appendResult = input_.Data()[offset_] == '&'
            ? AppendEntity(token.text_)
            : AppendCurrentCodePoint(token.text_);
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        if (token.text_.SizeBytes() > limits_.maxTextBytes) {
            return Failure(
                Base::ErrorCode::OutOfRange,
                XmlDiagnosticCodes::TextLimitExceeded,
                MessageTextLimit,
                SpanFrom(begin));
        }
    }

    token.kind_ = XmlTokenKind::Text;
    token.source_ = SpanFrom(begin);
    return token.kind_;
}

Base::Result<XmlTokenKind> Utf8XmlTokenizer::ParseCdata(
    XmlToken& token) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    ConsumeAscii(9U);

    while (!AtEnd() && !StartsWith("]]>", 3U)) {
        Base::Result<void> appendResult = AppendCurrentCodePoint(token.text_);
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        if (token.text_.SizeBytes() > limits_.maxTextBytes) {
            return Failure(
                Base::ErrorCode::OutOfRange,
                XmlDiagnosticCodes::TextLimitExceeded,
                MessageTextLimit,
                SpanFrom(begin));
        }
    }

    if (AtEnd()) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::UnexpectedEndOfInput,
            MessageUnexpectedEnd,
            SpanFrom(begin));
    }
    ConsumeAscii(3U);

    token.kind_ = XmlTokenKind::Text;
    token.source_ = SpanFrom(begin);
    return token.kind_;
}

Base::Result<void> Utf8XmlTokenizer::ParseName(
    Base::String& name,
    ::Aero::Diagnostics::SourceSpan& source) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    const std::uint32_t startOffset = offset_;
    std::uint32_t colonCount = 0U;
    bool first = true;

    while (!AtEnd()) {
        const unsigned char current = static_cast<unsigned char>(
            input_.Data()[offset_]);
        const bool accepted = current >= 0x80U ||
            (first ? IsAsciiNameStart(current) : IsAsciiNameContinue(current));
        if (!accepted) {
            break;
        }

        if (current == static_cast<unsigned char>(':')) {
            ++colonCount;
        }
        AdvanceCodePoint();
        first = false;

        if (offset_ - startOffset > limits_.maxNameBytes) {
            return Failure(
                Base::ErrorCode::OutOfRange,
                XmlDiagnosticCodes::NameLimitExceeded,
                MessageNameLimit,
                SpanFrom(begin));
        }
    }

    const std::uint32_t byteCount = offset_ - startOffset;
    if (byteCount == 0U || colonCount > 1U ||
        input_.Data()[startOffset] == ':' ||
        input_.Data()[offset_ - 1U] == ':') {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed,
            SpanFrom(begin));
    }

    Base::Result<void> assignResult = name.AssignUnchecked(
        Base::StringView(input_.Data() + startOffset, byteCount));
    if (!assignResult) {
        return assignResult.GetStatus();
    }
    source = SpanFrom(begin);
    return {};
}

Base::Result<void> Utf8XmlTokenizer::ParseAttributeValue(
    char quote,
    Base::String& value,
    ::Aero::Diagnostics::SourceSpan& source) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();

    while (!AtEnd() && input_.Data()[offset_] != quote) {
        if (input_.Data()[offset_] == '<') {
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed,
                SpanFrom(begin));
        }

        Base::Result<void> appendResult = input_.Data()[offset_] == '&'
            ? AppendEntity(value)
            : AppendCurrentCodePoint(value, true);
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        if (value.SizeBytes() > limits_.maxTextBytes) {
            return Failure(
                Base::ErrorCode::OutOfRange,
                XmlDiagnosticCodes::TextLimitExceeded,
                MessageTextLimit,
                SpanFrom(begin));
        }
    }

    if (AtEnd()) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::UnexpectedEndOfInput,
            MessageUnexpectedEnd,
            SpanFrom(begin));
    }

    source = SpanFrom(begin);
    ConsumeAscii(1U);
    return {};
}

Base::Result<void> Utf8XmlTokenizer::AppendEntity(
    Base::String& output) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    ConsumeAscii(1U);
    if (AtEnd()) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::UnexpectedEndOfInput,
            MessageUnexpectedEnd,
            SpanFrom(begin));
    }

    if (input_.Data()[offset_] == '#') {
        ConsumeAscii(1U);
        bool hexadecimal = false;
        if (!AtEnd() &&
            (input_.Data()[offset_] == 'x' || input_.Data()[offset_] == 'X')) {
            hexadecimal = true;
            ConsumeAscii(1U);
        }

        std::uint32_t codePoint = 0U;
        std::uint32_t digitCount = 0U;
        while (!AtEnd()) {
            const char current = input_.Data()[offset_];
            const bool digit = hexadecimal ? IsHexDigit(current) : IsDecimalDigit(current);
            if (!digit) {
                break;
            }

            const std::uint32_t digitValue = hexadecimal
                ? HexValue(current)
                : static_cast<std::uint32_t>(current - '0');
            const std::uint32_t base = hexadecimal ? 16U : 10U;
            if (codePoint > (0x10FFFFU - digitValue) / base) {
                return Failure(
                    Base::ErrorCode::OutOfRange,
                    XmlDiagnosticCodes::InvalidXmlCharacter,
                    MessageInvalidCharacter,
                    SpanFrom(begin));
            }
            codePoint = codePoint * base + digitValue;
            ++digitCount;
            ConsumeAscii(1U);
        }

        if (digitCount == 0U || AtEnd() || input_.Data()[offset_] != ';') {
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed,
                SpanFrom(begin));
        }
        ConsumeAscii(1U);
        return AppendCodePoint(output, codePoint);
    }

    const std::uint32_t entityStart = offset_;
    while (!AtEnd() && input_.Data()[offset_] != ';') {
        const unsigned char current = static_cast<unsigned char>(
            input_.Data()[offset_]);
        if (!IsAsciiNameContinue(current) || current == static_cast<unsigned char>(':')) {
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed,
                SpanFrom(begin));
        }
        ConsumeAscii(1U);
    }

    if (AtEnd()) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::UnexpectedEndOfInput,
            MessageUnexpectedEnd,
            SpanFrom(begin));
    }

    const Base::StringView entity(
        input_.Data() + entityStart,
        offset_ - entityStart);
    ConsumeAscii(1U);

    if (Equals(entity, "lt", 2U)) {
        return output.AppendUnchecked(Base::StringView("<"));
    }
    if (Equals(entity, "gt", 2U)) {
        return output.AppendUnchecked(Base::StringView(">"));
    }
    if (Equals(entity, "amp", 3U)) {
        return output.AppendUnchecked(Base::StringView("&"));
    }
    if (Equals(entity, "quot", 4U)) {
        return output.AppendUnchecked(Base::StringView("\""));
    }
    if (Equals(entity, "apos", 4U)) {
        return output.AppendUnchecked(Base::StringView("'"));
    }

    return Failure(
        Base::ErrorCode::ValidationFailed,
        XmlDiagnosticCodes::UnknownEntity,
        MessageUnknownEntity,
        SpanFrom(begin));
}

Base::Result<void> Utf8XmlTokenizer::AppendCodePoint(
    Base::String& output,
    std::uint32_t codePoint) noexcept {
    if (!IsXmlCharacter(codePoint)) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::InvalidXmlCharacter,
            MessageInvalidCharacter,
            {Position(), Position()});
    }

    char bytes[4]{};
    const std::uint32_t length = Base::EncodeCodePoint(codePoint, bytes);
    return output.AppendUnchecked(Base::StringView(bytes, length));
}

Base::Result<void> Utf8XmlTokenizer::AppendCurrentCodePoint(
    Base::String& output,
    bool attributeValue) noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    const std::uint32_t codePoint = CurrentCodePoint();
    const std::uint32_t length = CurrentCodePointLength();
    if (!IsXmlCharacter(codePoint)) {
        AdvanceCodePoint();
        return Failure(
            Base::ErrorCode::ValidationFailed,
            XmlDiagnosticCodes::InvalidXmlCharacter,
            MessageInvalidCharacter,
            SpanFrom(begin));
    }

    if (codePoint == 0xDU) {
        Base::Result<void> appendResult = output.AppendUnchecked(
            attributeValue ? Base::StringView(" ") : Base::StringView("\n"));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        AdvanceCodePoint();
        return {};
    }
    if (attributeValue && (codePoint == 0x9U || codePoint == 0xAU)) {
        Base::Result<void> appendResult = output.AppendUnchecked(
            Base::StringView(" "));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        AdvanceCodePoint();
        return {};
    }

    Base::Result<void> appendResult = output.AppendUnchecked(
        Base::StringView(input_.Data() + offset_, length));
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    AdvanceCodePoint();
    return {};
}

Base::Result<void> Utf8XmlTokenizer::SkipComment() noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    ConsumeAscii(4U);

    while (!AtEnd()) {
        if (StartsWith("-->", 3U)) {
            ConsumeAscii(3U);
            return {};
        }
        if (StartsWith("--", 2U)) {
            ConsumeAscii(2U);
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed,
                SpanFrom(begin));
        }
        const std::uint32_t codePoint = CurrentCodePoint();
        if (!IsXmlCharacter(codePoint)) {
            AdvanceCodePoint();
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::InvalidXmlCharacter,
                MessageInvalidCharacter,
                SpanFrom(begin));
        }
        AdvanceCodePoint();
    }

    return Failure(
        Base::ErrorCode::ValidationFailed,
        XmlDiagnosticCodes::UnexpectedEndOfInput,
        MessageUnexpectedEnd,
        SpanFrom(begin));
}

Base::Result<void> Utf8XmlTokenizer::SkipProcessingInstruction() noexcept {
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    ConsumeAscii(2U);

    while (!AtEnd()) {
        if (StartsWith("?>", 2U)) {
            ConsumeAscii(2U);
            return {};
        }
        const std::uint32_t codePoint = CurrentCodePoint();
        if (!IsXmlCharacter(codePoint)) {
            AdvanceCodePoint();
            return Failure(
                Base::ErrorCode::ValidationFailed,
                XmlDiagnosticCodes::InvalidXmlCharacter,
                MessageInvalidCharacter,
                SpanFrom(begin));
        }
        AdvanceCodePoint();
    }

    return Failure(
        Base::ErrorCode::ValidationFailed,
        XmlDiagnosticCodes::UnexpectedEndOfInput,
        MessageUnexpectedEnd,
        SpanFrom(begin));
}

Base::Status Utf8XmlTokenizer::Failure(
    Base::ErrorCode error,
    ::Aero::Diagnostics::DiagnosticCode diagnostic,
    Base::StringView message,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> item = ::Aero::Diagnostics::Diagnostic::Create(
            diagnostic,
            ::Aero::Diagnostics::DiagnosticSeverity::Error,
            message,
            source,
            ::Aero::Diagnostics::InvalidDiagnosticObjectId,
            Meta::InvalidMemberId);
        if (!item) {
            return item.GetStatus();
        }

        Base::Result<void> reportResult = diagnostics_->Report(
            std::move(item).Value());
        if (!reportResult) {
            return reportResult.GetStatus();
        }
    }

    return Base::Status::Failure(error, message.Data());
}

} // namespace Aero::Markup


// ===== NodeReader =====


// Canonical markup node reader implementation.


namespace Aero::Markup {
namespace {

constexpr Base::StringView XmlPrefix("xml");
constexpr Base::StringView XmlNamespaceUri(
    "http://www.w3.org/XML/1998/namespace");
constexpr Base::StringView XmlnsPrefix("xmlns");
constexpr Base::StringView XmlnsNamespaceUri(
    "http://www.w3.org/2000/xmlns/");
constexpr Base::StringView MarkupCompatibilityNamespaceUri(
    "http://schemas.openxmlformats.org/markup-compatibility/2006");

constexpr Base::StringView MessageUnboundPrefix(
    "XAML qualified name uses an unbound XML namespace prefix");
constexpr Base::StringView MessageInvalidNamespace(
    "XML namespace declaration is invalid for the XAML node stream");
constexpr Base::StringView MessageDuplicateNamespace(
    "XML element declares the same namespace prefix more than once");
constexpr Base::StringView MessageInvalidState(
    "XAML node reader encountered an invalid tokenizer or scope state");
constexpr Base::StringView MessageReaderFailed(
    "XAML node reader cannot continue after a conversion failure");

bool StartsWith(
    Base::StringView value,
    const char* literal,
    std::uint32_t literalLength) noexcept {
    return value.SizeBytes() >= literalLength &&
        (literalLength == 0U ||
            std::memcmp(value.Data(), literal, literalLength) == 0);
}

} // namespace

void QualifiedName::Clear() noexcept {
    prefix_.Clear();
    localName_.Clear();
    namespaceUri_.Clear();
}

void Node::Clear() noexcept {
    kind_ = NodeKind::None;
    name_.Clear();
    namespacePrefix_.Clear();
    namespaceUri_.Clear();
    value_.Clear();
    source_ = {};
    fromAttribute_ = false;
    compiledTypeId_ = Meta::InvalidTypeId;
    compiledTypeBinding_ = {};
    compiledMemberId_ = Meta::InvalidMemberId;
    compiledMemberBinding_ = {};
    compiledValue_ = Meta::Value{};
}

Base::Result<Node> Node::Clone(
    const Node& source) noexcept {
    Node clone;
    clone.kind_ = source.kind_;
    clone.source_ = source.source_;
    clone.fromAttribute_ = source.fromAttribute_;
    clone.compiledTypeId_ = source.compiledTypeId_;
    clone.compiledTypeBinding_ =
        source.compiledTypeBinding_;
    clone.compiledMemberId_ = source.compiledMemberId_;
    clone.compiledMemberBinding_ =
        source.compiledMemberBinding_;
    clone.compiledValue_ = source.compiledValue_;
    Base::Result<void> copied =
        clone.name_.prefix_.Assign(
            source.name_.prefix_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.name_.localName_.Assign(
        source.name_.localName_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.name_.namespaceUri_.Assign(
        source.name_.namespaceUri_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.namespacePrefix_.Assign(
        source.namespacePrefix_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.namespaceUri_.Assign(
        source.namespaceUri_.View());
    if (!copied) return copied.GetStatus();
    copied = clone.value_.Assign(source.value_.View());
    if (!copied) return copied.GetStatus();
    return clone;
}

NodeReader::NodeReader(
    IXmlTokenizer& tokenizer,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept
    : tokenizer_(&tokenizer),
      diagnostics_(diagnostics),
      xmlToken_(),
      pending_(),
      bindings_(),
      scopes_() {}

void NodeReader::Reset() noexcept {
    xmlToken_.Clear();
    pending_.Clear();
    bindings_.Clear();
    ignorableNamespaces_.Clear();
    scopes_.Clear();
    pendingIndex_ = 0U;
    ended_ = false;
    failed_ = false;
}

Base::Result<NodeKind> NodeReader::Read(Node& node) noexcept {
    if (failed_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageReaderFailed.Data());
    }

    if (pendingIndex_ < pending_.Size()) {
        return EmitPending(node);
    }

    pending_.Clear();
    pendingIndex_ = 0U;

    if (ended_) {
        node.Clear();
        node.kind_ = NodeKind::EndOfDocument;
        return node.kind_;
    }

    Base::Result<XmlTokenKind> tokenResult = tokenizer_->Read(xmlToken_);
    if (!tokenResult) {
        failed_ = true;
        return tokenResult.GetStatus();
    }

    Base::Result<void> queueResult;
    switch (tokenResult.Value()) {
    case XmlTokenKind::StartElement:
        queueResult = QueueStartElement(xmlToken_);
        break;
    case XmlTokenKind::EndElement:
        queueResult = QueueEndElement(xmlToken_);
        break;
    case XmlTokenKind::Text:
        queueResult = QueueText(xmlToken_);
        break;
    case XmlTokenKind::EndOfDocument:
        queueResult = QueueEndOfDocument(xmlToken_);
        break;
    case XmlTokenKind::None:
        queueResult = Failure(
            Base::ErrorCode::InvalidState,
            NodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            xmlToken_.Source());
        break;
    }

    if (!queueResult) {
        failed_ = true;
        return queueResult.GetStatus();
    }
    if (pending_.Empty()) {
        // Ignorable markup-compatibility nodes and their whitespace produce
        // no XAML nodes. Continue until an observable node is available.
        return Read(node);
    }

    return EmitPending(node);
}

Base::Result<void> NodeReader::QueueStartElement(
    const XmlToken& token) noexcept {
    const std::uint32_t bindingStart = bindings_.Size();
    const std::uint32_t ignorableNamespaceStart =
        ignorableNamespaces_.Size();

    for (const XmlAttribute& attribute : token.Attributes()) {
        Base::StringView prefix;
        if (!IsNamespaceDeclaration(attribute.Name(), prefix)) {
            continue;
        }

        Base::Result<void> bindingResult = AddNamespaceBinding(
            prefix,
            attribute.Value(),
            bindingStart,
            attribute.Source());
        if (!bindingResult) {
            return bindingResult.GetStatus();
        }
    }

    for (const XmlAttribute& attribute : token.Attributes()) {
        Base::StringView prefix;
        if (!IsNamespaceDeclaration(attribute.Name(), prefix)) {
            continue;
        }

        Base::Result<void> namespaceResult = QueueNamespaceDeclaration(
            prefix,
            attribute.Value(),
            attribute.Source());
        if (!namespaceResult) {
            return namespaceResult.GetStatus();
        }
    }

    for (const XmlAttribute& attribute : token.Attributes()) {
        Base::StringView prefix;
        if (IsNamespaceDeclaration(attribute.Name(), prefix) ||
            !IsMarkupCompatibilityIgnorable(attribute)) {
            continue;
        }
        Base::Result<void> ignored = AddIgnorableNamespaces(
            attribute.Value(), attribute.Source());
        if (!ignored) return ignored.GetStatus();
    }

    ScopeFrame frame;
    frame.bindingStart = bindingStart;
    frame.ignorableNamespaceStart = ignorableNamespaceStart;
    Base::Result<void> resolveObject = ResolveQualifiedName(
        token.Name(),
        true,
        token.NameSource(),
        frame.objectName);
    if (!resolveObject) {
        return resolveObject.GetStatus();
    }

    frame.ignored = IsIgnorableNamespace(
        frame.objectName.NamespaceUri()) ||
        (!scopes_.Empty() && scopes_.Back().ignored);

    if (!frame.ignored) {
        Base::Result<void> startResult = QueueObjectNode(
            NodeKind::StartObject,
            frame.objectName,
            token.Source());
        if (!startResult) {
            return startResult.GetStatus();
        }
    }

    if (!frame.ignored) {
        for (const XmlAttribute& attribute : token.Attributes()) {
            Base::StringView prefix;
            if (IsNamespaceDeclaration(attribute.Name(), prefix) ||
                IsMarkupCompatibilityIgnorable(attribute)) {
                continue;
            }

            QualifiedName attributeName;
            Base::Result<void> resolved = ResolveQualifiedName(
                attribute.Name(), false, attribute.NameSource(), attributeName);
            if (!resolved) return resolved.GetStatus();
            if (IsIgnorableNamespace(attributeName.NamespaceUri())) {
                continue;
            }

            Base::Result<void> memberResult = QueueMemberNodes(attribute);
            if (!memberResult) {
                return memberResult.GetStatus();
            }
        }
    }

    if (token.IsEmptyElement()) {
        Base::Result<void> endResult;
        if (!frame.ignored) {
            endResult = QueueObjectNode(
                NodeKind::EndObject,
                frame.objectName,
                token.Source());
        }
        PopBindings(bindingStart);
        PopIgnorableNamespaces(ignorableNamespaceStart);
        return endResult;
    }

    scopes_.PushBack(std::move(frame));
    return {};
}

Base::Result<void> NodeReader::QueueEndElement(
    const XmlToken& token) noexcept {
    if (scopes_.Empty()) {
        return Failure(
            Base::ErrorCode::InvalidState,
            NodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            token.Source());
    }

    ScopeFrame& frame = scopes_.Back();
    Base::Result<void> endResult;
    if (!frame.ignored) {
        endResult = QueueObjectNode(
            NodeKind::EndObject,
            frame.objectName,
            token.Source());
    }
    if (!endResult) {
        return endResult.GetStatus();
    }

    const std::uint32_t bindingStart = frame.bindingStart;
    const std::uint32_t ignorableNamespaceStart =
        frame.ignorableNamespaceStart;
    scopes_.PopBack();
    PopBindings(bindingStart);
    PopIgnorableNamespaces(ignorableNamespaceStart);
    return {};
}

Base::Result<void> NodeReader::QueueText(
    const XmlToken& token) noexcept {
    if (!scopes_.Empty() && scopes_.Back().ignored) return {};
    Node node;
    node.kind_ = NodeKind::Value;
    node.source_ = token.Source();
    node.fromAttribute_ = false;
    Base::Result<void> assignResult = node.value_.AssignUnchecked(token.Text());
    if (!assignResult) {
        return assignResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> NodeReader::QueueEndOfDocument(
    const XmlToken& token) noexcept {
    if (!scopes_.Empty() || !bindings_.Empty()) {
        return Failure(
            Base::ErrorCode::InvalidState,
            NodeDiagnosticCodes::InvalidNodeStreamState,
            MessageInvalidState,
            token.Source());
    }

    Node node;
    node.kind_ = NodeKind::EndOfDocument;
    node.source_ = token.Source();
    return AppendPending(std::move(node));
}

Base::Result<void> NodeReader::QueueNamespaceDeclaration(
    Base::StringView prefix,
    Base::StringView uri,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Node node;
    node.kind_ = NodeKind::NamespaceDeclaration;
    node.source_ = source;

    Base::Result<void> prefixResult = node.namespacePrefix_.AssignUnchecked(prefix);
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = node.namespaceUri_.AssignUnchecked(uri);
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> NodeReader::QueueObjectNode(
    NodeKind kind,
    const QualifiedName& name,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Node node;
    node.kind_ = kind;
    node.source_ = source;
    Base::Result<void> copyResult = CopyQualifiedName(name, node.name_);
    if (!copyResult) {
        return copyResult.GetStatus();
    }
    return AppendPending(std::move(node));
}

Base::Result<void> NodeReader::QueueMemberNodes(
    const XmlAttribute& attribute) noexcept {
    QualifiedName memberName;
    Base::Result<void> resolveResult = ResolveQualifiedName(
        attribute.Name(),
        false,
        attribute.NameSource(),
        memberName);
    if (!resolveResult) {
        return resolveResult.GetStatus();
    }

    Node start;
    start.kind_ = NodeKind::StartMember;
    start.source_ = attribute.NameSource();
    start.fromAttribute_ = true;
    Base::Result<void> startNameResult = CopyQualifiedName(memberName, start.name_);
    if (!startNameResult) {
        return startNameResult.GetStatus();
    }
    Base::Result<void> appendStart = AppendPending(std::move(start));
    if (!appendStart) {
        return appendStart.GetStatus();
    }

    Node value;
    value.kind_ = NodeKind::Value;
    value.source_ = attribute.ValueSource();
    value.fromAttribute_ = true;
    Base::Result<void> valueResult = value.value_.AssignUnchecked(attribute.Value());
    if (!valueResult) {
        return valueResult.GetStatus();
    }
    Base::Result<void> appendValue = AppendPending(std::move(value));
    if (!appendValue) {
        return appendValue.GetStatus();
    }

    Node end;
    end.kind_ = NodeKind::EndMember;
    end.source_ = attribute.NameSource();
    end.fromAttribute_ = true;
    Base::Result<void> endNameResult = CopyQualifiedName(memberName, end.name_);
    if (!endNameResult) {
        return endNameResult.GetStatus();
    }
    return AppendPending(std::move(end));
}

Base::Result<void> NodeReader::AppendPending(Node&& node) noexcept {
    pending_.PushBack(std::move(node));
    return {};
}

Base::Result<void> NodeReader::AddNamespaceBinding(
    Base::StringView prefix,
    Base::StringView uri,
    std::uint32_t bindingStart,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (prefix == XmlnsPrefix || uri == XmlnsNamespaceUri ||
        (!prefix.Empty() && uri.Empty()) ||
        (prefix == XmlPrefix && uri != XmlNamespaceUri) ||
        (prefix != XmlPrefix && uri == XmlNamespaceUri)) {
        return Failure(
            Base::ErrorCode::ValidationFailed,
            NodeDiagnosticCodes::InvalidNamespaceDeclaration,
            MessageInvalidNamespace,
            source);
    }

    for (std::uint32_t index = bindingStart; index < bindings_.Size(); ++index) {
        if (bindings_[index].prefix.View() == prefix) {
            return Failure(
                Base::ErrorCode::AlreadyExists,
                NodeDiagnosticCodes::DuplicateNamespacePrefix,
                MessageDuplicateNamespace,
                source);
        }
    }

    NamespaceBinding binding;
    Base::Result<void> prefixResult = binding.prefix.AssignUnchecked(prefix);
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = binding.uri.AssignUnchecked(uri);
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    bindings_.PushBack(std::move(binding));
    return {};
}

Base::Result<void> NodeReader::AddIgnorableNamespaces(
    Base::StringView prefixes,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    std::uint32_t begin = 0U;
    while (begin < prefixes.SizeBytes()) {
        while (begin < prefixes.SizeBytes() &&
               (prefixes[begin] == ' ' || prefixes[begin] == '\t' ||
                prefixes[begin] == '\r' || prefixes[begin] == '\n')) {
            ++begin;
        }
        const std::uint32_t tokenBegin = begin;
        while (begin < prefixes.SizeBytes() &&
               prefixes[begin] != ' ' && prefixes[begin] != '\t' &&
               prefixes[begin] != '\r' && prefixes[begin] != '\n') {
            ++begin;
        }
        if (tokenBegin == begin) continue;
        bool found = false;
        const Base::StringView uri = LookupNamespace(
            prefixes.Substr(tokenBegin, begin - tokenBegin), found);
        if (!found || uri.Empty()) {
            return Failure(
                Base::ErrorCode::NotFound,
                NodeDiagnosticCodes::UnboundNamespacePrefix,
                MessageUnboundPrefix, source);
        }
        Base::String copied;
        Base::Result<void> assigned = copied.Assign(uri);
        if (!assigned) return assigned.GetStatus();
        ignorableNamespaces_.PushBack(
            std::move(copied));
    }
    return {};
}

Base::Result<void> NodeReader::ResolveQualifiedName(
    Base::StringView qualifiedName,
    bool useDefaultNamespace,
    ::Aero::Diagnostics::SourceSpan source,
    QualifiedName& output) noexcept {
    std::uint32_t colon = qualifiedName.SizeBytes();
    for (std::uint32_t index = 0U; index < qualifiedName.SizeBytes(); ++index) {
        if (qualifiedName[index] == ':') {
            colon = index;
            break;
        }
    }

    const bool hasPrefix = colon < qualifiedName.SizeBytes();
    const Base::StringView prefix = hasPrefix
        ? qualifiedName.Substr(0U, colon)
        : Base::StringView();
    const Base::StringView localName = hasPrefix
        ? qualifiedName.Substr(
            colon + 1U,
            qualifiedName.SizeBytes() - colon - 1U)
        : qualifiedName;

    bool found = false;
    Base::StringView namespaceUri;
    if (hasPrefix) {
        namespaceUri = LookupNamespace(prefix, found);
        if (!found) {
            return Failure(
                Base::ErrorCode::NotFound,
                NodeDiagnosticCodes::UnboundNamespacePrefix,
                MessageUnboundPrefix,
                source);
        }
    } else if (useDefaultNamespace) {
        namespaceUri = LookupNamespace({}, found);
        if (!found) {
            namespaceUri = {};
        }
    }

    output.Clear();
    Base::Result<void> prefixResult = output.prefix_.AssignUnchecked(prefix);
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> localResult = output.localName_.AssignUnchecked(localName);
    if (!localResult) {
        return localResult.GetStatus();
    }
    Base::Result<void> uriResult = output.namespaceUri_.AssignUnchecked(namespaceUri);
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    return {};
}

Base::Result<void> NodeReader::CopyQualifiedName(
    const QualifiedName& source,
    QualifiedName& output) noexcept {
    output.Clear();
    Base::Result<void> prefixResult = output.prefix_.AssignUnchecked(source.Prefix());
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> localResult = output.localName_.AssignUnchecked(source.LocalName());
    if (!localResult) {
        return localResult.GetStatus();
    }
    Base::Result<void> uriResult = output.namespaceUri_.AssignUnchecked(source.NamespaceUri());
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    return {};
}

Base::StringView NodeReader::LookupNamespace(
    Base::StringView prefix,
    bool& found) const noexcept {
    if (prefix == XmlPrefix) {
        found = true;
        return XmlNamespaceUri;
    }

    for (std::uint32_t index = bindings_.Size(); index > 0U; --index) {
        const NamespaceBinding& binding = bindings_[index - 1U];
        if (binding.prefix.View() == prefix) {
            found = true;
            return binding.uri.View();
        }
    }

    found = false;
    return {};
}

bool NodeReader::IsNamespaceDeclaration(
    Base::StringView attributeName,
    Base::StringView& prefix) const noexcept {
    if (attributeName == XmlnsPrefix) {
        prefix = {};
        return true;
    }
    if (StartsWith(attributeName, "xmlns:", 6U) &&
        attributeName.SizeBytes() > 6U) {
        prefix = attributeName.Substr(6U, attributeName.SizeBytes() - 6U);
        return true;
    }
    return false;
}

bool NodeReader::IsIgnorableNamespace(
    Base::StringView uri) const noexcept {
    for (const Base::String& ignored : ignorableNamespaces_) {
        if (ignored.View() == uri) return true;
    }
    return false;
}

bool NodeReader::IsMarkupCompatibilityIgnorable(
    const XmlAttribute& attribute) const noexcept {
    QualifiedName name;
    Base::Result<void> resolved = const_cast<NodeReader*>(this)
        ->ResolveQualifiedName(
            attribute.Name(), false, attribute.NameSource(), name);
    return resolved &&
        name.NamespaceUri() == MarkupCompatibilityNamespaceUri &&
        name.LocalName() == Base::StringView("Ignorable");
}

void NodeReader::PopBindings(std::uint32_t bindingStart) noexcept {
    while (bindings_.Size() > bindingStart) {
        bindings_.PopBack();
    }
}

void NodeReader::PopIgnorableNamespaces(std::uint32_t start) noexcept {
    while (ignorableNamespaces_.Size() > start) {
        ignorableNamespaces_.PopBack();
    }
}

Base::Result<NodeKind> NodeReader::EmitPending(
    Node& node) noexcept {
    if (pendingIndex_ >= pending_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageInvalidState.Data());
    }

    node = std::move(pending_[pendingIndex_]);
    ++pendingIndex_;
    if (node.Kind() == NodeKind::EndOfDocument) {
        ended_ = true;
    }
    return node.Kind();
}

Base::Status NodeReader::Failure(
    Base::ErrorCode error,
    ::Aero::Diagnostics::DiagnosticCode diagnostic,
    Base::StringView message,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> item = ::Aero::Diagnostics::Diagnostic::Create(
            diagnostic,
            ::Aero::Diagnostics::DiagnosticSeverity::Error,
            message,
            source,
            ::Aero::Diagnostics::InvalidDiagnosticObjectId,
            Meta::InvalidMemberId);
        if (!item) {
            return item.GetStatus();
        }

        Base::Result<void> reportResult = diagnostics_->Report(
            std::move(item).Value());
        if (!reportResult) {
            return reportResult.GetStatus();
        }
    }
    return Base::Status::Failure(error, message.Data());
}

} // namespace Aero::Markup

#if AERO_WITH_EXPAT
// ===== ExpatXmlTokenizer =====

// Optional Expat tokenizer backend.



namespace Aero::Markup {
namespace {

constexpr Base::StringView ExpatMessageMalformed(
    "Expat rejected malformed XML");
constexpr Base::StringView MessageUnsupported(
    "DTD and external entities are disabled");
constexpr Base::StringView MessageLimit(
    "XML input exceeds configured tokenizer limits");

} // namespace

ExpatXmlTokenizer::ExpatXmlTokenizer(
    XmlTokenizerLimits limits) noexcept
    : limits_(limits) {}

ExpatXmlTokenizer::~ExpatXmlTokenizer() noexcept {
    if (parser_ != nullptr) {
        XML_ParserFree(
            static_cast<XML_Parser>(parser_));
        parser_ = nullptr;
    }
}

::Aero::Diagnostics::SourcePosition
ExpatXmlTokenizer::Position() const noexcept {
    if (parser_ == nullptr) return {};
    XML_Parser parser =
        static_cast<XML_Parser>(parser_);
    const XML_Size line =
        XML_GetCurrentLineNumber(parser);
    const XML_Size column =
        XML_GetCurrentColumnNumber(parser);
    const XML_Index offset =
        XML_GetCurrentByteIndex(parser);
    return {
        static_cast<std::uint32_t>(line),
        static_cast<std::uint32_t>(column + 1U),
        offset >= 0
            ? static_cast<std::uint64_t>(offset)
            : 0U};
}

void ExpatXmlTokenizer::Stop(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode diagnosticCode,
    Base::StringView message) noexcept {
    if (!failure_.IsOk()) return;
    failure_ = status;
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> diagnostic =
            ::Aero::Diagnostics::Diagnostic::Create(
                diagnosticCode,
                ::Aero::Diagnostics::DiagnosticSeverity::Error,
                message,
                {Position(), Position()});
        if (diagnostic) {
            diagnostics_->Report(
                std::move(diagnostic).Value());
        }
    }
    if (parser_ != nullptr) {
        XML_StopParser(
            static_cast<XML_Parser>(parser_),
            XML_FALSE);
    }
}

Base::Result<void> ExpatXmlTokenizer::PushToken(
    XmlToken&& token,
    std::uint32_t depth) noexcept {
    tokens_.PushBack(std::move(token));
    tokenDepths_.PushBack(depth);
    return {};
}

void ExpatXmlTokenizer::HandleStart(
    const char* name,
    const char** attributes) noexcept {
    if (!failure_.IsOk()) return;
    const std::uint32_t nameBytes =
        static_cast<std::uint32_t>(
            std::strlen(name));
    if (nameBytes == 0U ||
        nameBytes > limits_.maxNameBytes ||
        parseDepth_ >= limits_.maxDepth) {
        Stop(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageLimit.Data()),
            parseDepth_ >= limits_.maxDepth
                ? XmlDiagnosticCodes::DepthLimitExceeded
                : XmlDiagnosticCodes::NameLimitExceeded,
            MessageLimit);
        return;
    }

    std::uint32_t attributeCount = 0U;
    while (attributes != nullptr &&
           attributes[attributeCount * 2U] != nullptr) {
        ++attributeCount;
    }
    if (attributeCount >
        limits_.maxAttributesPerElement) {
        Stop(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageLimit.Data()),
            XmlDiagnosticCodes::AttributeLimitExceeded,
            MessageLimit);
        return;
    }

    XmlToken token;
    token.kind_ = XmlTokenKind::StartElement;
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    token.source_ = {begin, begin};
    token.nameSource_ = {begin, begin};
    Base::Result<void> assigned =
        token.name_.Assign(
            Base::StringView(name, nameBytes));
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
        return;
    }
    token.attributes_.Reserve(
        attributeCount);
    for (std::uint32_t index = 0U;
         index < attributeCount; ++index) {
        const char* attributeName =
            attributes[index * 2U];
        const char* attributeValue =
            attributes[index * 2U + 1U];
        const std::uint32_t attributeNameBytes =
            static_cast<std::uint32_t>(
                std::strlen(attributeName));
        const std::uint32_t valueBytes =
            static_cast<std::uint32_t>(
                std::strlen(attributeValue));
        if (attributeNameBytes == 0U ||
            attributeNameBytes >
                limits_.maxNameBytes ||
            valueBytes > limits_.maxTextBytes) {
            Stop(
                Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    MessageLimit.Data()),
                attributeNameBytes >
                    limits_.maxNameBytes
                    ? XmlDiagnosticCodes::NameLimitExceeded
                    : XmlDiagnosticCodes::TextLimitExceeded,
                MessageLimit);
            return;
        }
        XmlAttribute attribute;
        assigned = attribute.name_.Assign(
            Base::StringView(
                attributeName,
                attributeNameBytes));
        if (assigned) {
            assigned = attribute.value_.Assign(
                Base::StringView(
                    attributeValue,
                    valueBytes));
        }
        if (assigned) {
            attribute.source_ = {begin, begin};
            attribute.nameSource_ = {begin, begin};
            attribute.valueSource_ = {begin, begin};
            token.attributes_.PushBack(
                std::move(attribute));
        } else {
            Stop(
                assigned.GetStatus(),
                XmlDiagnosticCodes::MalformedMarkup,
                ExpatMessageMalformed);
            return;
        }
    }
    ++parseDepth_;
    Base::Result<void> pushed =
        PushToken(
            std::move(token), parseDepth_);
    if (!pushed) {
        Stop(
            pushed.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
    }
}

void ExpatXmlTokenizer::HandleEnd(
    const char* name) noexcept {
    if (!failure_.IsOk()) return;
    if (parseDepth_ == 0U) {
        Stop(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                ExpatMessageMalformed.Data()),
            XmlDiagnosticCodes::MismatchedEndElement,
            ExpatMessageMalformed);
        return;
    }
    XmlToken token;
    token.kind_ = XmlTokenKind::EndElement;
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    token.source_ = {begin, begin};
    token.nameSource_ = {begin, begin};
    Base::Result<void> assigned =
        token.name_.Assign(
            Base::StringView(
                name,
                static_cast<std::uint32_t>(
                    std::strlen(name))));
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
        return;
    }
    --parseDepth_;
    Base::Result<void> pushed =
        PushToken(
            std::move(token), parseDepth_);
    if (!pushed) {
        Stop(
            pushed.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
    }
}

void ExpatXmlTokenizer::HandleText(
    const char* text,
    int length) noexcept {
    if (!failure_.IsOk() || length <= 0) return;
    const std::uint32_t size =
        static_cast<std::uint32_t>(length);
    if (size > limits_.maxTextBytes) {
        Stop(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageLimit.Data()),
            XmlDiagnosticCodes::TextLimitExceeded,
            MessageLimit);
        return;
    }
    if (!tokens_.Empty() &&
        tokens_.Back().kind_ ==
            XmlTokenKind::Text &&
        tokenDepths_.Back() == parseDepth_) {
        if (tokens_.Back().text_.SizeBytes() >
            limits_.maxTextBytes - size) {
            Stop(
                Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    MessageLimit.Data()),
                XmlDiagnosticCodes::TextLimitExceeded,
                MessageLimit);
            return;
        }
        Base::Result<void> appended =
            tokens_.Back().text_.Append(
                Base::StringView(text, size));
        if (!appended) {
            Stop(
                appended.GetStatus(),
                XmlDiagnosticCodes::MalformedMarkup,
                ExpatMessageMalformed);
        }
        return;
    }
    XmlToken token;
    token.kind_ = XmlTokenKind::Text;
    const ::Aero::Diagnostics::SourcePosition begin = Position();
    token.source_ = {begin, begin};
    Base::Result<void> assigned =
        token.text_.Assign(
            Base::StringView(text, size));
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
        return;
    }
    Base::Result<void> pushed =
        PushToken(
            std::move(token), parseDepth_);
    if (!pushed) {
        Stop(
            pushed.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
    }
}

void ExpatXmlTokenizer::RejectDeclaration() noexcept {
    Stop(
        Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageUnsupported.Data()),
        XmlDiagnosticCodes::UnsupportedDeclaration,
        MessageUnsupported);
}

int ExpatXmlTokenizer::RejectExternalEntity() noexcept {
    RejectDeclaration();
    return XML_STATUS_ERROR;
}

Base::Result<void> ExpatXmlTokenizer::InitializeParser(
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    diagnostics_ = diagnostics;
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (parser == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Expat parser allocation failed");
    }
    parser_ = parser;
    XML_SetUserData(parser, this);
    XML_SetElementHandler(
        parser,
        [](void* user, const XML_Char* name,
           const XML_Char** attributes) {
            static_cast<ExpatXmlTokenizer*>(user)->
                HandleStart(name, attributes);
        },
        [](void* user, const XML_Char* name) {
            static_cast<ExpatXmlTokenizer*>(user)->
                HandleEnd(name);
        });
    XML_SetCharacterDataHandler(
        parser,
        [](void* user, const XML_Char* text,
           int length) {
            static_cast<ExpatXmlTokenizer*>(user)->
                HandleText(text, length);
        });
    XML_SetStartDoctypeDeclHandler(
        parser,
        [](void* user, const XML_Char*,
           const XML_Char*, const XML_Char*, int) {
            static_cast<ExpatXmlTokenizer*>(user)->
                RejectDeclaration();
        });
    XML_SetExternalEntityRefHandler(
        parser,
        [](XML_Parser parserValue,
           const XML_Char*, const XML_Char*,
           const XML_Char*, const XML_Char*) -> int {
            void* user = XML_GetUserData(parserValue);
            return static_cast<ExpatXmlTokenizer*>(user)->
                RejectExternalEntity();
        });
    XML_SetParamEntityParsing(
        parser,
        XML_PARAM_ENTITY_PARSING_NEVER);
    return {};
}

Base::Result<void> ExpatXmlTokenizer::Reset(
    Base::StringView utf8,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (parser_ != nullptr) {
        XML_ParserFree(
            static_cast<XML_Parser>(parser_));
        parser_ = nullptr;
    }
    tokens_.Clear();
    tokenDepths_.Clear();
    diagnostics_ = diagnostics;
    failure_ = {};
    readIndex_ = 0U;
    parseDepth_ = 0U;
    depth_ = 0U;
    stream_ = nullptr;
    streamBytes_ = 0U;
    streamEof_ = false;
    streamMode_ = false;
    initialized_ = false;

    if (limits_.maxInputBytes == 0U ||
        limits_.maxDepth == 0U ||
        limits_.maxAttributesPerElement == 0U ||
        limits_.maxNameBytes == 0U ||
        limits_.maxTextBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Expat tokenizer limits must be positive");
    }
    if (utf8.SizeBytes() > limits_.maxInputBytes ||
        utf8.SizeBytes() >
            static_cast<std::uint32_t>(INT_MAX)) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XML input exceeds configured tokenizer limits");
    }

    Base::Result<void> parserInitialized =
        InitializeParser(diagnostics);
    if (!parserInitialized) return parserInitialized.GetStatus();

    const enum XML_Status parsed = XML_Parse(
        static_cast<XML_Parser>(parser_),
        utf8.Data(),
        static_cast<int>(utf8.SizeBytes()),
        XML_TRUE);
    if (!failure_.IsOk()) {
        return failure_;
    }
    if (parsed != XML_STATUS_OK ||
        parseDepth_ != 0U) {
        Stop(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                ExpatMessageMalformed.Data()),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
        return failure_;
    }
    XmlToken end;
    end.kind_ = XmlTokenKind::EndOfDocument;
    const ::Aero::Diagnostics::SourcePosition position = Position();
    end.source_ = {position, position};
    Base::Result<void> stored =
        PushToken(std::move(end), 0U);
    if (!stored) return stored.GetStatus();
    initialized_ = true;
    return {};
}

Base::Result<void> ExpatXmlTokenizer::Reset(
    Base::Stream& stream,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    if (parser_ != nullptr) {
        XML_ParserFree(
            static_cast<XML_Parser>(parser_));
        parser_ = nullptr;
    }
    tokens_.Clear();
    tokenDepths_.Clear();
    diagnostics_ = diagnostics;
    failure_ = {};
    readIndex_ = 0U;
    parseDepth_ = 0U;
    depth_ = 0U;
    stream_ = &stream;
    streamBytes_ = 0U;
    streamEof_ = false;
    streamMode_ = true;
    initialized_ = false;

    if (limits_.maxInputBytes == 0U ||
        limits_.maxDepth == 0U ||
        limits_.maxAttributesPerElement == 0U ||
        limits_.maxNameBytes == 0U ||
        limits_.maxTextBytes == 0U) {
        stream_ = nullptr;
        streamMode_ = false;
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Expat tokenizer limits must be positive");
    }
    Base::Result<void> parserInitialized =
        InitializeParser(diagnostics);
    if (!parserInitialized) {
        stream_ = nullptr;
        streamMode_ = false;
        return parserInitialized.GetStatus();
    }
    initialized_ = true;
    return {};
}

Base::Result<XmlTokenKind> ExpatXmlTokenizer::ReadStream(
    XmlToken& token) noexcept {
    for (;;) {
        if (readIndex_ < tokens_.Size()) {
            // Expat may split one XML character-data section at an input
            // buffer boundary. Keep the trailing text token until the next
            // parse step establishes a real structural boundary so both
            // direct and compiled XAML observe one logical value.
            const bool awaitTextBoundary =
                !streamEof_ &&
                readIndex_ + 1U == tokens_.Size() &&
                tokens_[readIndex_].kind_ == XmlTokenKind::Text;
            if (!awaitTextBoundary) {
                token = std::move(tokens_[readIndex_]);
                depth_ = tokenDepths_[readIndex_];
                ++readIndex_;
                if (readIndex_ == tokens_.Size()) {
                    tokens_.Clear();
                    tokenDepths_.Clear();
                    readIndex_ = 0U;
                }
                return token.kind_;
            }
        }
        if (streamEof_) {
            token.Clear();
            token.kind_ = XmlTokenKind::EndOfDocument;
            depth_ = 0U;
            return token.kind_;
        }
        if (stream_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Expat stream is not initialized");
        }

        std::uint8_t buffer[4096];
        Base::Result<std::uint32_t> read =
            stream_->Read(Base::Span<uint8_t>(
                buffer, static_cast<std::uint32_t>(sizeof(buffer))));
        if (!read) {
            Stop(
                read.GetStatus(),
                XmlDiagnosticCodes::MalformedMarkup,
                ExpatMessageMalformed);
            return read.GetStatus();
        }
        const std::uint32_t count = read.Value();
        if (count > limits_.maxInputBytes ||
            streamBytes_ >
                limits_.maxInputBytes - count) {
            Stop(
                Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    MessageLimit.Data()),
                XmlDiagnosticCodes::InputLimitExceeded,
                MessageLimit);
            return failure_;
        }
        streamBytes_ += count;
        if (count == 0U) {
            const enum XML_Status parsed = XML_Parse(
                static_cast<XML_Parser>(parser_),
                nullptr,
                0,
                XML_TRUE);
            streamEof_ = true;
            if (!failure_.IsOk()) return failure_;
            if (parsed != XML_STATUS_OK || parseDepth_ != 0U) {
                Stop(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        ExpatMessageMalformed.Data()),
                    XmlDiagnosticCodes::MalformedMarkup,
                    ExpatMessageMalformed);
                return failure_;
            }
            XmlToken end;
            end.kind_ = XmlTokenKind::EndOfDocument;
            const ::Aero::Diagnostics::SourcePosition position = Position();
            end.source_ = {position, position};
            Base::Result<void> stored =
                PushToken(std::move(end), 0U);
            if (!stored) return stored.GetStatus();
            continue;
        }
        const enum XML_Status parsed = XML_Parse(
            static_cast<XML_Parser>(parser_),
            reinterpret_cast<const char*>(buffer),
            static_cast<int>(count),
            XML_FALSE);
        if (!failure_.IsOk()) return failure_;
        if (parsed != XML_STATUS_OK) {
            Stop(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    ExpatMessageMalformed.Data()),
                XmlDiagnosticCodes::MalformedMarkup,
                ExpatMessageMalformed);
            return failure_;
        }
    }
}

Base::Result<XmlTokenKind> ExpatXmlTokenizer::Read(
    XmlToken& token) noexcept {
    if (!initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Expat tokenizer is not initialized");
    }
    if (streamMode_) {
        token.Clear();
        return ReadStream(token);
    }
    if (readIndex_ >= tokens_.Size()) {
        token.Clear();
        token.kind_ = XmlTokenKind::EndOfDocument;
        depth_ = 0U;
        return token.kind_;
    }
    token = std::move(tokens_[readIndex_]);
    depth_ = tokenDepths_[readIndex_];
    ++readIndex_;
    return token.kind_;
}

} // namespace Aero::Markup

#endif

// ============================================================================
// SECTION: XamlCompiledSchema / Manifest / Validation
// Source: src/gui/markup/XamlCompiledSchema.cpp
// ============================================================================

// ===== CompiledSchema =====


// Canonical compiled-schema bridge used by Loader.



namespace Aero::Markup {
namespace {

Base::Status SchemaNodeFailure(
    Base::Status status,
    const Node& node) noexcept {
    thread_local char message[512];
    const Base::StringView localName = node.Name().LocalName();
    const ::Aero::Diagnostics::SourcePosition position = node.Source().begin;
    std::snprintf(
        message,
        sizeof(message),
        "Compiled XAML schema error at %u:%u for '%.*s': %s",
        position.line,
        position.column,
        static_cast<int>(localName.SizeBytes()),
        localName.Data(),
        status.message != nullptr ? status.message : "operation failed");
    return Base::Status::Failure(status.code, message);
}

Base::Result<SchemaTypeInfo> ResolveTypeInfo(
    const Schema& schema,
    Base::StringView xamlNamespace,
    Base::StringView localName) noexcept {
    Base::Result<const Meta::TypeInfo*> type =
        schema.ResolveType(xamlNamespace, localName);
    if (!type) return type.GetStatus();
    return SchemaTypeInfo{
        type->Id(),
        type->Kind(),
        type->Flags()};
}

Base::Result<SchemaTypeInfo> ResolveTypeInfo(
    const SchemaManifest& schema,
    Base::StringView xamlNamespace,
    Base::StringView localName) noexcept {
    return schema.ResolveType(xamlNamespace, localName);
}

inline Base::StringView TrimCompiledText(
    Base::StringView value) noexcept {
    return ::Aero::Base::ValueConversion::Trim(value);
}

inline bool IsWhitespaceOnly(
    Base::StringView value) noexcept {
    return TrimCompiledText(value).Empty();
}

inline bool EqualsAsciiInsensitive(
    Base::StringView value,
    Base::StringView expected) noexcept {
    return ::Aero::Base::ValueConversion::EqualsAsciiInsensitive(value, expected);
}

bool TryGetCompiledLiteral(
    Base::StringView authored,
    Base::StringView& literal) noexcept {
    const Base::StringView trimmed =
        TrimCompiledText(authored);
    if (trimmed.Empty() || trimmed[0] != '{') {
        literal = authored;
        return true;
    }
    if (trimmed.SizeBytes() >= 2U &&
        trimmed[1] == '}') {
        literal = trimmed.Substr(
            2U, trimmed.SizeBytes() - 2U);
        return true;
    }
    return false;
}

bool IsPersistableCompiledCustomType(
    Meta::TypeId type) noexcept {
    return type == Meta::TypeOf<::Aero::Length>() ||
        type == Meta::TypeOf<Base::Thickness>() ||
        type == Meta::TypeOf<Base::CornerRadius>() ||
        type == Meta::TypeOf<Base::Color>() ||
        type == Meta::TypeOf<Base::Point>() ||
        type == Meta::TypeOf<Base::Transform2D>() ||
        type == Meta::TypeOf<::Aero::GridLength>();
}

bool IsPersistableCompiledValue(
    const Meta::Value& value) noexcept {
    if (value.Type() == Meta::InvalidTypeId) return false;
    switch (value.Kind()) {
    case Meta::ValueKind::Boolean:
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double:
    case Meta::ValueKind::String:
        return true;
    case Meta::ValueKind::Custom:
        return IsPersistableCompiledCustomType(value.Type());
    case Meta::ValueKind::Unset:
    case Meta::ValueKind::Object:
        return false;
    }
    return false;
}

template<class T>
Base::Result<Meta::Value> MakeManifestCustomValue(
    Meta::TypeId type,
    const T& value) noexcept {
    static const Meta::ValueTypeRegistration s_reg =
        Meta::MakeValueTypeRegistration<T>();
    return Meta::Value::TryFromCustom(
        type, &value, &s_reg);
}



Base::Result<Meta::Value>
ConvertManifestPrimitive(
    Meta::TypeId type,
    Base::StringView literal) noexcept {
    if (type == Meta::TypeOf<Base::String>()) {
        return Meta::Value::TryFromString(type, literal);
    }

    const Base::StringView text =
        TrimCompiledText(literal);
    if (type == Meta::TypeOf<bool>()) {
        Base::Result<bool> parsed =
            ::Aero::Base::ValueConversion::ConvertBoolean(text);
        if (!parsed) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Boolean literal is invalid");
        }
        return Meta::Value::FromBoolean(type, parsed.Value());
    }

    if (type == Meta::TypeOf<double>()) {
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::ConvertDouble(text);
        if (!parsed) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Double literal is invalid");
        }
        return Meta::Value::FromDouble(type, parsed.Value());
    }

    auto convertSigned = [&](auto dummy) -> Base::Result<Meta::Value> {
        using T = decltype(dummy);
        Base::Result<T> parsed = ::Aero::Base::ValueConversion::ConvertInteger<T>(text);
        if (!parsed) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB2 signed integer literal is invalid");
        return Meta::Value::FromSignedInteger(type, parsed.Value());
    };
    if (type == Meta::TypeOf<std::int8_t>()) return convertSigned(std::int8_t{});
    if (type == Meta::TypeOf<std::int16_t>()) return convertSigned(std::int16_t{});
    if (type == Meta::TypeOf<std::int32_t>()) return convertSigned(std::int32_t{});
    if (type == Meta::TypeOf<std::int64_t>()) return convertSigned(std::int64_t{});

    auto convertUnsigned = [&](auto dummy) -> Base::Result<Meta::Value> {
        using T = decltype(dummy);
        Base::Result<T> parsed = ::Aero::Base::ValueConversion::ConvertInteger<T>(text);
        if (!parsed) return Base::Status::Failure(Base::ErrorCode::ValidationFailed, "AXB2 unsigned integer literal is invalid");
        return Meta::Value::FromUnsignedInteger(type, parsed.Value());
    };
    if (type == Meta::TypeOf<std::uint8_t>()) return convertUnsigned(std::uint8_t{});
    if (type == Meta::TypeOf<std::uint16_t>()) return convertUnsigned(std::uint16_t{});
    if (type == Meta::TypeOf<std::uint32_t>()) return convertUnsigned(std::uint32_t{});
    if (type == Meta::TypeOf<std::uint64_t>()) return convertUnsigned(std::uint64_t{});

    if (type == Meta::TypeOf<::Aero::Length>()) {
        if (EqualsAsciiInsensitive(text, "auto")) {
            return MakeManifestCustomValue(
                type, ::Aero::Length::Auto());
        }
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::
                ParseDouble(text);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Length must be Auto or nonnegative");
        }
        return MakeManifestCustomValue(
            type,
            ::Aero::Length::Pixels(parsed.Value()));
    }

    if (type == Meta::TypeOf<Base::Thickness>()) {
        Base::Result<Base::Thickness> parsed =
            ::Aero::Base::ValueConversion::ConvertThickness(text);
        if (!parsed) return parsed.GetStatus();
        return MakeManifestCustomValue(type, parsed.Value());
    }

    if (type == Meta::TypeOf<Base::CornerRadius>()) {
        Base::Result<Base::CornerRadius> parsed =
            ::Aero::Base::ValueConversion::ConvertCornerRadius(text);
        if (!parsed) return parsed.GetStatus();
        return MakeManifestCustomValue(type, parsed.Value());
    }

    if (type == Meta::TypeOf<Base::Point>()) {
        Base::Result<Base::Point> parsed =
            ::Aero::Base::ValueConversion::ConvertPoint(text);
        if (!parsed) return parsed.GetStatus();
        return MakeManifestCustomValue(type, parsed.Value());
    }

    if (type == Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            ::Aero::Base::ValueConversion::ConvertColor(text);
        if (!color) return color.GetStatus();
        return MakeManifestCustomValue(type, color.Value());
    }

    if (type == Meta::TypeOf<Base::Transform2D>()) {
        Base::Result<Base::Transform2D> matrix =
            ::Aero::Base::ValueConversion::ConvertMatrix(text);
        if (!matrix) return matrix.GetStatus();
        return MakeManifestCustomValue(type, matrix.Value());
    }

    if (type == Meta::TypeOf<::Aero::GridLength>()) {
        Base::Result<::Aero::GridLength> length =
            ::Aero::Base::ValueConversion::ConvertGridLength(text);
        if (!length) return length.GetStatus();
        return MakeManifestCustomValue(type, length.Value());
    }

    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "AXB2 manifest cannot preconvert this value type");
}

Base::Result<void> BindCompiledValue(
    const Schema& schema,
    Node& node,
    Meta::TypeId valueType) noexcept {
    Base::StringView literal;
    if (valueType == Meta::InvalidTypeId ||
        !TryGetCompiledLiteral(node.Value(), literal)) {
        return {};
    }
    Base::Result<Meta::Value> converted =
        schema.ConvertText( valueType, literal);
    if (converted &&
        IsPersistableCompiledValue(converted.Value())) {
        node.BindCompiledValue(
            std::move(converted).Value());
    }
    return {};
}

Base::Result<void> BindCompiledValue(
    const SchemaManifest&,
    Node& node,
    Meta::TypeId valueType) noexcept {
    Base::StringView literal;
    if (valueType == Meta::InvalidTypeId ||
        !TryGetCompiledLiteral(node.Value(), literal)) {
        return {};
    }
    Base::Result<Meta::Value> converted =
        ConvertManifestPrimitive(valueType, literal);
    if (converted &&
        IsPersistableCompiledValue(converted.Value())) {
        node.BindCompiledValue(
            std::move(converted).Value());
    }
    return {};
}

CompiledMemberBinding BuildCompiledMemberBinding(
    const Schema& schema,
    const ResolvedMember& member) noexcept;

void BindCompiledMemberInstruction(
    const Schema& schema,
    Node& node,
    const ResolvedMember& member) noexcept {
    node.BindCompiledMember(
        BuildCompiledMemberBinding(schema, member));
}

CompiledMemberBinding BuildCompiledMemberBinding(
    const Schema& schema,
    const ResolvedMember& member) noexcept {
    const MemberWritePolicy policy =
        schema.ResolveMemberWritePolicy( member);
    CompiledMemberBinding binding;
    binding.id = member.id;
    binding.kind = member.kind;
    binding.ownerType = member.ownerType;
    binding.valueType = member.valueType;
    if (const Meta::TypeInfo* valueType =
            schema.Types().FindType(member.valueType)) {
        binding.valueTypeKind = valueType->Kind();
        binding.valueTypeFlags = valueType->Flags();
    }
    binding.propertyFlags = member.propertyFlags;
    binding.eventFlags = member.eventFlags;
    binding.writeMode =
        static_cast<std::uint8_t>(policy.mode);
    binding.attached = member.attached;
    binding.acceptsAnyValue =
        policy.acceptsAnyValue;
    binding.writable = policy.writable;
    return binding;
}

void BindCompiledMemberInstruction(
    const SchemaManifest&,
    Node& node,
    const ResolvedMember& member) noexcept {
    // A manifest intentionally carries no runtime accessor/facet callbacks.
    // Persist the stable id; the target runtime expands it after identity
    // validation during AXB2 deserialization.
    node.BindCompiledMember(member.id);
}

Base::Result<void> BindCompiledTypeInstruction(
    const Schema& schema,
    Node& node,
    const SchemaTypeInfo& type) noexcept {
    CompiledTypeBinding binding;
    binding.id = type.id;
    binding.kind = type.kind;
    binding.flags = type.flags;

    Base::Result<ResolvedMember> content =
        schema.ResolveContentMember(type.id);
    if (content) {
        binding.contentMember =
            BuildCompiledMemberBinding(
                schema, content.Value());
        binding.hasContentMember = true;
    } else if (content.GetStatus().code !=
               Base::ErrorCode::NotFound) {
        return content.GetStatus();
    }

    node.BindCompiledType(binding);
    return {};
}

Base::Result<void> BindCompiledTypeInstruction(
    const SchemaManifest&,
    Node& node,
    const SchemaTypeInfo& type) noexcept {
    // Manifest files intentionally carry no executable content/accessor
    // policy. The target runtime expands the stable TypeId after validating
    // the AXB2 identity.
    node.BindCompiledType(type.id);
    return {};
}

template<class TSchema>
Base::Result<void> ValidateSchemaCore(
    const CompiledDocument& document,
    const TSchema& schema,
    bool bindInstructions = false) noexcept {
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML schema validation requires a valid document");
    }
    enum class FrameKind : std::uint8_t {
        Object = 0U,
        ValueObject,
        Member,
        PropertyElement,
        NullObject
    };
    struct Frame {
        FrameKind kind = FrameKind::Object;
        Meta::TypeId type = Meta::InvalidTypeId;
    };
    Base::Vector<Frame> frames;
    bool rootSeen = false;
    for (const Node& node : document.Nodes()) {
        switch (node.Kind()) {
        case NodeKind::NamespaceDeclaration:
            break;
        case NodeKind::Value: {
            if (!bindInstructions || frames.Empty() ||
                (!node.IsFromAttribute() &&
                 IsWhitespaceOnly(node.Value()))) {
                break;
            }
            const Frame& frame = frames.Back();
            Meta::TypeId valueType = Meta::InvalidTypeId;
            if (frame.kind == FrameKind::Member ||
                frame.kind == FrameKind::PropertyElement ||
                frame.kind == FrameKind::ValueObject) {
                valueType = frame.type;
            } else if (frame.kind == FrameKind::Object) {
                Base::Result<ResolvedMember> content =
                    schema.ResolveContentMember(frame.type);
                if (content) {
                    valueType = content.Value().valueType;
                }
            }
            Base::Result<void> bound = BindCompiledValue(
                schema,
                const_cast<Node&>(node),
                valueType);
            if (!bound) return bound.GetStatus();
            break;
        }
        case NodeKind::StartObject: {
            const bool nullObject =
                node.Name().NamespaceUri() == LanguageNamespaceUri() &&
                node.Name().LocalName() == Base::StringView("Null");
            bool propertyElement = false;
            for (std::uint32_t index = 0U;
                 index < node.Name().LocalName().SizeBytes(); ++index) {
                propertyElement = propertyElement ||
                    node.Name().LocalName()[index] == '.';
            }
            if (propertyElement && !frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                Base::Result<ResolvedMember> member = schema.ResolveMember(
                    frames.Back().type,
                    node.Name(),
                    MemberSyntax::PropertyElement);
                if (!member) {
                    return SchemaNodeFailure(member.GetStatus(), node);
                }
                if (bindInstructions) {
                    BindCompiledMemberInstruction(
                        schema,
                        const_cast<Node&>(node),
                        member.Value());
                }
                frames.PushBack({
                    FrameKind::PropertyElement,
                    member.Value().valueType});
                break;
            }
            if (nullObject) {
                frames.PushBack({
                    FrameKind::NullObject,
                    Meta::InvalidTypeId});
                break;
            }
            Base::Result<SchemaTypeInfo> type = ResolveTypeInfo(
                schema,
                node.Name().NamespaceUri(),
                node.Name().LocalName());
            if (!type) {
                return SchemaNodeFailure(type.GetStatus(), node);
            }
            if (bindInstructions) {
                Base::Result<void> boundType =
                    BindCompiledTypeInstruction(
                        schema,
                        const_cast<Node&>(node),
                        type.Value());
                if (!boundType) {
                    return boundType.GetStatus();
                }
            }
            if (!frames.Empty() &&
                frames.Back().kind == FrameKind::Object) {
                Base::Result<ResolvedMember> content =
                    schema.ResolveContentMember(frames.Back().type);
                if (!content) return content.GetStatus();
            } else if (!frames.Empty() &&
                frames.Back().kind != FrameKind::Member &&
                frames.Back().kind != FrameKind::PropertyElement) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML object nesting is invalid");
            } else if (frames.Empty()) {
                if (rootSeen) {
                    return Base::Status::Failure(
                        Base::ErrorCode::AlreadyExists,
                        "Compiled XAML contains multiple roots");
                }
                rootSeen = true;
            }
            frames.PushBack({
                Meta::HasTypeFlag(
                    type.Value().flags,
                    Meta::TypeFlags::ValueType)
                    ? FrameKind::ValueObject
                    : FrameKind::Object,
                type.Value().id});
            break;
        }
        case NodeKind::EndObject:
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
                 frames.Back().kind != FrameKind::ValueObject &&
                 frames.Back().kind != FrameKind::PropertyElement &&
                 frames.Back().kind != FrameKind::NullObject)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML object frame is unbalanced");
            }
            frames.PopBack();
            break;
        case NodeKind::StartMember: {
            if (frames.Empty() ||
                (frames.Back().kind != FrameKind::Object &&
                 frames.Back().kind != FrameKind::ValueObject)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member has no object owner");
            }
            Meta::TypeId memberValueType = Meta::InvalidTypeId;
            if (frames.Back().kind == FrameKind::ValueObject &&
                node.Name().NamespaceUri() != LanguageNamespaceUri()) {
                if (!node.IsFromAttribute() ||
                    node.Name().LocalName() != Base::StringView("Value")) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Compiled XAML value-type member was not found");
                }
                memberValueType = frames.Back().type;
            } else if (
                node.IsFromAttribute() &&
                node.Name().LocalName() == Base::StringView("Name")) {
                // WPF treats the ordinary Name attribute as the x:Name
                // directive. The object writer performs the name-scope
                // registration and also initializes a real Name property
                // when the target type exposes one.
            } else if (node.Name().NamespaceUri() !=
                       LanguageNamespaceUri()) {
                Base::Result<ResolvedMember> member = schema.ResolveMember(
                    frames.Back().type,
                    node.Name(),
                    MemberSyntax::Attribute);
                if (!member) {
                    return SchemaNodeFailure(member.GetStatus(), node);
                }
                memberValueType = member.Value().valueType;
                if (bindInstructions) {
                    BindCompiledMemberInstruction(
                        schema,
                        const_cast<Node&>(node),
                        member.Value());
                }
            } else if (
                (frames.Back().kind == FrameKind::ValueObject &&
                 node.Name().LocalName() != Base::StringView("Key")) ||
                (frames.Back().kind == FrameKind::Object &&
                 node.Name().LocalName() != Base::StringView("Name") &&
                 node.Name().LocalName() != Base::StringView("Key") &&
                 node.Name().LocalName() != Base::StringView("Class"))) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Compiled XAML directive is not supported");
            }
            frames.PushBack({
                FrameKind::Member,
                memberValueType});
            break;
        }
        case NodeKind::EndMember:
            if (frames.Empty() ||
                frames.Back().kind != FrameKind::Member) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML member frame is unbalanced");
            }
            frames.PopBack();
            break;
        case NodeKind::EndOfDocument:
            if (!frames.Empty() || !rootSeen) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Compiled XAML document is incomplete");
            }
            return {};
        case NodeKind::None:
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML contains an empty node");
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "Compiled XAML has no end-of-document node");
}

} // namespace

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const Schema& schema) noexcept {
    return Compile(reader, schema, {});
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const Schema& schema,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledDocument> compiled =
        Compile(reader, schema.Domain(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(schema.Domain());
    if (!identity) return identity.GetStatus();
    compiled.Value().identity_ = identity.Value();
    Base::Result<void> valid = compiled.Value().BindSchema(schema);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const SchemaManifest& manifest) noexcept {
    return Compile(reader, manifest, {});
}

Base::Result<CompiledDocument> CompiledDocument::Compile(
    NodeReader& reader,
    const SchemaManifest& manifest,
    const Base::ResourceUri& originUri) noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML requires a valid schema manifest");
    }
    Base::Result<CompiledDocument> compiled =
        CompileWithIdentity(reader, manifest.Identity(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<void> valid = compiled.Value().BindSchema(manifest);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<void> CompiledDocument::ValidateSchema(
    const Schema& schema) const noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML validation requires a frozen runtime schema");
    }
    return ValidateSchemaCore(*this, schema);
}

Base::Result<void> CompiledDocument::BindSchema(
    const Schema& schema) noexcept {
    if (!schema.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 binding requires a frozen runtime schema");
    }
    return ValidateSchemaCore(*this, schema, true);
}

Base::Result<void> CompiledDocument::ValidateSchema(
    const SchemaManifest& manifest) const noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Compiled XAML validation requires a valid schema manifest");
    }
    if (CompareCompiledCacheIdentity(identity_, manifest.Identity()) !=
        CompiledCacheCompatibility::Compatible) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Compiled XAML identity does not match the schema manifest");
    }
    return ValidateSchemaCore(*this, manifest);
}

Base::Result<void> CompiledDocument::BindSchema(
    const SchemaManifest& manifest) noexcept {
    if (!manifest.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 binding requires a valid schema manifest");
    }
    if (CompareCompiledCacheIdentity(identity_, manifest.Identity()) !=
        CompiledCacheCompatibility::Compatible) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 identity does not match the schema manifest");
    }
    return ValidateSchemaCore(*this, manifest, true);
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: XamlCompiledDocument / AXB2 Serialization
// Source: src/gui/markup/XamlCompiledDocument.cpp
// ============================================================================


// ===== CompiledDocument =====


// Canonical compiled-document implementation.

namespace Aero::Markup {
namespace {

constexpr std::uint32_t CompiledDocumentMagic =
    UINT32_C(0x32425841); // "AXB2"
constexpr std::uint32_t InvalidTableIndex = UINT32_MAX;

enum class AxbSectionKind : std::uint32_t {
    Dependencies = 1U,
    Strings = 2U,
    Types = 3U,
    Members = 4U,
    Values = 5U,
    Instructions = 6U,
    SourceMap = 7U,
    SourceFallback = 8U
};

struct AxbSection {
    AxbSectionKind kind = AxbSectionKind::Strings;
    std::uint32_t count = 0U;
    Base::Vector<std::uint8_t> bytes;
};

struct AxbSectionDirectoryEntry {
    AxbSectionKind kind = AxbSectionKind::Strings;
    std::uint32_t offset = 0U;
    std::uint32_t size = 0U;
    std::uint32_t count = 0U;
};

enum class AxbValueKind : std::uint8_t {
    Text = 0U,
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Double,
    String,
    Custom
};

inline constexpr std::uint8_t AxbMaxValuePayloads = 6U;

struct AxbValueRecord {
    AxbValueKind kind = AxbValueKind::Text;
    std::uint8_t payloadCount = 0U;
    std::uint32_t typeIndex = InvalidTableIndex;
    std::uint64_t payload[AxbMaxValuePayloads]{};
};

enum AxbInstructionFlag : std::uint8_t {
    AxbInstructionFromAttribute = 1U << 0U,
    AxbInstructionHasType = 1U << 1U,
    AxbInstructionHasMember = 1U << 2U,
    AxbInstructionHasValue = 1U << 3U,
    AxbInstructionHasQualifiedName = 1U << 4U,
    AxbInstructionHasNamespace = 1U << 5U
};

inline constexpr std::uint8_t AxbInstructionKnownFlags =
    AxbInstructionFromAttribute |
    AxbInstructionHasType |
    AxbInstructionHasMember |
    AxbInstructionHasValue |
    AxbInstructionHasQualifiedName |
    AxbInstructionHasNamespace;

bool HasInstructionFlag(
    std::uint8_t flags,
    AxbInstructionFlag flag) noexcept {
    return (flags & static_cast<std::uint8_t>(flag)) != 0U;
}

bool NeedsInstructionQualifiedName(
    const Node& node) noexcept {
    if (node.Kind() == NodeKind::StartObject) {
        return node.CompiledTypeId() == Meta::InvalidTypeId &&
            node.CompiledMemberId() == Meta::InvalidMemberId;
    }
    if (node.Kind() == NodeKind::StartMember) {
        return node.CompiledMemberId() == Meta::InvalidMemberId;
    }
    return false;
}

bool IsInstructionShapeValid(
    NodeKind kind,
    std::uint8_t flags) noexcept {
    if ((flags & ~AxbInstructionKnownFlags) != 0U) {
        return false;
    }

    const bool hasValue = HasInstructionFlag(
        flags, AxbInstructionHasValue);
    const bool hasNamespace = HasInstructionFlag(
        flags, AxbInstructionHasNamespace);

    if (hasNamespace !=
        (kind == NodeKind::NamespaceDeclaration)) {
        return false;
    }

    return hasValue == (kind == NodeKind::Value);
}


template<class TDestination, class TSource>
TDestination CopyValueBits(
    const TSource& source) noexcept {
    static_assert(
        sizeof(TDestination) == sizeof(TSource),
        "AXB2 value bit copy requires equal sizes");
    TDestination destination{};
    std::memcpy(
        &destination, &source, sizeof(destination));
    return destination;
}

std::uint64_t PackFloatPair(
    float low,
    float high) noexcept {
    const std::uint32_t lowBits =
        CopyValueBits<std::uint32_t>(low);
    const std::uint32_t highBits =
        CopyValueBits<std::uint32_t>(high);
    return static_cast<std::uint64_t>(lowBits) |
        (static_cast<std::uint64_t>(highBits) << 32U);
}

float UnpackFloat(
    std::uint64_t payload,
    bool high) noexcept {
    const std::uint32_t bits = high
        ? static_cast<std::uint32_t>(payload >> 32U)
        : static_cast<std::uint32_t>(payload);
    return CopyValueBits<float>(bits);
}

template<class T>
Base::Result<Meta::Value> MakeStoredCustomValue(
    Meta::TypeId type,
    const T& value) noexcept {
    static const Meta::ValueTypeRegistration s_reg =
        Meta::MakeValueTypeRegistration<T>();
    return Meta::Value::TryFromCustom(
        type, &value, &s_reg);
}




Base::Result<void> AppendPosition(
    Base::Vector<std::uint8_t>& output,
    ::Aero::Diagnostics::SourcePosition position) noexcept {
    Base::Result<void> result =
        AppendVarU32(output, position.line);
    if (!result) return result.GetStatus();
    result = AppendVarU32(output, position.column);
    if (!result) return result.GetStatus();
    return AppendVarU64(output, position.byteOffset);
}

Base::Result<::Aero::Diagnostics::SourcePosition> ReadPosition(
    Decoder& decoder) noexcept {
    Base::Result<std::uint32_t> line =
        decoder.ReadVarU32();
    if (!line) return line.GetStatus();
    Base::Result<std::uint32_t> column =
        decoder.ReadVarU32();
    if (!column) return column.GetStatus();
    Base::Result<std::uint64_t> offset =
        decoder.ReadVarU64();
    if (!offset) return offset.GetStatus();
    return ::Aero::Diagnostics::SourcePosition{
        line.Value(), column.Value(), offset.Value()};
}

struct AxbValueEqual {
    bool operator()(const AxbValueRecord& a, const AxbValueRecord& b) const noexcept {
        if (a.kind != b.kind || a.payloadCount != b.payloadCount || a.typeIndex != b.typeIndex) {
            return false;
        }
        for (std::uint8_t i = 0; i < a.payloadCount; ++i) {
            if (a.payload[i] != b.payload[i]) return false;
        }
        return true;
    }
};

struct AxbValueHash {
    Base::HashCode operator()(const AxbValueRecord& v, Base::HashCode seed = 0U) const noexcept {
        Base::HashCode h = Base::MixHash64(static_cast<Base::HashCode>(v.kind) ^ seed);
        h = Base::MixHash64(h ^ static_cast<Base::HashCode>(v.payloadCount));
        h = Base::MixHash64(h ^ static_cast<Base::HashCode>(v.typeIndex));
        for (std::uint8_t i = 0; i < v.payloadCount; ++i) {
            h = Base::MixHash64(h ^ v.payload[i]);
        }
        return h;
    }
};

struct SerializeInterner {
    explicit SerializeInterner(
        Base::Vector<Base::String>& stringsRef,
        Base::Vector<std::uint64_t>& typesRef,
        Base::Vector<std::uint64_t>& membersRef,
        Base::Vector<AxbValueRecord>& valuesRef) noexcept
        : strings(stringsRef),
          types(typesRef),
          members(membersRef),
          values(valuesRef) {}

    Base::Vector<Base::String>& strings;
    Base::Vector<std::uint64_t>& types;
    Base::Vector<std::uint64_t>& members;
    Base::Vector<AxbValueRecord>& values;

    Base::HashMap<Base::StringView, std::uint32_t> stringMap;
    Base::HashMap<std::uint64_t, std::uint32_t> typeMap;
    Base::HashMap<std::uint64_t, std::uint32_t> memberMap;
    Base::HashMap<AxbValueRecord, std::uint32_t, AxbValueHash, AxbValueEqual> valueMap;

    Base::Result<std::uint32_t> InternString(Base::StringView value) noexcept {
        if (const std::uint32_t* found = stringMap.Find(value)) {
            return *found;
        }
        const std::uint32_t index = strings.Size();
        Base::String s;
        Base::Result<void> assigned = s.Assign(value);
        if (!assigned) return assigned.GetStatus();
        strings.PushBack(std::move(s));
        stringMap.Insert(value, index);
        return index;
    }

    Base::Result<std::uint32_t> InternType(std::uint64_t id) noexcept {
        if (id == 0U || id == Meta::InvalidTypeId) return InvalidTableIndex;
        if (const std::uint32_t* found = typeMap.Find(id)) {
            return *found;
        }
        const std::uint32_t index = types.Size();
        types.PushBack(id);
        typeMap.Insert(id, index);
        return index;
    }

    Base::Result<std::uint32_t> InternMember(std::uint64_t id) noexcept {
        if (id == 0U || id == Meta::InvalidMemberId) return InvalidTableIndex;
        if (const std::uint32_t* found = memberMap.Find(id)) {
            return *found;
        }
        const std::uint32_t index = members.Size();
        members.PushBack(id);
        memberMap.Insert(id, index);
        return index;
    }

    Base::Result<std::uint32_t> InternValue(const AxbValueRecord& record) noexcept {
        if (const std::uint32_t* found = valueMap.Find(record)) {
            return *found;
        }
        const std::uint32_t index = values.Size();
        values.PushBack(record);
        valueMap.Insert(record, index);
        return index;
    }

    Base::Result<std::uint32_t> FindString(Base::StringView value) const noexcept {
        if (const std::uint32_t* found = stringMap.Find(value)) {
            return *found;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 instruction string was not interned");
    }

    Base::Result<std::uint32_t> FindType(std::uint64_t id) const noexcept {
        if (id == 0U || id == Meta::InvalidTypeId) return InvalidTableIndex;
        if (const std::uint32_t* found = typeMap.Find(id)) {
            return *found;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 instruction type id was not interned");
    }

    Base::Result<std::uint32_t> FindMember(std::uint64_t id) const noexcept {
        if (id == 0U || id == Meta::InvalidMemberId) return InvalidTableIndex;
        if (const std::uint32_t* found = memberMap.Find(id)) {
            return *found;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 instruction member id was not interned");
    }
};

Base::Result<AxbValueRecord> MakeAxbValueRecord(
    const Node& node,
    SerializeInterner& interner) noexcept {
    const auto makeText = [&]() noexcept
        -> Base::Result<AxbValueRecord> {
        Base::Result<std::uint32_t> stringIndex =
            interner.InternString(node.Value());
        if (!stringIndex) return stringIndex.GetStatus();
        AxbValueRecord record;
        record.kind = AxbValueKind::Text;
        record.payloadCount = 1U;
        record.payload[0] = stringIndex.Value();
        return record;
    };

    if (!node.HasCompiledValue()) return makeText();
    const Meta::Value& value = node.CompiledValue();
    Base::Result<std::uint32_t> typeIndex =
        interner.InternType(value.Type());
    if (!typeIndex ||
        typeIndex.Value() == InvalidTableIndex) {
        return typeIndex
            ? makeText()
            : Base::Result<AxbValueRecord>(
                  typeIndex.GetStatus());
    }

    AxbValueRecord record;
    record.typeIndex = typeIndex.Value();
    switch (value.Kind()) {
    case Meta::ValueKind::Boolean:
        record.kind = AxbValueKind::Boolean;
        record.payloadCount = 1U;
        record.payload[0] =
            value.AsBoolean() ? 1U : 0U;
        return record;
    case Meta::ValueKind::SignedInteger: {
        record.kind = AxbValueKind::SignedInteger;
        record.payloadCount = 1U;
        const std::int64_t stored =
            value.AsSignedInteger();
        record.payload[0] =
            CopyValueBits<std::uint64_t>(stored);
        return record;
    }
    case Meta::ValueKind::UnsignedInteger:
        record.kind = AxbValueKind::UnsignedInteger;
        record.payloadCount = 1U;
        record.payload[0] =
            value.AsUnsignedInteger();
        return record;
    case Meta::ValueKind::Double: {
        record.kind = AxbValueKind::Double;
        record.payloadCount = 1U;
        const double stored = value.AsDouble();
        record.payload[0] =
            CopyValueBits<std::uint64_t>(stored);
        return record;
    }
    case Meta::ValueKind::String: {
        record.kind = AxbValueKind::String;
        record.payloadCount = 1U;
        Base::Result<std::uint32_t> stringIndex =
            interner.InternString(value.AsString());
        if (!stringIndex) return stringIndex.GetStatus();
        record.payload[0] = stringIndex.Value();
        return record;
    }
    case Meta::ValueKind::Custom: {
        record.kind = AxbValueKind::Custom;
        if (value.Type() ==
            Meta::TypeOf<::Aero::Length>()) {
            Base::Result<::Aero::Length> decoded =
                Meta::ValueCodec<::Aero::Length>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().value);
            record.payload[1] =
                decoded.Value().isAuto ? 1U : 0U;
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Thickness>()) {
            Base::Result<Base::Thickness> decoded =
                Meta::ValueCodec<Base::Thickness>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] = PackFloatPair(
                decoded.Value().left,
                decoded.Value().top);
            record.payload[1] = PackFloatPair(
                decoded.Value().right,
                decoded.Value().bottom);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::CornerRadius>()) {
            Base::Result<Base::CornerRadius> decoded =
                Meta::ValueCodec<Base::CornerRadius>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] = PackFloatPair(
                decoded.Value().topLeft,
                decoded.Value().topRight);
            record.payload[1] = PackFloatPair(
                decoded.Value().bottomRight,
                decoded.Value().bottomLeft);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Color>()) {
            Base::Result<Base::Color> decoded =
                Meta::ValueCodec<Base::Color>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] = PackFloatPair(
                decoded.Value().red,
                decoded.Value().green);
            record.payload[1] = PackFloatPair(
                decoded.Value().blue,
                decoded.Value().alpha);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Point>()) {
            Base::Result<Base::Point> decoded =
                Meta::ValueCodec<Base::Point>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 1U;
            record.payload[0] = PackFloatPair(
                decoded.Value().x,
                decoded.Value().y);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<Base::Transform2D>()) {
            Base::Result<Base::Transform2D> decoded =
                Meta::ValueCodec<Base::Transform2D>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 6U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m11);
            record.payload[1] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m12);
            record.payload[2] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m21);
            record.payload[3] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().m22);
            record.payload[4] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().dx);
            record.payload[5] =
                CopyValueBits<std::uint64_t>(
                    decoded.Value().dy);
            return record;
        }
        if (value.Type() ==
            Meta::TypeOf<::Aero::GridLength>()) {
            Base::Result<::Aero::GridLength> decoded =
                Meta::ValueCodec<::Aero::GridLength>::
                    Decode(value);
            if (!decoded) return makeText();
            record.payloadCount = 2U;
            record.payload[0] =
                CopyValueBits<std::uint64_t>(
                    static_cast<double>(decoded.Value().value));
            record.payload[1] =
                static_cast<std::uint64_t>(
                    decoded.Value().unit);
            return record;
        }
        return makeText();
    }
    case Meta::ValueKind::Unset:
    case Meta::ValueKind::Object:
        return makeText();
    }
    return makeText();
}

Base::Result<Meta::Value> DecodeCompiledValue(
    const AxbValueRecord& record,
    Base::Span<const std::uint64_t> types,
    Base::Span<const Base::String> strings) noexcept {
    if (record.kind == AxbValueKind::Text ||
        record.typeIndex == InvalidTableIndex ||
        record.typeIndex >= types.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 typed value has an invalid type");
    }
    const Meta::TypeId type = types[record.typeIndex];
    const auto requirePayloads =
        [&record](std::uint8_t count) noexcept {
            return record.payloadCount == count;
        };

    switch (record.kind) {
    case AxbValueKind::Boolean:
        if (!requirePayloads(1U) ||
            record.payload[0] > 1U) break;
        return Meta::Value::FromBoolean(
            type, record.payload[0] != 0U);
    case AxbValueKind::SignedInteger: {
        if (!requirePayloads(1U)) break;
        const std::int64_t value =
            CopyValueBits<std::int64_t>(
                record.payload[0]);
        return Meta::Value::FromSignedInteger(
            type, value);
    }
    case AxbValueKind::UnsignedInteger:
        if (!requirePayloads(1U)) break;
        return Meta::Value::FromUnsignedInteger(
            type, record.payload[0]);
    case AxbValueKind::Double: {
        if (!requirePayloads(1U)) break;
        const double value =
            CopyValueBits<double>(
                record.payload[0]);
        if (!std::isfinite(value)) break;
        return Meta::Value::FromDouble(type, value);
    }
    case AxbValueKind::String:
        if (!requirePayloads(1U) ||
            record.payload[0] >= strings.Size()) {
            break;
        }
        return Meta::Value::TryFromString(
            type,
            strings[static_cast<std::uint32_t>(
                record.payload[0])].View());
    case AxbValueKind::Custom: {
        if (type == Meta::TypeOf<::Aero::Length>() &&
            requirePayloads(2U)) {
            const double value =
                CopyValueBits<double>(
                    record.payload[0]);
            if (!std::isfinite(value) ||
                record.payload[1] > 1U ||
                (record.payload[1] == 0U &&
                 value < 0.0)) {
                break;
            }
            return MakeStoredCustomValue(
                type,
                record.payload[1] != 0U
                    ? ::Aero::Length::Auto()
                    : ::Aero::Length::Pixels(value));
        }
        if (type == Meta::TypeOf<Base::Thickness>() &&
            requirePayloads(2U)) {
            const Base::Thickness value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true),
                UnpackFloat(record.payload[1], false),
                UnpackFloat(record.payload[1], true)};
            if (!::Aero::IsFinite(value)) break;
            return MakeStoredCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::CornerRadius>() &&
            requirePayloads(2U)) {
            const Base::CornerRadius value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true),
                UnpackFloat(record.payload[1], false),
                UnpackFloat(record.payload[1], true)};
            if (!std::isfinite(value.topLeft) ||
                !std::isfinite(value.topRight) ||
                !std::isfinite(value.bottomRight) ||
                !std::isfinite(value.bottomLeft)) {
                break;
            }
            return MakeStoredCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::Color>() &&
            requirePayloads(2U)) {
            const Base::Color value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true),
                UnpackFloat(record.payload[1], false),
                UnpackFloat(record.payload[1], true)};
            if (!Base::IsFiniteColor(value) ||
                value.red < 0.0F || value.red > 1.0F ||
                value.green < 0.0F || value.green > 1.0F ||
                value.blue < 0.0F || value.blue > 1.0F ||
                value.alpha < 0.0F || value.alpha > 1.0F) {
                break;
            }
            return MakeStoredCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::Point>() &&
            requirePayloads(1U)) {
            const Base::Point value{
                UnpackFloat(record.payload[0], false),
                UnpackFloat(record.payload[0], true)};
            if (!::Aero::IsFinite(value)) break;
            return MakeStoredCustomValue(type, value);
        }
        if (type == Meta::TypeOf<Base::Transform2D>() &&
            requirePayloads(6U)) {
            const Base::Transform2D value{
                CopyValueBits<double>(record.payload[0]),
                CopyValueBits<double>(record.payload[1]),
                CopyValueBits<double>(record.payload[2]),
                CopyValueBits<double>(record.payload[3]),
                CopyValueBits<double>(record.payload[4]),
                CopyValueBits<double>(record.payload[5])};
            if (!Base::IsFiniteTransform(value)) break;
            return MakeStoredCustomValue(type, value);
        }
        if (type == Meta::TypeOf<::Aero::GridLength>() &&
            requirePayloads(2U)) {
            const double value =
                CopyValueBits<double>(
                    record.payload[0]);
            if (!std::isfinite(value) ||
                value < 0.0 ||
                record.payload[1] >
                    static_cast<std::uint64_t>(
                        ::Aero::GridUnitType::Star)) {
                break;
            }
            return MakeStoredCustomValue(
                type,
                ::Aero::GridLength{
                    static_cast<float>(value),
                    static_cast<::Aero::GridUnitType>(
                        record.payload[1])});
        }
        break;
    }
    case AxbValueKind::Text:
        break;
    }
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "AXB2 typed value payload is invalid");
}

Base::Result<CompiledMemberBinding>
ResolveCompiledMemberBinding(
    const ::Aero::Meta::Registry& domain,
    Meta::MemberId memberId) noexcept {
    CompiledMemberBinding binding;
    binding.id = memberId;

    if (const Meta::PropertyInfo* property =
            domain.Types().FindProperty(memberId)) {
        binding.kind = Meta::MemberKind::Property;
        binding.ownerType = property->OwnerType();
        binding.valueType = property->ValueType();
        if (const Meta::TypeInfo* valueType =
                domain.Types().FindType(
                    property->ValueType())) {
            binding.valueTypeKind =
                valueType->Kind();
            binding.valueTypeFlags =
                valueType->Flags();
        }
        binding.propertyFlags = property->Flags();
        binding.attached =
            (static_cast<std::uint32_t>(
                 property->Flags()) &
             static_cast<std::uint32_t>(
                 Meta::PropertyFlags::Attached)) != 0U;

        const bool runtimeWritable =
            domain.CanWriteProperty(memberId);
        Base::Result<Meta::ContentInfo> content =
            domain.GetContentInfo(memberId);
        const bool contentWritable =
            content && content.Value().writable;
        binding.writable =
            runtimeWritable || contentWritable;
        binding.writeMode = static_cast<std::uint8_t>(
            (contentWritable &&
                    content.Value().kind ==
                        Meta::ContentKind::Collection) ||
                    memberId ==
                        VisualStateManager::VisualStateGroupsProperty
                            .Handle().value
                ? MemberWriteMode::Collection
                : MemberWriteMode::SetOnce);
        binding.acceptsAnyValue =
            runtimeWritable &&
            (((static_cast<std::uint32_t>(
                   property->Flags()) &
               static_cast<std::uint32_t>(
                   Meta::PropertyFlags::AnyValue)) != 0U) ||
             property->ValueType() ==
                 Meta::TypeOf<Meta::Value>());
        return binding;
    }

    if (const Meta::EventInfo* event =
            domain.Types().FindEvent(memberId)) {
        binding.kind = Meta::MemberKind::Event;
        binding.ownerType = event->OwnerType();
        binding.valueType = event->EventArgsType();
        if (const Meta::TypeInfo* valueType =
                domain.Types().FindType(
                    event->EventArgsType())) {
            binding.valueTypeKind =
                valueType->Kind();
            binding.valueTypeFlags =
                valueType->Flags();
        }
        binding.eventFlags = event->Flags();
        binding.attached =
            (static_cast<std::uint32_t>(
                 event->Flags()) &
             static_cast<std::uint32_t>(
                 Meta::EventFlags::Attached)) != 0U;
        return binding;
    }

    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        "AXB2 member table references an unknown frozen member id");
}

Base::Result<CompiledTypeBinding>
ResolveCompiledTypeBinding(
    const ::Aero::Meta::Registry& domain,
    Meta::TypeId typeId) noexcept {
    const Meta::TypeInfo* type =
        domain.Types().FindType(typeId);
    if (type == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 type table references an unknown frozen type id");
    }

    CompiledTypeBinding binding;
    binding.id = typeId;
    binding.kind = type->Kind();
    binding.flags = type->Flags();

    const Meta::MemberId contentId = domain.FindContentMember(typeId);
    if (contentId != Meta::InvalidMemberId) {
        Base::Result<CompiledMemberBinding> content =
            ResolveCompiledMemberBinding(
                domain,
                contentId);
        if (!content) return content.GetStatus();
        binding.contentMember = content.Value();
        binding.hasContentMember = true;
    }
    return binding;
}

const AxbSectionDirectoryEntry* FindSection(
    Base::Span<const AxbSectionDirectoryEntry> directory,
    AxbSectionKind kind) noexcept {
    for (const AxbSectionDirectoryEntry& entry : directory) {
        if (entry.kind == kind) return &entry;
    }
    return nullptr;
}

Base::Result<Decoder> OpenSection(
    Base::Span<const std::uint8_t> document,
    const AxbSectionDirectoryEntry& section) noexcept {
    if (section.offset > document.Size() ||
        section.size > document.Size() - section.offset) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 section range is outside the document");
    }
    return Decoder({document.Data() + section.offset, section.size});
}

Base::Result<void> AssignInternedString(
    Base::String& destination,
    Base::Span<const Base::String> strings,
    std::uint32_t index) noexcept {
    if (index == InvalidTableIndex) {
        destination.Clear();
        return {};
    }
    if (index >= strings.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 instruction references an invalid string index");
    }
    return destination.Assign(strings[index].View());
}

} // namespace

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    NodeReader& reader,
    const ::Aero::Meta::Registry& domain) noexcept {
    return Compile(reader, domain, {});
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    NodeReader& reader,
    const ::Aero::Meta::Registry& domain,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(domain);
    if (!identity) return identity.GetStatus();
    return CompileWithIdentity(reader, identity.Value(), originUri);
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    Base::Span<const Node> nodes,
    const ::Aero::Meta::Registry& domain,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledCacheIdentity> identity =
        BuildCompiledCacheIdentity(domain);
    if (!identity) return identity.GetStatus();

    CompiledDocument document;
    document.identity_ = identity.Value();
    document.originUri_ = originUri;
    if (!originUri.Empty()) {
        document.dependencies_.PushBack(originUri);
    }
    document.nodes_.Reserve(nodes.Size());
    for (const Node& node : nodes) {
        Base::Result<Node> cloned = Node::Clone(node);
        if (!cloned) return cloned.GetStatus();
        document.nodes_.PushBack(
            std::move(cloned).Value());
    }
    return document;
}

Base::Result<CompiledDocument>
CompiledDocument::Clone() const noexcept {
    CompiledDocument document;
    document.identity_ = identity_;
    document.originUri_ = originUri_;
    document.dependencies_ = dependencies_;
    document.bytecodeProgram_ = bytecodeProgram_;
    document.nodes_.Reserve(nodes_.Size());
    for (const Node& node : nodes_) {
        Base::Result<Node> cloned = Node::Clone(node);
        if (!cloned) return cloned.GetStatus();
        document.nodes_.PushBack(std::move(cloned).Value());
    }
    return document;
}

Base::Result<CompiledDocument>
CompiledDocument::Compile(
    Base::Span<const Node> nodes,
    const Schema& schema,
    const Base::ResourceUri& originUri) noexcept {
    Base::Result<CompiledDocument> compiled =
        Compile(nodes, schema.Domain(), originUri);
    if (!compiled) return compiled.GetStatus();
    Base::Result<void> valid = compiled.Value().BindSchema(schema);
    if (!valid) return valid.GetStatus();
    return std::move(compiled).Value();
}

Base::Result<CompiledDocument>
CompiledDocument::CompileWithIdentity(
    NodeReader& reader,
    const CompiledCacheIdentity& identity,
    const Base::ResourceUri& originUri) noexcept {
    CompiledDocument document;
    document.identity_ = identity;
    document.originUri_ = originUri;
    if (!originUri.Empty()) {
        document.dependencies_.PushBack(originUri);
    }
    Node node;
    while (true) {
        Base::Result<NodeKind> read = reader.Read(node);
        if (!read) return read.GetStatus();
        Base::Result<Node> cloned = Node::Clone(node);
        if (!cloned) return cloned.GetStatus();
        document.nodes_.PushBack(
            std::move(cloned).Value());
        if (read.Value() == NodeKind::EndOfDocument) break;
    }
    return document;
}

Base::Result<void> CompiledDocument::AddDependency(
    const Base::ResourceUri& dependency) noexcept {
    if (dependency.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Compiled XAML dependency URI is empty");
    }
    for (const Base::ResourceUri& existing :
         dependencies_) {
        if (existing == dependency) {
            return {};
        }
    }
    dependencies_.PushBack(dependency);
    return {};
}

Base::Result<Base::Vector<std::uint8_t>>
CompiledDocument::Serialize(
    const CompiledDocumentSerializeOptions& options) const noexcept {
    if (!IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "AXB2 document is not valid");
    }

    Base::Vector<Base::String> strings;
    Base::Vector<std::uint64_t> types;
    Base::Vector<std::uint64_t> members;
    Base::Vector<AxbValueRecord> values;
    Base::Vector<std::uint32_t> nodeValueIndices;

    SerializeInterner interner(strings, types, members, values);

    Base::Result<std::uint32_t> originString = interner.InternString(
        originUri_.Canonical());
    if (!originString) return originString.GetStatus();
    for (const Base::ResourceUri& dependency : dependencies_) {
        Base::Result<std::uint32_t> index = interner.InternString(
            dependency.Canonical());
        if (!index) return index.GetStatus();
    }
    nodeValueIndices.Reserve(nodes_.Size());

    for (const Node& node : nodes_) {
        if (NeedsInstructionQualifiedName(node)) {
            const Base::StringView qualifiedName[] = {
                node.name_.prefix_.View(),
                node.name_.localName_.View(),
                node.name_.namespaceUri_.View()};
            for (Base::StringView value : qualifiedName) {
                Base::Result<std::uint32_t> index =
                    interner.InternString(value);
                if (!index) return index.GetStatus();
            }
        }
        if (node.kind_ == NodeKind::NamespaceDeclaration) {
            Base::Result<std::uint32_t> prefix =
                interner.InternString(
                    node.namespacePrefix_.View());
            if (!prefix) return prefix.GetStatus();
            Base::Result<std::uint32_t> uri =
                interner.InternString(
                    node.namespaceUri_.View());
            if (!uri) return uri.GetStatus();
        }

        std::uint32_t valueIndex = InvalidTableIndex;
        if (node.kind_ == NodeKind::Value) {
            Base::Result<AxbValueRecord> record =
                MakeAxbValueRecord(node, interner);
            if (!record) return record.GetStatus();
            Base::Result<std::uint32_t> interned =
                interner.InternValue(record.Value());
            if (!interned) return interned.GetStatus();
            valueIndex = interned.Value();
        }
        nodeValueIndices.PushBack(valueIndex);

        Base::Result<std::uint32_t> typeIndex = interner.InternType(
            node.compiledTypeId_);
        if (!typeIndex) return typeIndex.GetStatus();
        Base::Result<std::uint32_t> memberIndex = interner.InternMember(
            node.compiledMemberId_);
        if (!memberIndex) return memberIndex.GetStatus();
    }

    Base::Vector<AxbSection> sections;
    auto addSection = [&sections](
        AxbSectionKind kind,
        std::uint32_t count,
        Base::Vector<std::uint8_t>&& bytes) noexcept -> Base::Result<void> {
        AxbSection section;
        section.kind = kind;
        section.count = count;
        section.bytes = std::move(bytes);
        sections.PushBack(std::move(section));
        return {};
    };

    Base::Vector<std::uint8_t> dependencyBytes;
    Base::Result<void> result = AppendU32(
        dependencyBytes, originString.Value());
    if (!result) return result.GetStatus();
    result = AppendU32(dependencyBytes, dependencies_.Size());
    if (!result) return result.GetStatus();
    for (const Base::ResourceUri& dependency : dependencies_) {
        Base::Result<std::uint32_t> index = interner.FindString(
            dependency.Canonical());
        if (!index) return index.GetStatus();
        result = AppendU32(dependencyBytes, index.Value());
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Dependencies,
        dependencies_.Size(),
        std::move(dependencyBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> stringBytes;
    result = AppendU32(stringBytes, strings.Size());
    if (!result) return result.GetStatus();
    for (const Base::String& string : strings) {
        result = AppendString(stringBytes, string.View());
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Strings, strings.Size(), std::move(stringBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> typeBytes;
    result = AppendU32(typeBytes, types.Size());
    if (!result) return result.GetStatus();
    for (std::uint64_t type : types) {
        result = AppendU64(typeBytes, type);
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Types, types.Size(), std::move(typeBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> memberBytes;
    result = AppendU32(memberBytes, members.Size());
    if (!result) return result.GetStatus();
    for (std::uint64_t member : members) {
        result = AppendU64(memberBytes, member);
        if (!result) return result.GetStatus();
    }
    result = addSection(
        AxbSectionKind::Members, members.Size(), std::move(memberBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> valueBytes;
    result = AppendU32(valueBytes, values.Size());
    if (!result) return result.GetStatus();
    for (const AxbValueRecord& value : values) {
        if (value.payloadCount == 0U ||
            value.payloadCount > AxbMaxValuePayloads) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "AXB2 value payload count is invalid");
        }
        result = AppendU8(
            valueBytes,
            static_cast<std::uint8_t>(value.kind));
        if (!result) return result.GetStatus();
        result = AppendU8(
            valueBytes, value.payloadCount);
        if (!result) return result.GetStatus();
        result = AppendU16(valueBytes, 0U);
        if (!result) return result.GetStatus();
        result = AppendU32(
            valueBytes, value.typeIndex);
        if (!result) return result.GetStatus();
        for (std::uint32_t payloadIndex = 0U;
             payloadIndex < value.payloadCount;
             ++payloadIndex) {
            result = AppendU64(
                valueBytes,
                value.payload[payloadIndex]);
            if (!result) return result.GetStatus();
        }
    }
    result = addSection(
        AxbSectionKind::Values, values.Size(), std::move(valueBytes));
    if (!result) return result.GetStatus();

    Base::Vector<std::uint8_t> instructionBytes;
    result = AppendVarU32(
        instructionBytes, nodes_.Size());
    if (!result) return result.GetStatus();
    for (std::uint32_t nodeIndex = 0U;
         nodeIndex < nodes_.Size(); ++nodeIndex) {
        const Node& node = nodes_[nodeIndex];
        Base::Result<std::uint32_t> typeIndex =
            interner.FindType(node.compiledTypeId_);
        if (!typeIndex) return typeIndex.GetStatus();
        Base::Result<std::uint32_t> memberIndex =
            interner.FindMember(node.compiledMemberId_);
        if (!memberIndex) return memberIndex.GetStatus();
        const std::uint32_t valueIndex =
            nodeValueIndices[nodeIndex];
        const bool hasQualifiedName =
            NeedsInstructionQualifiedName(node);
        const bool hasNamespace =
            node.kind_ == NodeKind::NamespaceDeclaration;

        std::uint8_t flags = node.fromAttribute_
            ? static_cast<std::uint8_t>(AxbInstructionFromAttribute)
            : static_cast<std::uint8_t>(0U);
        if (typeIndex.Value() != InvalidTableIndex) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasType);
        }
        if (memberIndex.Value() != InvalidTableIndex) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasMember);
        }
        if (valueIndex != InvalidTableIndex) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasValue);
        }
        if (hasQualifiedName) {
            flags = static_cast<std::uint8_t>(
                flags |
                AxbInstructionHasQualifiedName);
        }
        if (hasNamespace) {
            flags = static_cast<std::uint8_t>(
                flags | AxbInstructionHasNamespace);
        }
        if (!IsInstructionShapeValid(
                node.kind_, flags)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "AXB2 instruction shape is invalid");
        }

        result = AppendU8(
            instructionBytes,
            static_cast<std::uint8_t>(
                node.kind_));
        if (!result) return result.GetStatus();
        result = AppendU8(
            instructionBytes, flags);
        if (!result) return result.GetStatus();

        if (HasInstructionFlag(
                flags, AxbInstructionHasType)) {
            result = AppendVarU32(
                instructionBytes,
                typeIndex.Value());
            if (!result) return result.GetStatus();
        }
        if (HasInstructionFlag(
                flags, AxbInstructionHasMember)) {
            result = AppendVarU32(
                instructionBytes,
                memberIndex.Value());
            if (!result) return result.GetStatus();
        }
        if (HasInstructionFlag(
                flags, AxbInstructionHasValue)) {
            result = AppendVarU32(
                instructionBytes, valueIndex);
            if (!result) return result.GetStatus();
        }
        if (hasQualifiedName) {
            const Base::StringView qualifiedName[] = {
                node.name_.prefix_.View(),
                node.name_.localName_.View(),
                node.name_.namespaceUri_.View()};
            for (Base::StringView value :
                 qualifiedName) {
                Base::Result<std::uint32_t> index =
                    interner.FindString(value);
                if (!index) return index.GetStatus();
                result = AppendVarU32(
                    instructionBytes,
                    index.Value());
                if (!result) {
                    return result.GetStatus();
                }
            }
        }
        if (hasNamespace) {
            Base::Result<std::uint32_t> prefix =
                interner.FindString(
                    node.namespacePrefix_.View());
            if (!prefix) return prefix.GetStatus();
            Base::Result<std::uint32_t> uri =
                interner.FindString(
                    node.namespaceUri_.View());
            if (!uri) return uri.GetStatus();
            result = AppendVarU32(
                instructionBytes,
                prefix.Value());
            if (!result) return result.GetStatus();
            result = AppendVarU32(
                instructionBytes,
                uri.Value());
            if (!result) return result.GetStatus();
        }
    }
    result = addSection(
        AxbSectionKind::Instructions,
        nodes_.Size(),
        std::move(instructionBytes));
    if (!result) return result.GetStatus();

    if (options.includeSourceMap) {
        Base::Vector<std::uint8_t> sourceMapBytes;
        result = AppendVarU32(
            sourceMapBytes, nodes_.Size());
        if (!result) return result.GetStatus();
        for (const Node& node : nodes_) {
            result = AppendPosition(
                sourceMapBytes, node.source_.begin);
            if (!result) return result.GetStatus();
            result = AppendPosition(
                sourceMapBytes, node.source_.end);
            if (!result) return result.GetStatus();
        }
        result = addSection(
            AxbSectionKind::SourceMap,
            nodes_.Size(),
            std::move(sourceMapBytes));
        if (!result) return result.GetStatus();
    }

    Base::Vector<std::uint8_t> output;
    result = AppendU32(output, CompiledDocumentMagic);
    if (!result) return result.GetStatus();
    result = AppendU32(output, XamlCompiledDocumentEncodingVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.cacheFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.typeIdAlgorithmVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.metadataSchemaFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.metadataProgramFormatVersion);
    if (!result) return result.GetStatus();
    result = AppendU32(output, identity_.schemaVersion);
    if (!result) return result.GetStatus();
    result = AppendU64(output, identity_.metadataSchemaHash);
    if (!result) return result.GetStatus();
    result = AppendU32(output, sections.Size());
    if (!result) return result.GetStatus();

    std::uint32_t sectionOffset =
        40U + sections.Size() * 16U;
    for (const AxbSection& section : sections) {
        if (sectionOffset > UINT32_MAX - section.bytes.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 document exceeds the 32-bit section range");
        }
        result = AppendU32(output, static_cast<std::uint32_t>(section.kind));
        if (!result) return result.GetStatus();
        result = AppendU32(output, sectionOffset);
        if (!result) return result.GetStatus();
        result = AppendU32(output, section.bytes.Size());
        if (!result) return result.GetStatus();
        result = AppendU32(output, section.count);
        if (!result) return result.GetStatus();
        sectionOffset += section.bytes.Size();
    }
    for (const AxbSection& section : sections) {
        for (std::uint8_t byte : section.bytes) {
            output.PushBack(byte);
        }
    }
    return output;
}

Base::Result<CompiledDocument>
CompiledDocument::Deserialize(
    Base::Span<const std::uint8_t> bytes,
    const ::Aero::Meta::Registry& domain,
    const CompiledDocumentLimits& limits) noexcept {
    if (limits.maxNodes == 0U ||
        limits.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Compiled XAML limits must be positive");
    }
    Decoder decoder(bytes);
    Base::Result<std::uint32_t> magic = decoder.ReadU32();
    if (!magic) return magic.GetStatus();
    Base::Result<std::uint32_t> encoding = decoder.ReadU32();
    if (!encoding) return encoding.GetStatus();
    if (magic.Value() != CompiledDocumentMagic ||
        encoding.Value() != XamlCompiledDocumentEncodingVersion) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "AXB2 encoding is not supported");
    }

    CompiledDocument document;
    Base::Result<std::uint32_t> value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.cacheFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.typeIdAlgorithmVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.metadataSchemaFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.metadataProgramFormatVersion = value.Value();
    value = decoder.ReadU32();
    if (!value) return value.GetStatus();
    document.identity_.schemaVersion = value.Value();
    Base::Result<std::uint64_t> hash = decoder.ReadU64();
    if (!hash) return hash.GetStatus();
    document.identity_.metadataSchemaHash = hash.Value();
    Base::Result<void> compatible = ValidateCompiledCacheIdentity(
        document.identity_, domain);
    if (!compatible) return compatible.GetStatus();

    Base::Result<std::uint32_t> sectionCount = decoder.ReadU32();
    if (!sectionCount) return sectionCount.GetStatus();
    if (sectionCount.Value() < 6U || sectionCount.Value() > 8U) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 section count is invalid");
    }
    Base::Vector<AxbSectionDirectoryEntry> directory;
    directory.Reserve(sectionCount.Value());
    const std::uint32_t payloadBegin =
        40U + sectionCount.Value() * 16U;
    for (std::uint32_t index = 0U; index < sectionCount.Value(); ++index) {
        Base::Result<std::uint32_t> kind = decoder.ReadU32();
        if (!kind) return kind.GetStatus();
        Base::Result<std::uint32_t> offset = decoder.ReadU32();
        if (!offset) return offset.GetStatus();
        Base::Result<std::uint32_t> size = decoder.ReadU32();
        if (!size) return size.GetStatus();
        Base::Result<std::uint32_t> count = decoder.ReadU32();
        if (!count) return count.GetStatus();
        if (kind.Value() < static_cast<std::uint32_t>(
                AxbSectionKind::Dependencies) ||
            kind.Value() > static_cast<std::uint32_t>(
                AxbSectionKind::SourceFallback) ||
            offset.Value() < payloadBegin ||
            offset.Value() > bytes.Size() ||
            size.Value() > bytes.Size() - offset.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 section directory is invalid");
        }
        for (const AxbSectionDirectoryEntry& existing : directory) {
            const bool duplicate = existing.kind ==
                static_cast<AxbSectionKind>(kind.Value());
            const bool overlap =
                offset.Value() < existing.offset + existing.size &&
                existing.offset < offset.Value() + size.Value();
            if (duplicate || overlap) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 sections are duplicated or overlapping");
            }
        }
        directory.PushBack({
            static_cast<AxbSectionKind>(kind.Value()),
            offset.Value(), size.Value(), count.Value()});
    }

    const Base::Span<const AxbSectionDirectoryEntry> directorySpan{
        directory.Data(), directory.Size()};
    const AxbSectionDirectoryEntry* dependencySection = FindSection(
        directorySpan, AxbSectionKind::Dependencies);
    const AxbSectionDirectoryEntry* stringSection = FindSection(
        directorySpan, AxbSectionKind::Strings);
    const AxbSectionDirectoryEntry* typeSection = FindSection(
        directorySpan, AxbSectionKind::Types);
    const AxbSectionDirectoryEntry* memberSection = FindSection(
        directorySpan, AxbSectionKind::Members);
    const AxbSectionDirectoryEntry* valueSection = FindSection(
        directorySpan, AxbSectionKind::Values);
    const AxbSectionDirectoryEntry* instructionSection = FindSection(
        directorySpan, AxbSectionKind::Instructions);
    const AxbSectionDirectoryEntry* sourceMapSection = FindSection(
        directorySpan, AxbSectionKind::SourceMap);
    if (dependencySection == nullptr || stringSection == nullptr ||
        typeSection == nullptr || memberSection == nullptr ||
        valueSection == nullptr || instructionSection == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 is missing a required section");
    }

    Base::Result<Decoder> opened = OpenSection(bytes, *stringSection);
    if (!opened) return opened.GetStatus();
    Decoder stringsDecoder = opened.Value();
    Base::Result<std::uint32_t> stringCount = stringsDecoder.ReadU32();
    if (!stringCount) return stringCount.GetStatus();
    if (stringCount.Value() != stringSection->count) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 string table count is invalid");
    }
    Base::Vector<Base::String> strings;
    strings.Reserve(stringCount.Value());
    std::uint32_t totalStringBytes = 0U;
    for (std::uint32_t index = 0U; index < stringCount.Value(); ++index) {
        Base::Result<Base::String> string = stringsDecoder.ReadString(
            totalStringBytes, limits.maxStringBytes);
        if (!string) return string.GetStatus();
        strings.PushBack(std::move(string).Value());
    }
    if (!stringsDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 string table has trailing data");
    }

    opened = OpenSection(bytes, *typeSection);
    if (!opened) return opened.GetStatus();
    Decoder typesDecoder = opened.Value();
    Base::Result<std::uint32_t> typeCount = typesDecoder.ReadU32();
    if (!typeCount) return typeCount.GetStatus();
    if (typeCount.Value() != typeSection->count ||
        typeCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 type table count exceeds limits");
    }
    Base::Vector<std::uint64_t> types;
    Base::Vector<CompiledTypeBinding> typeBindings;
    types.Reserve(typeCount.Value());
    typeBindings.Reserve(typeCount.Value());
    for (std::uint32_t index = 0U; index < typeCount.Value(); ++index) {
        Base::Result<std::uint64_t> id = typesDecoder.ReadU64();
        if (!id) return id.GetStatus();
        if (id.Value() == Meta::InvalidTypeId) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 type table contains an invalid type id");
        }
        Base::Result<CompiledTypeBinding> binding =
            ResolveCompiledTypeBinding(domain, id.Value());
        if (!binding) return binding.GetStatus();
        types.PushBack(id.Value());
        typeBindings.PushBack(binding.Value());
    }
    if (!typesDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 type table has trailing data");
    }

    opened = OpenSection(bytes, *memberSection);
    if (!opened) return opened.GetStatus();
    Decoder membersDecoder = opened.Value();
    Base::Result<std::uint32_t> memberCount = membersDecoder.ReadU32();
    if (!memberCount) return memberCount.GetStatus();
    if (memberCount.Value() != memberSection->count ||
        memberCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 member table count exceeds limits");
    }
    Base::Vector<CompiledMemberBinding> members;
    members.Reserve(memberCount.Value());
    for (std::uint32_t index = 0U; index < memberCount.Value(); ++index) {
        Base::Result<std::uint64_t> id =
            membersDecoder.ReadU64();
        if (!id) return id.GetStatus();
        if (id.Value() == Meta::InvalidMemberId) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 member table contains an invalid member id");
        }
        Base::Result<CompiledMemberBinding> binding =
            ResolveCompiledMemberBinding(
                domain, id.Value());
        if (!binding) return binding.GetStatus();
        members.PushBack(
            binding.Value());
    }
    if (!membersDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 member table has trailing data");
    }

    opened = OpenSection(bytes, *valueSection);
    if (!opened) return opened.GetStatus();
    Decoder valuesDecoder = opened.Value();
    Base::Result<std::uint32_t> valueCount = valuesDecoder.ReadU32();
    if (!valueCount) return valueCount.GetStatus();
    if (valueCount.Value() != valueSection->count ||
        valueCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 value table count exceeds limits");
    }
    Base::Vector<AxbValueRecord> values;
    values.Reserve(valueCount.Value());
    for (std::uint32_t index = 0U;
         index < valueCount.Value(); ++index) {
        Base::Result<std::uint8_t> kind =
            valuesDecoder.ReadU8();
        if (!kind) return kind.GetStatus();
        Base::Result<std::uint8_t> payloadCount =
            valuesDecoder.ReadU8();
        if (!payloadCount) return payloadCount.GetStatus();
        Base::Result<std::uint16_t> reservedBits =
            valuesDecoder.ReadU16();
        if (!reservedBits) return reservedBits.GetStatus();
        Base::Result<std::uint32_t> typeIndex =
            valuesDecoder.ReadU32();
        if (!typeIndex) return typeIndex.GetStatus();

        if (kind.Value() >
                static_cast<std::uint8_t>(
                    AxbValueKind::Custom) ||
            payloadCount.Value() == 0U ||
            payloadCount.Value() > AxbMaxValuePayloads ||
            reservedBits.Value() != 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 value record header is invalid");
        }

        AxbValueRecord record;
        record.kind =
            static_cast<AxbValueKind>(kind.Value());
        record.payloadCount = payloadCount.Value();
        record.typeIndex = typeIndex.Value();
        for (std::uint32_t payloadIndex = 0U;
             payloadIndex < record.payloadCount;
             ++payloadIndex) {
            Base::Result<std::uint64_t> payload =
                valuesDecoder.ReadU64();
            if (!payload) return payload.GetStatus();
            record.payload[payloadIndex] =
                payload.Value();
        }

        const bool textValue =
            record.kind == AxbValueKind::Text;
        if ((textValue &&
             record.typeIndex != InvalidTableIndex) ||
            (!textValue &&
             (record.typeIndex == InvalidTableIndex ||
              record.typeIndex >= types.Size()))) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 value record type is invalid");
        }
        if (record.kind != AxbValueKind::Custom &&
            record.payloadCount != 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 scalar value payload count is invalid");
        }
        if ((record.kind == AxbValueKind::Text ||
             record.kind == AxbValueKind::String) &&
            (record.payload[0] > UINT32_MAX ||
             record.payload[0] >= strings.Size())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 value record string is invalid");
        }
        if (record.kind == AxbValueKind::Boolean &&
            record.payload[0] > 1U) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 Boolean payload is invalid");
        }
        values.PushBack(record);
    }
    if (!valuesDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 value table has trailing data");
    }

    opened = OpenSection(bytes, *dependencySection);
    if (!opened) return opened.GetStatus();
    Decoder dependenciesDecoder = opened.Value();
    Base::Result<std::uint32_t> originIndex = dependenciesDecoder.ReadU32();
    if (!originIndex) return originIndex.GetStatus();
    Base::Result<std::uint32_t> dependencyCount =
        dependenciesDecoder.ReadU32();
    if (!dependencyCount) return dependencyCount.GetStatus();
    if (dependencyCount.Value() != dependencySection->count ||
        dependencyCount.Value() > limits.maxDependencies ||
        originIndex.Value() >= strings.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 dependency table exceeds limits");
    }
    if (!strings[originIndex.Value()].Empty()) {
        Base::Result<Base::ResourceUri> parsed = Base::ResourceUri::Parse(
            strings[originIndex.Value()].View());
        if (!parsed) return parsed.GetStatus();
        document.originUri_ = std::move(parsed).Value();
    }
    document.dependencies_.Reserve(dependencyCount.Value());
    for (std::uint32_t index = 0U; index < dependencyCount.Value(); ++index) {
        Base::Result<std::uint32_t> stringIndex =
            dependenciesDecoder.ReadU32();
        if (!stringIndex) return stringIndex.GetStatus();
        if (stringIndex.Value() >= strings.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 dependency references an invalid string");
        }
        Base::Result<Base::ResourceUri> parsed = Base::ResourceUri::Parse(
            strings[stringIndex.Value()].View());
        if (!parsed) return parsed.GetStatus();
        Base::Result<void> added = document.AddDependency(parsed.Value());
        if (!added) return added.GetStatus();
    }
    if (!dependenciesDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 dependency table has trailing data");
    }

    Base::Vector<::Aero::Diagnostics::SourceSpan> sourceMap;
    if (sourceMapSection != nullptr) {
        opened = OpenSection(bytes, *sourceMapSection);
        if (!opened) return opened.GetStatus();
        Decoder sourceDecoder = opened.Value();
        Base::Result<std::uint32_t> sourceCount =
            sourceDecoder.ReadVarU32();
        if (!sourceCount) return sourceCount.GetStatus();
        if (sourceCount.Value() != sourceMapSection->count ||
            sourceCount.Value() > limits.maxNodes) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "AXB2 source map count exceeds limits");
        }
        sourceMap.Reserve(sourceCount.Value());
        for (std::uint32_t index = 0U; index < sourceCount.Value(); ++index) {
            Base::Result<::Aero::Diagnostics::SourcePosition> begin =
                ReadPosition(sourceDecoder);
            if (!begin) return begin.GetStatus();
            Base::Result<::Aero::Diagnostics::SourcePosition> end =
                ReadPosition(sourceDecoder);
            if (!end) return end.GetStatus();
            ::Aero::Diagnostics::SourceSpan span{begin.Value(), end.Value()};
            if (!::Aero::Diagnostics::IsValidSourceSpan(span)) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 source map span is invalid");
            }
            sourceMap.PushBack(span);
        }
        if (!sourceDecoder.AtEnd()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 source map has trailing data");
        }
    }

    opened = OpenSection(bytes, *instructionSection);
    if (!opened) return opened.GetStatus();
    Decoder instructionsDecoder = opened.Value();
    Base::Result<std::uint32_t> instructionCount =
        instructionsDecoder.ReadVarU32();
    if (!instructionCount) return instructionCount.GetStatus();
    if (instructionCount.Value() == 0U ||
        instructionCount.Value() != instructionSection->count ||
        instructionCount.Value() > limits.maxNodes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "AXB2 instruction count exceeds limits");
    }
    if (sourceMapSection != nullptr &&
        sourceMap.Size() != instructionCount.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 source map is not aligned with instructions");
    }

    document.nodes_.Reserve(instructionCount.Value());
    for (std::uint32_t index = 0U;
         index < instructionCount.Value(); ++index) {
        Base::Result<std::uint8_t> kind =
            instructionsDecoder.ReadU8();
        if (!kind) return kind.GetStatus();
        Base::Result<std::uint8_t> flags =
            instructionsDecoder.ReadU8();
        if (!flags) return flags.GetStatus();
        if (kind.Value() ==
                static_cast<std::uint8_t>(
                    NodeKind::None) ||
            kind.Value() >
                static_cast<std::uint8_t>(
                    NodeKind::EndOfDocument)) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 instruction kind is invalid");
        }

        const NodeKind nodeKind =
            static_cast<NodeKind>(kind.Value());
        if (!IsInstructionShapeValid(
                nodeKind, flags.Value())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 instruction shape is invalid");
        }

        std::uint32_t typeIndex =
            InvalidTableIndex;
        std::uint32_t memberIndex =
            InvalidTableIndex;
        std::uint32_t valueIndex =
            InvalidTableIndex;
        std::uint32_t qualifiedName[3]{
            InvalidTableIndex,
            InvalidTableIndex,
            InvalidTableIndex};
        std::uint32_t namespacePair[2]{
            InvalidTableIndex,
            InvalidTableIndex};

        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasType)) {
            Base::Result<std::uint32_t> read =
                instructionsDecoder.ReadVarU32();
            if (!read) return read.GetStatus();
            typeIndex = read.Value();
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasMember)) {
            Base::Result<std::uint32_t> read =
                instructionsDecoder.ReadVarU32();
            if (!read) return read.GetStatus();
            memberIndex = read.Value();
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasValue)) {
            Base::Result<std::uint32_t> read =
                instructionsDecoder.ReadVarU32();
            if (!read) return read.GetStatus();
            valueIndex = read.Value();
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasQualifiedName)) {
            for (std::uint32_t& stringIndex :
                 qualifiedName) {
                Base::Result<std::uint32_t> read =
                    instructionsDecoder.ReadVarU32();
                if (!read) return read.GetStatus();
                stringIndex = read.Value();
            }
        }
        if (HasInstructionFlag(
                flags.Value(),
                AxbInstructionHasNamespace)) {
            for (std::uint32_t& stringIndex :
                 namespacePair) {
                Base::Result<std::uint32_t> read =
                    instructionsDecoder.ReadVarU32();
                if (!read) return read.GetStatus();
                stringIndex = read.Value();
            }
        }

        if ((typeIndex != InvalidTableIndex &&
             typeIndex >= types.Size()) ||
            (memberIndex != InvalidTableIndex &&
             memberIndex >= members.Size()) ||
            (valueIndex != InvalidTableIndex &&
             valueIndex >= values.Size())) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "AXB2 instruction operand is outside its table");
        }
        for (std::uint32_t stringIndex :
             qualifiedName) {
            if (stringIndex != InvalidTableIndex &&
                stringIndex >= strings.Size()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 qualified name index is invalid");
            }
        }
        for (std::uint32_t stringIndex :
             namespacePair) {
            if (stringIndex != InvalidTableIndex &&
                stringIndex >= strings.Size()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "AXB2 namespace index is invalid");
            }
        }

        Node node;
        node.kind_ = nodeKind;
        node.fromAttribute_ =
            HasInstructionFlag(
                flags.Value(),
                AxbInstructionFromAttribute);
        if (typeIndex != InvalidTableIndex) {
            node.BindCompiledType(typeBindings[typeIndex]);
        }
        if (memberIndex != InvalidTableIndex) {
            node.BindCompiledMember(
                members[memberIndex]);
        }

        Base::Result<void> assigned;
        if (qualifiedName[0] != InvalidTableIndex) {
            assigned = AssignInternedString(
                node.name_.prefix_,
                strings.AsSpan(),
                qualifiedName[0]);
            if (!assigned) return assigned.GetStatus();
            assigned = AssignInternedString(
                node.name_.localName_,
                strings.AsSpan(),
                qualifiedName[1]);
            if (!assigned) return assigned.GetStatus();
            assigned = AssignInternedString(
                node.name_.namespaceUri_,
                strings.AsSpan(),
                qualifiedName[2]);
            if (!assigned) return assigned.GetStatus();
        }
        if (namespacePair[0] != InvalidTableIndex) {
            assigned = AssignInternedString(
                node.namespacePrefix_,
                strings.AsSpan(),
                namespacePair[0]);
            if (!assigned) return assigned.GetStatus();
            assigned = AssignInternedString(
                node.namespaceUri_,
                strings.AsSpan(),
                namespacePair[1]);
            if (!assigned) return assigned.GetStatus();
        }
        if (valueIndex != InvalidTableIndex) {
            const AxbValueRecord& stored =
                values[valueIndex];
            if (stored.kind == AxbValueKind::Text) {
                assigned = node.value_.Assign(
                    strings[static_cast<std::uint32_t>(
                        stored.payload[0])].View());
                if (!assigned) {
                    return assigned.GetStatus();
                }
            } else {
                Base::Result<Meta::Value> decoded =
                    DecodeCompiledValue(
                        stored,
                        types.AsSpan(),
                        strings.AsSpan());
                if (!decoded) {
                    return decoded.GetStatus();
                }
                node.BindCompiledValue(
                    std::move(decoded).Value());
            }
        }
        if (!sourceMap.Empty()) {
            node.source_ = sourceMap[index];
        }
        document.nodes_.PushBack(
            std::move(node));
    }
    if (!instructionsDecoder.AtEnd()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 instruction stream has trailing data");
    }
    if (!document.originUri_.Empty()) {
        bool originListed = false;
        for (const Base::ResourceUri& dependency :
             document.dependencies_) {
            originListed =
                originListed ||
                dependency == document.originUri_;
        }
        if (!originListed) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML origin is absent from dependencies");
        }
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "AXB2 instruction stream is incomplete");
    }
    return document;
}

} // namespace Aero::Markup



// ============================================================================
// SECTION: TemplateCompiler / Blueprints / TriggerPlans
// Source: src/gui/markup/TemplateCompiler.cpp
// ============================================================================

// ===== TemplateCompiler =====






namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace Aero::Media;
using Controls::Primitives::Selector;
using namespace Data;
using Documents::Inline; // not Documents:: -- avoid Span collision with Aero::Span
using namespace Media::Animation;

Base::Status InvalidTemplateCompiler(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Visual* WalkTemplateAncestor(
    Visual* start,
    Base::StringView ancestorType,
    std::uint32_t level,
    const Meta::TypeRegistry& types) noexcept {
    if (start == nullptr || level == 0U) return nullptr;
    Base::StringView ancestorName = ancestorType;
    for (std::uint32_t index = 0U; index < ancestorName.SizeBytes(); ++index) {
        if (ancestorName[index] == ':') {
            ancestorName = ancestorName.Substr(
                index + 1U, ancestorName.SizeBytes() - index - 1U);
            break;
        }
    }
    std::uint32_t matched = 0U;
    Visual* current = start->GetVisualParent();
    if (current == nullptr) current = TryCast<Visual>(start->GetLogicalParent());
    if (current == nullptr) {
        if (auto* fe = TryCast<FrameworkElement>(start)) {
            current = TryCast<Visual>(fe->GetTemplatedParent());
        }
    }
    while (current != nullptr) {
        const TypeInfo* type = types.FindType(current->RuntimeType());
        if ((ancestorName.Empty() ||
             (type != nullptr && type->Name() == ancestorName)) &&
            ++matched == level) {
            return current;
        }
        Visual* next = current->GetVisualParent();
        if (next == nullptr) next = TryCast<Visual>(current->GetLogicalParent());
        if (next == nullptr) {
            if (auto* fe = TryCast<FrameworkElement>(current)) {
                next = TryCast<Visual>(fe->GetTemplatedParent());
            }
        }
        current = next;
    }
    return nullptr;
}

Base::Object* ResolveTemplateBindingAncestor(
    const TemplatePrototypeBinding& binding,
    Base::Object& target,
    Meta::Registry& runtime) noexcept {
    if (binding.relativeAncestorType.Empty() ||
        binding.relativeAncestorLevel == 0U ||
        !runtime.Types().IsDerivedFrom(
            target.RuntimeType(), Visual::StaticTypeId())) {
        return nullptr;
    }
    return WalkTemplateAncestor(
        static_cast<Visual*>(&target),
        binding.relativeAncestorType.View(),
        binding.relativeAncestorLevel,
        runtime.Types());
}

struct PendingPrototypeNode {
    Base::Ref<Base::Object> object;
    std::uint32_t parent = UINT32_MAX;
    MemberId contentMember = InvalidMemberId;
};

bool RequiresPrototypeObject(
    Base::Span<const DeferredBindingEdge> bindings,
    const Base::Object* object) noexcept {
    for (const DeferredBindingEdge& binding : bindings) {
        if (binding.target == object ||
            binding.source == object) {
            return true;
        }
    }
    return false;
}

bool HasContentEdges(
    Base::Span<const DeferredContentEdge> edges,
    const Base::Object* object) noexcept {
    for (const DeferredContentEdge& edge : edges) {
        if (edge.parent == object) {
            return true;
        }
    }
    return false;
}

Base::Result<Value> ConvertTemplateTextValue(
    Meta::Registry& runtime,
    const DependencyProperty& property,
    Base::StringView text) noexcept {
    // WPF uses -1 for no selection. Internally selection keeps UINT32_MAX;
    // translate only the standard selection properties at the XAML boundary.
    if ((property.Handle() == Selector::SelectedIndexProperty.Handle() ||
         property.Handle() == TabControl::SelectedIndexProperty.Handle()) &&
        text == Base::StringView("-1")) {
        return ValueCodec<std::uint32_t>::Encode(UINT32_MAX);
    }
    if (property.ValueType() == Meta::TypeOf<Base::Object>()) {
        Base::Result<Value> strVal =
            Value::TryFromString(Meta::TypeOf<Base::String>(), text);
        if (!strVal) return strVal.GetStatus();
        Base::Result<Base::Ref<Controls::BoxedItemValue>> boxed =
            Base::MakeRef<Controls::BoxedItemValue>(std::move(strVal).Value());
        if (!boxed) return boxed.GetStatus();
        return Value::FromObject(
            property.ValueType(),
            Base::Ref<Base::Object>(std::move(boxed).Value()));
    }
    if (property.ValueType() == Meta::TypeOf<::Aero::Nullable<bool>>()) {
        if (text == Base::StringView("True") || text == Base::StringView("true")) {
            return ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{true});
        }
        if (text == Base::StringView("False") || text == Base::StringView("false")) {
            return ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{false});
        }
        if (text == Base::StringView("{x:Null}") || text == Base::StringView("Null") || text == Base::StringView("null")) {
            return ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{});
        }
    }
    return runtime.TryConvertText(property.ValueType(), text);
}

inline const DependencyProperty* ResolveTemplateProperty(
    const DependencyPropertyRegistry& properties,
    TypeId targetType,
    Base::StringView name) noexcept {
    return ResolveXamlDependencyProperty(properties, targetType, name);
}

std::uint32_t FindPrototypeObject(
    const Base::Vector<PendingPrototypeNode>& pending,
    const Base::Object* object) noexcept {
    for (std::uint32_t index = 0U;
         index < pending.Size();
         ++index) {
        if (pending[index].object.Get() == object) {
            return index;
        }
    }
    return UINT32_MAX;
}

bool IsGradientStopObject(
    Meta::Registry& runtime,
    const Base::Object& object) noexcept {
    return runtime.Types().IsDerivedFrom(
        object.RuntimeType(),
        GradientStop::StaticTypeId());
}


Base::Result<CompiledTemplateBlueprint>
CompileBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Base::Span<const TemplateMetadataBindingPlan>
        metadataBindings,
    Base::Span<const TemplateDynamicResourcePlan>
        dynamicResources,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (!visualTree) {
        return InvalidTemplateCompiler(
            "Template requires a VisualTree");
    }

    CompiledTemplateBlueprint blueprint;
    blueprint.runtime = &runtime;
    blueprint.properties = &properties;

    Base::Vector<PendingPrototypeNode> pending;
    Base::Result<void> appended{};
    pending.PushBack({
            visualTree,
            UINT32_MAX,
            InvalidMemberId});

    // Templated-parent bindings may target Freezables nested in a visual
    // property (for example a GradientStop.Color binding). Visuals and
    // TransformGroup children are discovered from content edges so authored
    // order is preserved; pre-inserting RotateTransform made clock hands
    // compile as parentless nodes and drop out of TransformGroup.Children.
    auto preInsertNamedTarget = [&](Base::StringView targetName, const char* targetTypeDesc) -> Base::Result<void> {
        if (targetName.Empty() || names == nullptr) return {};
        Base::Object* target = names->Find(targetName);
        if (target == nullptr ||
            IsGradientStopObject(runtime, *target) ||
            runtime.Types().IsDerivedFrom(
                target->RuntimeType(), Visual::StaticTypeId()) ||
            runtime.Types().IsDerivedFrom(
                target->RuntimeType(), Transform::StaticTypeId())) {
            return {};
        }
        if (!runtime.Types().IsDerivedFrom(
                target->RuntimeType(), DependencyObject::StaticTypeId())) {
            thread_local char message[128];
            std::snprintf(message, sizeof(message), "%s target is not a DependencyObject", targetTypeDesc);
            return InvalidTemplateCompiler(message);
        }
        if (FindPrototypeObject(pending, target) != UINT32_MAX) return {};
        pending.PushBack({
            Base::Ref<Base::Object>::FromBorrowed(*target),
            UINT32_MAX,
            InvalidMemberId});
        return {};
    };

    for (const TemplateMetadataBindingPlan& binding : metadataBindings) {
        Base::Result<void> res = preInsertNamedTarget(binding.targetName.View(), "TemplatedParent Binding");
        if (!res) return res.GetStatus();
    }
    for (const TemplateDynamicResourcePlan& resource : dynamicResources) {
        Base::Result<void> res = preInsertNamedTarget(resource.targetName.View(), "DynamicResource");
        if (!res) return res.GetStatus();
    }

    for (std::uint32_t index = 0U;
         index < pending.Size(); ++index) {
        const PendingPrototypeNode& source =
            pending[index];
        Base::Object* object = source.object.Get();
        if (object == nullptr ||
            !runtime.Types().IsDerivedFrom(
                object->RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Template graph contains a non-dependency object");
        }
        const bool visual = runtime.Types().IsDerivedFrom(
            object->RuntimeType(), Visual::StaticTypeId());
        if (index == 0U && !visual) {
            return InvalidTemplateCompiler(
                "Template VisualTree root must be a Visual object");
        }

        TemplatePrototypeNode node;
        node.type = object->RuntimeType();
        node.parent = source.parent;
        node.contentMember = source.contentMember;
        const Base::StringView name = names != nullptr
            ? names->NameOf(*object)
            : Base::StringView{};
        Base::Result<void> named =
            node.name.Assign(name);
        if (!named) return named.GetStatus();

        if (auto* framework = ::Aero::TryCast<FrameworkElement>(object)) {
            for (const auto& b : FrameworkElementSeams::AuthoredBehaviors(*framework)) {
                node.authoredBehaviors.PushBack(b);
            }
            for (const auto& t : FrameworkElementSeams::AuthoredTriggers(*framework)) {
                node.authoredTriggers.PushBack(t);
            }
        }

        auto& dependencyObject =
            static_cast<DependencyObject&>(*object);
        const PropertyStore* store = DependencyObjectAccess::Store(dependencyObject);
        if (store != nullptr) {
            for (const auto& entry : store->entries) {
                if (!entry.Value().HasLocal()) {
                    continue;
                }
                const DependencyProperty* property =
                    properties.Find(DependencyPropertyHandle{entry.Key()});
                if (property == nullptr || property->MetadataFor(node.type) == nullptr) {
                    continue;
                }
                Base::Result<Value> local =
                    dependencyObject.ReadLocalValue(
                        property->Handle());
                if (!local) return local.GetStatus();
                if (local.Value().IsUnset()) continue;
                TemplatePrototypeProperty prototypeProperty;
                prototypeProperty.property = property->Handle();
                prototypeProperty.value = local.Value();
                if (local.Value().Kind() == ValueKind::Object &&
                    !local.Value().IsNullObject() &&
                    local.Value().AsObject() &&
                    runtime.Types().IsDerivedFrom(
                        local.Value().AsObject()->RuntimeType(),
                        DependencyObject::StaticTypeId())) {
                    prototypeProperty.objectNode = FindPrototypeObject(
                        pending, local.Value().AsObject().Get());
                    const bool hasEdges = HasContentEdges(
                        edges, local.Value().AsObject().Get());
                    const bool isFreezable = runtime.Types().IsDerivedFrom(
                        local.Value().AsObject()->RuntimeType(),
                        Freezable::StaticTypeId());
                    const bool requiresClone =
                        prototypeProperty.objectNode != UINT32_MAX ||
                        RequiresPrototypeObject(
                            bindings, local.Value().AsObject().Get()) ||
                        hasEdges ||
                        isFreezable;
                    if (requiresClone &&
                        prototypeProperty.objectNode == UINT32_MAX) {
                        prototypeProperty.objectNode = pending.Size();
                        pending.PushBack({
                            local.Value().AsObject(),
                            UINT32_MAX,
                            InvalidMemberId});
                    }
                }
                node.properties.PushBack(
                    std::move(prototypeProperty));
            }
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                Grid::StaticTypeId())) {
            const auto& grid =
                static_cast<const Grid&>(*object);
            node.gridColumns.Assign(
                grid.GetColumnDefinitions());
            node.gridRows.Assign(
                grid.GetRowDefinitions());
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                StreamGeometry::StaticTypeId())) {
            const auto& geom =
                static_cast<const StreamGeometry&>(*object);
            appended = node.streamGeometryData.Assign(
                geom.GetData());
            if (!appended) {
                return appended.GetStatus();
            }
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                GradientBrush::StaticTypeId())) {
            const auto& gradient =
                static_cast<const GradientBrush&>(*object);
            for (const Base::Ref<GradientStop>& stop : gradient.GetGradientStops()) {
                if (!stop) continue;
                TemplatePrototypeGradientStop record;
                record.offset = stop->GetOffset();
                record.color = stop->GetColor();
                if (names != nullptr) {
                    appended = record.name.Assign(
                        names->NameOf(*stop));
                    if (!appended) return appended.GetStatus();
                }
                node.gradientStops.PushBack(
                    std::move(record));
            }
        }

        const bool contentPresenterCandidate =
            runtime.Types().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId()) ||
            runtime.Types().IsDerivedFrom(
                node.type,
                ScrollContentPresenter::StaticTypeId());
        if (contentPresenterCandidate) {
            constexpr Base::StringView ScrollPresenterPart(
                "PART_ScrollContentPresenter");
            constexpr Base::StringView HeaderSource("Header");
            Base::StringView contentSource;
            for (const TemplatePrototypeProperty& property :
                 node.properties) {
                if (property.property ==
                        ContentPresenter::ContentSourceProperty.Handle() &&
                    property.value.Kind() == ValueKind::String) {
                    contentSource = property.value.AsString();
                    break;
                }
            }
            const bool headerPresenter = contentSource == HeaderSource;
            const bool explicitScrollPresenter =
                node.name.View() == ScrollPresenterPart;
            const bool exactScrollPresenter =
                node.type == ScrollContentPresenter::StaticTypeId();
            // ContentSource=Header presenters display Header, not Content.
            // Projecting Content onto them is wrong and can cycle when the
            // only available "content" is the template root itself.
            if (!headerPresenter ||
                explicitScrollPresenter ||
                exactScrollPresenter) {
                bool replace = blueprint.contentPresenter == UINT32_MAX;
                if (!replace &&
                    blueprint.contentPresenter < blueprint.nodes.Size()) {
                    const TemplatePrototypeNode& selected =
                        blueprint.nodes[blueprint.contentPresenter];
                    const bool selectedExplicit =
                        selected.name.View() == ScrollPresenterPart;
                    const bool selectedExact =
                        selected.type == ScrollContentPresenter::StaticTypeId();
                    replace = explicitScrollPresenter ||
                        (!selectedExplicit && exactScrollPresenter &&
                         !selectedExact);
                }
                if (replace) {
                    blueprint.contentPresenter = index;
                }
            }
        }

        blueprint.nodes.PushBack(
            std::move(node));

        for (const DeferredContentEdge& edge :
             edges) {
            if (edge.parent != object) continue;
            if (!edge.child) {
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a null child");
            }
            if (IsGradientStopObject(runtime, *edge.child)) {
                // GradientStops are cloned from the authored brush collection
                // so TemplatedParent Color bindings target the instance that
                // actually lives in the applied LinearGradientBrush.
                continue;
            }
            const std::uint32_t existing =
                FindPrototypeObject(pending, edge.child.Get());
            if (existing != UINT32_MAX) {
                if (existing == index) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate visual content contains a cycle");
                }
                PendingPrototypeNode& existingNode = pending[existing];
                if (existingNode.parent == UINT32_MAX) {
                    // A dependency object can first enter the prototype graph
                    // because a Binding or object-valued property references
                    // it, and later appear on its actual visual-content edge.
                    // Retain one clone and attach that clone at the authored
                    // location instead of treating the two discovery paths as
                    // duplicate visual content.
                    existingNode.parent = index;
                    existingNode.contentMember = edge.member;
                    if (existing < blueprint.nodes.Size()) {
                        blueprint.nodes[existing].parent = index;
                        blueprint.nodes[existing].contentMember = edge.member;
                    }
                    continue;
                }
                if (existingNode.parent == index &&
                    existingNode.contentMember == edge.member) {
                    // Structural and content facets can report the same edge.
                    // It is one authored child, not two template instances.
                    continue;
                }
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a duplicate child");
            }
            pending.PushBack({
                edge.child,
                index,
                edge.member});
        }

        // String inlines (Hyperlink "Show XAML", inter-Run spaces) live on
        // TextBlock/Span owned collections and may not have DeferredContentEdge
        // records. Clone them so ControlTemplate instances keep the text.
        if (runtime.Types().IsDerivedFrom(
                object->RuntimeType(),
                TextBlock::StaticTypeId()) ||
            runtime.Types().IsDerivedFrom(
                object->RuntimeType(),
                Documents::Span::StaticTypeId())) {
            const Meta::PropertyInfo* inlinesProperty =
                runtime.Types().FindProperty(
                    object->RuntimeType(),
                    Base::StringView("Inlines"),
                    true);
            const MemberId inlinesMember =
                inlinesProperty != nullptr
                ? inlinesProperty->Id()
                : InvalidMemberId;
            const std::uint32_t inlineCount =
                runtime.Types().IsDerivedFrom(
                    object->RuntimeType(),
                    TextBlock::StaticTypeId())
                ? static_cast<TextBlock&>(*object).GetInlineCount()
                : static_cast<Documents::Span&>(*object).GetInlines().GetCount();
            for (std::uint32_t inlineIndex = 0U;
                 inlineIndex < inlineCount;
                 ++inlineIndex) {
                Inline* inlineValue = nullptr;
                if (runtime.Types().IsDerivedFrom(
                        object->RuntimeType(),
                        TextBlock::StaticTypeId())) {
                    inlineValue = static_cast<TextBlock&>(*object)
                        .GetInlines().GetItem(inlineIndex);
                } else {
                    inlineValue = static_cast<Documents::Span&>(*object)
                        .GetInlines().GetItem(inlineIndex);
                }
                if (inlineValue == nullptr) {
                    continue;
                }
                const std::uint32_t existing =
                    FindPrototypeObject(pending, inlineValue);
                if (existing != UINT32_MAX) {
                    PendingPrototypeNode& existingNode = pending[existing];
                    if (existingNode.parent == UINT32_MAX) {
                        existingNode.parent = index;
                        existingNode.contentMember = inlinesMember;
                        if (existing < blueprint.nodes.Size()) {
                            blueprint.nodes[existing].parent = index;
                            blueprint.nodes[existing].contentMember =
                                inlinesMember;
                        }
                    }
                    continue;
                }
                pending.PushBack({
                    Base::Ref<Base::Object>::FromBorrowed(*inlineValue),
                    index,
                    inlinesMember});
            }
        }
    }

    // Generated names from EnsureAuthoredName live on the template NameScope.
    // Unnamed TemplatedParent Binding targets must carry that name onto the
    // prototype node so apply-time FindTarget can find the part. WPF binds the
    // unnamed child itself, not the templated parent.
    auto assignGeneratedTargetName = [&](Base::StringView targetName) -> Base::Result<void> {
        if (targetName.Empty() || names == nullptr) return {};
        Base::Object* target = names->Find(targetName);
        if (target == nullptr) return {};
        const std::uint32_t targetIndex = FindPrototypeObject(pending, target);
        if (targetIndex == UINT32_MAX || targetIndex >= blueprint.nodes.Size()) return {};
        if (!blueprint.nodes[targetIndex].name.Empty()) return {};
        return blueprint.nodes[targetIndex].name.Assign(targetName);
    };

    for (const TemplateMetadataBindingPlan& binding : metadataBindings) {
        Base::Result<void> named = assignGeneratedTargetName(binding.targetName.View());
        if (!named) return named.GetStatus();
    }
    for (const TemplateDynamicResourcePlan& resource : dynamicResources) {
        Base::Result<void> named = assignGeneratedTargetName(resource.targetName.View());
        if (!named) return named.GetStatus();
    }

    for (const DeferredBindingEdge& source :
         bindings) {
        const std::uint32_t target =
            FindPrototypeObject(
                pending, source.target);
        Base::Object* resolvedSource = source.source;
        if (resolvedSource == nullptr &&
            !source.sourceName.Empty() && names != nullptr) {
            resolvedSource = names->Find(source.sourceName.View());
        }
        const std::uint32_t bindingSource =
            resolvedSource != nullptr
            ? FindPrototypeObject(pending, resolvedSource)
            : UINT32_MAX;
        const bool externalElementName =
            resolvedSource == nullptr &&
            !source.sourceName.Empty();
        if (target == UINT32_MAX ||
            (source.source != nullptr && bindingSource == UINT32_MAX) ||
            source.metadata == nullptr) {
            return InvalidTemplateCompiler(
                "Deferred template Binding target or source is outside its VisualTree");
        }
        TemplatePrototypeBinding binding;
        binding.target = target;
        binding.source = bindingSource;
        binding.metadata = source.metadata;
        binding.targetProperty =
            source.targetProperty;
        binding.dataContextProperty =
            source.dataContextProperty;
        binding.bindsToSource =
            source.bindsToSource;
        binding.mode = source.mode;
        binding.updateSourceTrigger =
            source.updateSourceTrigger;
        binding.converter = source.converter;
        binding.converterParameter = source.converterParameter;
        if (externalElementName) {
            Base::Result<void> sourceName =
                binding.sourceName.Assign(
                    source.sourceName.View());
            if (!sourceName) return sourceName.GetStatus();
        }
        Base::Result<void> assigned =
            binding.relativeAncestorType.Assign(
            source.relativeAncestorType.View());
        if (!assigned) return assigned.GetStatus();
        binding.relativeAncestorLevel =
            source.relativeAncestorLevel;
        assigned = binding.path.Assign(
                source.path.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = binding.stringFormat.Assign(
            source.stringFormat.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        blueprint.bindings.PushBack(
            std::move(binding));
    }

    return blueprint;
}



Base::Result<Value> ConvertAndValidateTemplateValue(
    Value value,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    const char* roleName) noexcept {
    if (value.IsUnset()) {
        thread_local char message[128];
        std::snprintf(message, sizeof(message), "%s requires Value", roleName);
        return InvalidTemplateCompiler(message);
    }
    if (value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Binding::StaticTypeId()) {
        return value;
    }
    if (value.IsDynamicResource()) {
        return value;
    }
    if (property.AcceptsAnyValue()) {
        return value;
    }
    if (value.Kind() == ValueKind::String &&
        value.Type() != property.ValueType()) {
        Base::Result<Value> converted =
            ConvertTemplateTextValue(
                runtime, property, value.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        value = std::move(converted).Value();
    }
    Base::Result<Value> coerced = CoerceXamlValueForProperty(
        property, value, runtime.Types());
    if (!coerced) return coerced.GetStatus();
    value = std::move(coerced).Value();

    const bool objectCompatible =
        value.Kind() == ValueKind::Object &&
        (value.IsNullObject() ||
         runtime.Types().IsDerivedFrom(
             value.Type(),
             property.ValueType()));
    if (value.Type() != property.ValueType() &&
        !objectCompatible) {
        thread_local char message[256];
        const TypeInfo* expected = runtime.Types().FindType(
            property.ValueType());
        const TypeInfo* actual = runtime.Types().FindType(value.Type());
        std::snprintf(
            message, sizeof(message),
            "%s '%.*s' expects '%.*s' but received '%.*s'",
            roleName,
            static_cast<int>(property.Name().SizeBytes()), property.Name().Data(),
            expected != nullptr ? static_cast<int>(expected->Name().SizeBytes()) : 9,
            expected != nullptr ? expected->Name().Data() : "<unknown>",
            actual != nullptr ? static_cast<int>(actual->Name().SizeBytes()) : 9,
            actual != nullptr ? actual->Name().Data() : "<unknown>");
        return InvalidTemplateCompiler(message);
    }
    Base::Result<void> valid =
        properties.ValidateValueFor(
            property.Handle(),
            targetType,
            value);
    return valid
        ? Base::Result<Value>(value)
        : Base::Result<Value>(
              valid.GetStatus());
}

Base::Result<Value> ConvertSetterValue(
    const Setter& setter,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return ConvertAndValidateTemplateValue(
        setter.GetAuthoredValue(), targetType, property, runtime, properties, "Setter");
}

Base::Status MissingTemplateProperty(
    const char* role,
    Base::StringView property,
    TypeId targetType,
    const TypeRegistry& types) noexcept {
    thread_local char message[384];
    const TypeInfo* target =
        types.FindType(targetType);
    const Base::StringView typeName =
        target != nullptr
        ? target->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "ControlTemplate %s '%.*s' was not found on TargetType '%.*s'",
        role,
        static_cast<int>(property.SizeBytes()),
        property.Data(),
        static_cast<int>(typeName.SizeBytes()),
        typeName.Data());
    return InvalidTemplateCompiler(message);
}

Base::Result<Value> ConvertTriggerValue(
    Value value,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return ConvertAndValidateTemplateValue(
        value, targetType, property, runtime, properties, "Template Trigger");
}

Base::Result<Base::Vector<TemplatePropertyTrigger>>
CompilePropertyTriggers(
    ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<TemplatePropertyTrigger> compiled;
    auto addCondition = [&controlTemplate, &program, &runtime, &properties](
        Base::StringView propertyName,
        Base::StringView sourceName,
        const Value& authoredValue,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        if (propertyName.Empty() || authoredValue.IsUnset()) {
            return InvalidTemplateCompiler(
                "ControlTemplate Trigger condition requires Property and Value");
        }
        TypeId targetType = controlTemplate.GetTargetType();
        if (!sourceName.Empty()) {
            const std::uint32_t slot = program.FindSlotByName(sourceName);
            if (slot == UINT32_MAX) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger source was not found");
            }
            targetType = program.GetSlotType(slot);
        }
        const DependencyProperty* property =
            ResolveTemplateProperty(properties, targetType, propertyName);
        if (property == nullptr) {
            return MissingTemplateProperty(
                "Trigger property", propertyName, targetType, runtime.Types());
        }
        Base::Result<Value> value = ConvertTriggerValue(
            authoredValue, targetType, *property, runtime, properties);
        if (!value) return value.GetStatus();
        TemplateTriggerCondition condition;
        Base::Result<void> assigned =
            condition.sourceName.Assign(sourceName);
        if (!assigned) return assigned.GetStatus();
        condition.property = property->Handle();
        condition.value = std::move(value).Value();
        trigger.conditions.PushBack(std::move(condition));
        return {};
    };
    auto appendSetters = [&controlTemplate, &program, &runtime, &properties](
        Base::Span<const Base::Ref<Setter>> setters,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        for (const Base::Ref<Setter>& setterObject : setters) {
            if (!setterObject || setterObject->GetPropertyName().Empty()) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter requires Property");
            }
            const Base::StringView targetName = setterObject->GetTargetName();
            TypeId targetType = controlTemplate.GetTargetType();
            if (!targetName.Empty()) {
                const std::uint32_t slot = program.FindSlotByName(targetName);
                if (slot == UINT32_MAX) {
                    // RowDefinition/ColumnDefinition and similar named
                    // declaration objects do not belong to the visual clone
                    // graph. They remain represented by the owning Grid's
                    // serialized definition vectors; their trigger mutation
                    // is deferred until that declaration-object runtime is
                    // materialized.
                    if (FrameworkTemplateState::AuthoredNames(controlTemplate).Find(
                            targetName) != nullptr) {
                        continue;
                    }
                    return InvalidTemplateCompiler(
                        "ControlTemplate Trigger Setter target was not found");
                }
                targetType = program.GetSlotType(slot);
            }
            const DependencyProperty* property = ResolveTemplateProperty(
                properties, targetType, setterObject->GetPropertyName());
            if (property == nullptr) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter property was not found");
            }
            Base::Result<Value> value = ConvertSetterValue(
                *setterObject,
                targetType,
                *property, runtime, properties);
            if (!value) return value.GetStatus();
            TemplateTriggerSetter setter;
            Base::Result<void> assigned = setter.targetName.Assign(targetName);
            if (!assigned) return assigned.GetStatus();
            setter.property = property->Handle();
            setter.value = std::move(value).Value();
            trigger.setters.PushBack(std::move(setter));
        }
        return {};
    };
    for (const Base::Ref<Base::Object>& object :
         FrameworkTemplateState::AuthoredTriggers(controlTemplate)) {
        TemplatePropertyTrigger trigger;
        Base::Result<void> configured;
        if (object && object->RuntimeType() == Trigger::StaticTypeId()) {
            const auto& source = static_cast<const Trigger&>(*object);
            configured = addCondition(source.GetPropertyName(), source.GetSourceName(),
                source.GetAuthoredValue(), trigger);
            if (configured) configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else if (object && object->RuntimeType() == MultiTrigger::StaticTypeId()) {
            const auto& source = static_cast<const MultiTrigger&>(*object);
            for (const Base::Ref<Condition>& condition : source.GetConditions()) {
                if (!condition) return InvalidTemplateCompiler(
                    "MultiTrigger contains a null Condition");
                configured = addCondition(condition->GetPropertyName(), condition->GetSourceName(),
                    condition->GetAuthoredValue(), trigger);
                if (!configured) return configured.GetStatus();
            }
            configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else {
            // Data and event triggers are compiled into the instance runtime
            // plan below. They cannot be represented by the dependency-
            // property trigger table used by TemplateEngine.
            continue;
        }
        if (!configured) return configured.GetStatus();
        Base::Result<void> added{};
        compiled.PushBack(
                std::move(trigger));
    }
    return compiled;
}

Base::Result<Base::Vector<VisualStateGroupPlan>>
CompileVisualStates(
    ControlTemplate& controlTemplate,
    const XamlBytecodeProgram& program,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<VisualStateGroupPlan> groups;
    auto compileGroup = [&groups, &program, &runtime, &properties](
        const Base::Ref<Base::Object>& groupObject)
        -> Base::Result<void> {
        if (!groupObject ||
            groupObject->RuntimeType() !=
                VisualStateGroup::
                    StaticTypeId()) {
            return InvalidTemplateCompiler(
                "ControlTemplate VisualStateGroups contains an invalid object");
        }
        const auto& sourceGroup =
            static_cast<
                const VisualStateGroup&>(
                    *groupObject);
        if (sourceGroup.GetName().Empty()) {
            return InvalidTemplateCompiler(
                "VisualStateGroup requires Name");
        }
        VisualStateGroupPlan group;
        Base::Result<void> assigned =
            group.name.Assign(
                sourceGroup.GetName());
        if (!assigned) return assigned.GetStatus();

        for (const Base::Ref<VisualState>& stateObject :
             sourceGroup.GetStates()) {
            if (!stateObject ||
                stateObject->RuntimeType() !=
                    VisualState::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualState");
            }
            const auto& sourceState =
                static_cast<
                const VisualState&>(
                    *stateObject);
            if (sourceState.GetName().Empty()) {
                return InvalidTemplateCompiler(
                    "VisualState requires Name");
            }
            VisualStatePlan state;
            assigned = state.name.Assign(
                sourceState.GetName());
            if (!assigned) {
                return assigned.GetStatus();
            }
            if (auto srcStoryboard = sourceState.GetStoryboard()) {
                auto storyboardCopy = srcStoryboard->Clone();
                if (!storyboardCopy) {
                    storyboardCopy = Base::MakeRef<Storyboard>();
                }
                state.storyboard = std::move(storyboardCopy);
            } else {
                state.storyboard.Reset();
            }

            for (const Base::Ref<Base::Object>&
                     setterObject :
                 sourceState.GetSetters()) {
                if (!setterObject ||
                    setterObject->RuntimeType() !=
                        Setter::StaticTypeId()) {
                    return InvalidTemplateCompiler(
                        "VisualState contains an invalid Setter");
                }
                const auto& sourceSetter =
                    static_cast<const Setter&>(
                        *setterObject);
                if (sourceSetter.GetTargetName().Empty() ||
                    sourceSetter.GetPropertyName().Empty()) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter requires TargetName and Property");
                }
                const std::uint32_t slot =
                    program.FindSlotByName(
                        sourceSetter.GetTargetName());
                if (slot == UINT32_MAX) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter target was not found");
                }
                const TypeId targetType = program.GetSlotType(slot);
                const DependencyProperty* property =
                    ResolveTemplateProperty(
                        properties, targetType,
                        sourceSetter.GetPropertyName());
                if (property == nullptr) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter property was not found");
                }
                Base::Result<Value> value =
                    ConvertSetterValue(
                        sourceSetter,
                        targetType,
                        *property,
                        runtime,
                        properties);
                if (!value) {
                    return value.GetStatus();
                }

                VisualStateSetterPlan setter;
                assigned =
                    setter.targetName.Assign(
                        sourceSetter.GetTargetName());
                if (!assigned) {
                    return assigned.GetStatus();
                }
                setter.property = property->Handle();
                setter.value =
                    std::move(value).Value();
                state.setters.PushBack(
                        std::move(setter));
            }
            group.states.PushBack(
                    std::move(state));
        }
        for (const Base::Ref<VisualTransition>&
                  transitionObject :
              sourceGroup.GetTransitions()) {
            if (!transitionObject ||
                transitionObject->RuntimeType() !=
                    VisualTransition::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualTransition");
            }
            const auto& sourceTransition =
                static_cast<
                const VisualTransition&>(
                    *transitionObject);
            if (sourceTransition.GetFrom().Empty() &&
                sourceTransition.GetTo().Empty() &&
                sourceTransition.GetGeneratedDuration().Empty() &&
                !sourceTransition.GetStoryboard()) {
                return InvalidTemplateCompiler(
                    "VisualTransition must specify a duration or Storyboard");
            }
            if (!sourceTransition.GetFrom().Empty()) {
                bool found = false;
                for (const VisualStatePlan& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.GetFrom();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition From state was not found");
                }
            }
            if (!sourceTransition.GetTo().Empty()) {
                bool found = false;
                for (const VisualStatePlan& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.GetTo();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition To state was not found");
                }
            }

            VisualTransitionPlan transition;
            assigned = transition.from.Assign(
                sourceTransition.GetFrom());
            if (assigned) {
                assigned = transition.to.Assign(
                    sourceTransition.GetTo());
            }
            if (!assigned) return assigned.GetStatus();
            if (!sourceTransition.GetGeneratedDuration().Empty()) {
                Storyboard duration;
                duration.SetDuration(sourceTransition.GetGeneratedDuration());
                transition.generatedDurationMicroseconds =
                    Timing(duration).durationMicroseconds;
            }
            transition.generatedEasingFunction =
                sourceTransition.GetGeneratedEasingFunction();
            if (auto srcStoryboard = sourceTransition.GetStoryboard()) {
                auto storyboardCopy =
                    Base::MakeRef<Storyboard>();
                if (!storyboardCopy) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                for (const auto& tl : srcStoryboard->GetTimelines()) {
                    if (!tl) continue;
                    storyboardCopy->AddTimeline(tl);
                }
                transition.storyboard = std::move(storyboardCopy);
            } else {
                transition.storyboard.Reset();
            }
            group.transitions.PushBack(
                std::move(transition));
        }
        groups.PushBack(
            std::move(group));
        return {};
    };
    for (const Base::Ref<Base::Object>& groupObject :
         FrameworkTemplateState::AuthoredVisualStateGroups(controlTemplate)) {
        Base::Result<void> compiled = compileGroup(groupObject);
        if (!compiled) return compiled.GetStatus();
    }
    Base::Ref<Base::Object> authoredRoot =
        FrameworkTemplateState::AuthoredVisualTree(controlTemplate);
    if (authoredRoot &&
        runtime.Types().IsDerivedFrom(
            authoredRoot->RuntimeType(),
            DependencyObject::StaticTypeId())) {
        auto& root = static_cast<DependencyObject&>(*authoredRoot);
        Base::Ref<VisualStateGroupCollection> valueStore = root.GetValue(VisualStateManager::VisualStateGroupsProperty);
        if (valueStore) {
            for (const Base::Ref<VisualStateGroup>& groupObject :
                 valueStore->GetItems()) {
                Base::Result<void> compiled = compileGroup(
                    Base::Ref<Base::Object>(groupObject));
                if (!compiled) return compiled.GetStatus();
            }
        }
    }
    return groups;
}

} // namespace

Base::Result<CompiledTriggerPlan> CompileTriggerPlan(
    Base::Ref<Aero::TriggerBase> trigger,
    const XamlBytecodeProgram& program,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept {
    if (!trigger) {
        return InvalidTemplateCompiler("Null trigger");
    }
    CompiledTriggerPlan plan;
    plan.trigger = trigger;
    Base::Span<const Base::Ref<Setter>> setters;
    if (trigger->RuntimeType() == Trigger::StaticTypeId() ||
        trigger->RuntimeType() == DataTrigger::StaticTypeId() ||
        trigger->RuntimeType() == MultiDataTrigger::StaticTypeId()) {
        setters = static_cast<const SetterTriggerBase&>(*trigger).GetAuthoredSetters();
    }
    for (const Base::Ref<Setter>& setter : setters) {
        if (!setter) continue;
        const std::uint32_t target = setter->GetTargetName().Empty()
            ? 0U : program.FindSlotByName(setter->GetTargetName());
        if (target >= program.slotNames.Size()) {
            return InvalidTemplateCompiler(
                "Template trigger Setter target was not found");
        }
        const TypeId targetType = program.GetSlotType(target);
        const DependencyProperty* property = ResolveTemplateProperty(
            properties, targetType, setter->GetPropertyName());
        if (property == nullptr) {
            return InvalidTemplateCompiler(
                "Template trigger Setter property was not found");
        }
        Base::Result<Value> value = ConvertSetterValue(
            *setter, targetType, *property, runtime, properties);
        if (!value) return value.GetStatus();
        CompiledTriggerSetter compiledSetter;
        compiledSetter.target = target;
        compiledSetter.property = property->Handle();
        compiledSetter.value = std::move(value).Value();
        plan.setters.PushBack(std::move(compiledSetter));
    }
    return plan;
}

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<CompiledTemplateBlueprint> blueprint =
        CompileBlueprint(
        FrameworkTemplateState::AuthoredVisualTree(controlTemplate),
        &FrameworkTemplateState::AuthoredNames(controlTemplate),
        edges,
        bindings,
        FrameworkTemplateState::MetadataBindings(
            controlTemplate),
        FrameworkTemplateState::DynamicResources(
            controlTemplate),
        runtime,
        properties);
    if (!blueprint) {
        return blueprint.GetStatus();
    }

    CompiledTemplateDefinition definition;
    definition.targetType =
        controlTemplate.GetTargetType();

    for (std::uint32_t nodeIndex = 0U;
         nodeIndex < blueprint.Value().nodes.Size(); ++nodeIndex) {
        TemplatePrototypeNode& node =
            blueprint.Value().nodes[nodeIndex];
        if (!runtime.Types().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId())) {
            continue;
        }
        const DependencyProperty* contentSourceProperty =
            properties.Find(
                ContentPresenter::
                    ContentSourceProperty.Handle());
        const DependencyProperty* contentProperty =
            properties.Find(
                ContentPresenter::
                    ContentProperty.Handle());
        const DependencyProperty* contentTemplateProperty =
            properties.Find(
                ContentPresenter::
                    ContentTemplateProperty.Handle());
        if (contentSourceProperty == nullptr ||
            contentProperty == nullptr) {
            return InvalidTemplateCompiler(
                "ContentPresenter content properties were not registered");
        }
        Base::StringView contentSource;
        bool hasContentProperty = false;
        bool hasContentTemplateProperty = false;
        for (const TemplatePrototypeProperty& property :
             node.properties) {
            if (property.property ==
                    contentSourceProperty->Handle() &&
                property.value.Kind() ==
                    ValueKind::String) {
                contentSource =
                    property.value.AsString();
            }
            if (property.property ==
                    contentProperty->Handle()) {
                hasContentProperty = true;
            }
            if (contentTemplateProperty != nullptr &&
                property.property == contentTemplateProperty->Handle()) {
                hasContentTemplateProperty = true;
            }
        }
        // Explicit Content= (including Content="{TemplateBinding Content}")
        // already wires the presenter. Do not add a second binding.
        if (hasContentProperty) {
            continue;
        }
        const bool defaultedContent = contentSource.Empty();
        if (defaultedContent) {
            // WPF ContentPresenter.ContentSource defaults to "Content".
            // Unnamed presenters in ListBoxItem/Button templates rely on that
            // implicit TemplateBinding to show DisplayMemberPath text.
            contentSource = Base::StringView("Content");
        }
        if (node.name.Empty()) {
            char generatedName[48];
            const int written = std::snprintf(
                generatedName, sizeof(generatedName),
                "__ContentSource%u", nodeIndex);
            if (written <= 0 || static_cast<std::size_t>(written) >=
                                    sizeof(generatedName)) {
                return InvalidTemplateCompiler(
                    "ContentPresenter generated template name is invalid");
            }
            Base::Result<void> named = node.name.Assign(
                Base::StringView(
                    generatedName,
                    static_cast<std::uint32_t>(written)));
            if (!named) return named.GetStatus();
        }
        const DependencyProperty* source =
            properties.Find(
                controlTemplate.GetTargetType(),
                contentSource);
        if (source == nullptr) {
            if (defaultedContent) {
                continue;
            }
            return MissingTemplateProperty(
                "ContentSource property",
                contentSource,
                controlTemplate.GetTargetType(),
                runtime.Types());
        }
        TemplateBindingPlan binding;
        Base::Result<void> assigned =
            binding.targetName.Assign(
                node.name.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        binding.sourceProperty =
            source->Handle();
        binding.targetProperty =
            contentProperty->Handle();
        definition.contentSourceBindings.
            PushBack(std::move(binding));

        if (!hasContentTemplateProperty && contentTemplateProperty != nullptr) {
            char templatePropName[64];
            std::snprintf(templatePropName, sizeof(templatePropName), "%.*sTemplate",
                static_cast<int>(contentSource.SizeBytes()), contentSource.Data());
            const DependencyProperty* sourceTemplate = properties.Find(
                controlTemplate.GetTargetType(), Base::StringView(templatePropName));
            if (sourceTemplate == nullptr && contentSource == Base::StringView("SelectedContent")) {
                sourceTemplate = properties.Find(controlTemplate.GetTargetType(), Base::StringView("ContentTemplate"));
            }
            if (sourceTemplate != nullptr) {
                TemplateBindingPlan tplBinding;
                if (tplBinding.targetName.Assign(node.name.View())) {
                    tplBinding.sourceProperty = sourceTemplate->Handle();
                    tplBinding.targetProperty = contentTemplateProperty->Handle();
                    definition.contentSourceBindings.PushBack(std::move(tplBinding));
                }
            }
        }
    }

    auto container = Base::MakeRef<XamlBytecodeContainer>();
    if (!container) {
        return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
    }
    Base::Result<void> bytecoded = CompileBlueprintToBytecode(
        blueprint.Value(), container->program);
    if (!bytecoded) return bytecoded.GetStatus();

    Base::Result<Base::Vector<VisualStateGroupPlan>>
        groups = CompileVisualStates(
            controlTemplate,
            container->program,
            runtime,
            properties);
    if (!groups) return groups.GetStatus();
    Base::Result<Base::Vector<TemplatePropertyTrigger>>
        triggers = CompilePropertyTriggers(
            controlTemplate,
            container->program,
            runtime,
            properties);
    if (!triggers) return triggers.GetStatus();

    for (const Base::Ref<Base::Object>& authored :
         FrameworkTemplateState::AuthoredTriggers(controlTemplate)) {
        if (!authored) continue;
        if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
            authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
            (authored->RuntimeType() == Trigger::StaticTypeId() &&
             (!static_cast<const Trigger&>(*authored).
                    GetEnterActions().Empty() ||
              !static_cast<const Trigger&>(*authored).
                    GetExitActions().Empty()))) {
            Base::Ref<TriggerBase> retained =
                Base::Ref<TriggerBase>::TryFromBorrowed(
                    static_cast<TriggerBase&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate instance trigger cannot be retained");
            }
            Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                std::move(retained), container->program, runtime, properties);
            if (!plan) return plan.GetStatus();
            container->program.controlTemplateDataTriggers.PushBack(
                std::move(plan).Value());
        } else if (runtime.Types().IsDerivedFrom(
                       authored->RuntimeType(),
                       EventTrigger::StaticTypeId())) {
            Base::Ref<EventTrigger> retained =
                Base::Ref<EventTrigger>::TryFromBorrowed(
                    static_cast<EventTrigger&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate EventTrigger cannot be retained");
            }
            container->program.controlTemplateEventTriggers.PushBack(
                    std::move(retained));
        }
    }

    definition.program = &container->program;
    definition.programOwner = std::move(container);
    definition.propertyTriggers =
        std::move(triggers).Value();
    definition.visualStateGroups =
        std::move(groups).Value();
    return definition;
}

// Compiles a derived ControlTemplate that inherits its visual tree from a
// sealed XAML-compiled base template (BasedOn without own VisualTree).
// Derived-authored property triggers and visual states resolve
// TargetName/SourceName against the BASE program, i.e. the shared tree.
// Action-bearing Trigger/DataTrigger/EventTrigger declarations cannot be
// represented in the property-trigger table; they must live in a template
// that authors its own VisualTree.
Base::Result<void>
CompileInheritedControlTemplate(
    ControlTemplate& derivedTemplate,
    const XamlBytecodeProgram& baseProgram,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    for (const Base::Ref<Base::Object>& object :
        FrameworkTemplateState::AuthoredTriggers(derivedTemplate)) {
        if (!object) {
            continue;
        }
        const Meta::TypeId kind = object->RuntimeType();
        if (kind == EventTrigger::StaticTypeId() ||
            kind == DataTrigger::StaticTypeId() ||
            kind == MultiDataTrigger::StaticTypeId()) {
            return InvalidTemplateCompiler(
                "Inherited ControlTemplate authoring an EventTrigger or DataTrigger requires its own VisualTree");
        }
        if (kind == Trigger::StaticTypeId()) {
            const auto& source = static_cast<const Trigger&>(*object);
            if (!source.GetEnterActions().Empty() ||
                !source.GetExitActions().Empty()) {
                return InvalidTemplateCompiler(
                    "Inherited ControlTemplate Trigger with Enter/ExitActions requires its own VisualTree");
            }
        }
    }
    Base::Result<Base::Vector<TemplatePropertyTrigger>> triggers =
        CompilePropertyTriggers(
            derivedTemplate, baseProgram, runtime, properties);
    if (!triggers) {
        return triggers.GetStatus();
    }
    for (TemplatePropertyTrigger& trigger : triggers.Value()) {
        Base::Result<void> added =
            FrameworkTemplateState::AddPropertyTrigger(
                derivedTemplate, std::move(trigger));
        if (!added) {
            return added.GetStatus();
        }
    }
    Base::Result<Base::Vector<VisualStateGroupPlan>> groups =
        CompileVisualStates(
            derivedTemplate, baseProgram, runtime, properties);
    if (!groups) {
        return groups.GetStatus();
    }
    for (VisualStateGroupPlan& group : groups.Value()) {
        Base::Result<void> added =
            FrameworkTemplateState::AddVisualStateGroup(
                derivedTemplate, std::move(group));
        if (!added) {
            return added.GetStatus();
        }
    }
    return {};
}

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return CompileBlueprint(
        visualTree,
        names,
        edges,
        bindings,
        {},
        {},
        runtime,
        properties);
}



static Base::Result<void> AttachCompiledTemplateBindings(
    const XamlBytecodeProgram& program,
    const Base::Vector<Base::Ref<Base::Object>>& objects,
    BindingEngine& bindings,
    FrameworkElement* templatedParent,
    const Base::Ref<Base::Object>& payload) noexcept {
    for (const TemplatePrototypeBinding& binding : program.blueprintBindings) {
        if (binding.metadata == nullptr ||
            binding.target >= objects.Size() ||
            (binding.source != UINT32_MAX &&
             binding.source >= objects.Size())) {
            return InvalidTemplateCompiler(
                "Compiled template Binding declaration is invalid");
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = binding.metadata;
        descriptor.source =
            binding.source != UINT32_MAX
            ? objects[binding.source].Get()
            : nullptr;
        if (descriptor.source == nullptr &&
            !binding.sourceName.Empty()) {
            if (templatedParent != nullptr) {
                descriptor.source = templatedParent->FindName(
                    binding.sourceName.View());
                if (descriptor.source == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "ControlTemplate Binding ElementName was not found in the outer NameScope");
                }
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "DataTemplate Binding cannot resolve an ElementName outside its template NameScope");
            }
        }
        if (descriptor.source == nullptr &&
            !binding.relativeAncestorType.Empty()) {
            descriptor.source = ResolveTemplateBindingAncestor(
                binding,
                *objects[binding.target],
                *program.runtime);
            if (descriptor.source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template Binding ancestor was not found");
            }
        }
        descriptor.target =
            static_cast<DependencyObject*>(
                objects[binding.target].Get());
        descriptor.targetProperty =
            binding.targetProperty;
        descriptor.dataContextProperty =
            binding.dataContextProperty;
        descriptor.dataContextOwner =
            descriptor.target != nullptr
            ? descriptor.target
            : (!objects.Empty() ? static_cast<DependencyObject*>(objects[0].Get()) : nullptr);
        descriptor.path = binding.path.View();
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.bindsToSource =
            binding.bindsToSource;
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        descriptor.converterResource = binding.converter;
        descriptor.converterParameter = binding.converterParameter;
        Base::Result<void> queued =
            bindings.QueueDeferred(descriptor);
        if (!queued) return queued.GetStatus();

        if (payload &&
            templatedParent == nullptr &&
            binding.source == UINT32_MAX &&
            binding.sourceName.Empty() &&
            binding.relativeAncestorType.Empty()) {
            Base::Result<void> activated =
                bindings.ActivateDeferredWhenReady(*descriptor.target);
            if (!activated) return activated.GetStatus();
        }
    }
    return {};
}

static Base::Result<void> AttachCompiledTemplateTriggers(
    const XamlBytecodeProgram& program,
    const Base::Vector<Base::Ref<Base::Object>>& objects,
    const Base::Vector<CompiledTriggerPlan>& triggers,
    const Base::Vector<Base::Ref<EventTrigger>>& eventTriggers,
    FrameworkElement* templatedParent,
    const Base::Ref<Base::Object>& payload,
    bool registerNamesInRoot) noexcept {
    if (triggers.Empty() && eventTriggers.Empty()) {
        return {};
    }
    if (objects.Empty() ||
        !program.runtime->Types().IsDerivedFrom(
            objects[0U]->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        return InvalidTemplateCompiler(
            "Template root does not support runtime triggers");
    }
    Base::Result<Base::Ref<DataTemplateTriggerInstance>> created =
        Base::MakeRef<DataTemplateTriggerInstance>();
    if (!created) return created.GetStatus();
    Base::Ref<DataTemplateTriggerInstance> triggerContext =
        std::move(created).Value();
    auto* root = static_cast<FrameworkElement*>(objects[0U].Get());
    triggerContext->root = root;

    for (std::uint32_t index = 0U; index < objects.Size(); ++index) {
        if (index >= program.slotNames.Size() || program.slotNames[index].Empty()) continue;
        DataTemplateTriggerInstance::NamedObject named;
        Base::Result<void> namedAssigned = named.name.Assign(
            program.slotNames[index].View());
        if (!namedAssigned) return namedAssigned.GetStatus();
        named.object = Base::WeakRef<Base::Object>(
            Base::Ref<Base::Object>::FromBorrowed(*objects[index]));
        triggerContext->names.PushBack(std::move(named));
        if (registerNamesInRoot) {
            static_cast<void>(root->RegisterName(
                program.slotNames[index].View(),
                *objects[index]));
        }
    }

    auto sourceFor = [&](const Binding& binding) noexcept -> Base::Object* {
        if (!binding.GetElementName().Empty()) {
            return triggerContext->FindName(binding.GetElementName());
        }
        if (templatedParent != nullptr) {
            const Base::Ref<RelativeSource> relative =
                binding.GetRelativeSource();
            if (!relative) {
                return templatedParent;
            }
            if (relative->GetMode() == RelativeSourceMode::FindAncestor) {
                return WalkTemplateAncestor(
                    templatedParent,
                    relative->GetAncestorType(),
                    relative->GetAncestorLevel(),
                    program.runtime->Types());
            }
            return templatedParent;
        }
        return payload.Get();
    };

    for (const CompiledTriggerPlan& plan : triggers) {
        const Base::Ref<TriggerBase>& authored = plan.trigger;
        if (!authored) continue;
        DataTemplatePropertyTrigger runtimeTrigger;
        if (authored->RuntimeType() == Trigger::StaticTypeId()) {
            const auto& property = static_cast<const Trigger&>(*authored);
            Base::Object* source = nullptr;
            if (templatedParent != nullptr) {
                source = property.GetSourceName().Empty()
                    ? static_cast<Base::Object*>(templatedParent)
                    : triggerContext->FindName(property.GetSourceName());
            } else {
                source = root;
            }
            if (source == nullptr ||
                !program.runtime->Types().IsDerivedFrom(
                    source->RuntimeType(),
                    DependencyObject::StaticTypeId())) {
                return InvalidTemplateCompiler(
                    "Template action trigger source was not found");
            }
            const TypeId sourceType = source->RuntimeType();
            const DependencyProperty* sourceProperty =
                ResolveTemplateProperty(
                    *program.properties,
                    sourceType,
                    property.GetPropertyName());
            if (sourceProperty == nullptr) {
                return MissingTemplateProperty(
                    "Template action trigger property",
                    property.GetPropertyName(), sourceType,
                    program.runtime->Types());
            }
            DataTemplateTriggerCondition condition;
            condition.source = Base::WeakRef<Base::Object>(
                Base::Ref<Base::Object>::FromBorrowed(*source));
            condition.dependencySource =
                Base::WeakRef<DependencyObject>(
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(source)));
            condition.property = sourceProperty->Handle();
            Base::Result<Value> converted = ConvertTriggerValue(
                property.GetAuthoredValue(), sourceType, *sourceProperty,
                *program.runtime, *program.properties);
            if (!converted) return converted.GetStatus();
            condition.value = std::move(converted).Value();
            runtimeTrigger.conditions.PushBack(std::move(condition));
        } else if (authored->RuntimeType() == DataTrigger::StaticTypeId()) {
            const auto& data = static_cast<const DataTrigger&>(*authored);
            if (!data.GetBinding()) return InvalidTemplateCompiler(
                "Template DataTrigger requires Binding");
            DataTemplateTriggerCondition condition;
            Base::Object* source = sourceFor(*data.GetBinding());
            if (source != nullptr) {
                condition.source =
                    Base::WeakRef<Base::Object>(
                        Base::Ref<Base::Object>::FromBorrowed(*source));
            }
            condition.usesDataContext =
                data.GetBinding()->GetElementName().Empty() &&
                !data.GetBinding()->GetRelativeSource();
            condition.binding = data.GetBinding();
            condition.value = data.GetAuthoredValue();
            runtimeTrigger.conditions.PushBack(std::move(condition));
        } else if (authored->RuntimeType() == MultiDataTrigger::StaticTypeId()) {
            const auto& multi = static_cast<const MultiDataTrigger&>(*authored);
            for (const Base::Ref<Condition>& authoredCondition : multi.GetConditions()) {
                if (!authoredCondition || !authoredCondition->GetBinding()) {
                    return InvalidTemplateCompiler(
                        "Template MultiDataTrigger requires complete Conditions");
                }
                DataTemplateTriggerCondition condition;
                Base::Object* source = sourceFor(*authoredCondition->GetBinding());
                if (source != nullptr) {
                    condition.source =
                        Base::WeakRef<Base::Object>(
                            Base::Ref<Base::Object>::FromBorrowed(*source));
                }
                condition.usesDataContext =
                    authoredCondition->GetBinding()->GetElementName().Empty() &&
                    !authoredCondition->GetBinding()->GetRelativeSource();
                condition.binding = authoredCondition->GetBinding();
                condition.value = authoredCondition->GetAuthoredValue();
                runtimeTrigger.conditions.PushBack(std::move(condition));
            }
        } else {
            continue;
        }
        if (templatedParent == nullptr || authored->RuntimeType() != Trigger::StaticTypeId()) {
            for (const CompiledTriggerSetter& cs : plan.setters) {
                DataTemplateTriggerSetter runtimeSetter;
                runtimeSetter.target =
                    Base::WeakRef<DependencyObject>(
                        Base::Ref<DependencyObject>::FromBorrowed(
                            *static_cast<DependencyObject*>(objects[cs.target].Get())));
                runtimeSetter.property = cs.property;
                runtimeSetter.value = cs.value;
                runtimeTrigger.setters.PushBack(std::move(runtimeSetter));
            }
        }
        runtimeTrigger.enterActions.Append(authored->GetEnterActions());
        runtimeTrigger.exitActions.Append(authored->GetExitActions());
        triggerContext->triggers.PushBack(std::move(runtimeTrigger));
    }
    for (const Base::Ref<EventTrigger>& eventTrigger : eventTriggers) {
        if (!eventTrigger) continue;
        triggerContext->eventTriggers.PushBack(eventTrigger);
    }
    FrameworkElement* const triggerRoot = triggerContext->root;
    Base::Ref<Base::Object> triggerOwner(triggerContext);
    FrameworkElementSeams::AddAuthoredTrigger(*triggerRoot, std::move(triggerOwner));
    return {};
}

Base::Result<void> BuildCompiledTemplate(
    TemplateBuilder& context,
    void* factoryContext) noexcept {
    const auto* program =
        static_cast<const XamlBytecodeProgram*>(
            factoryContext);
    if (program == nullptr || !program->IsValid()) {
        return InvalidTemplateCompiler(
            "Compiled template bytecode is invalid");
    }

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> executed = XamlVM::Execute(
        *program, *program->runtime, &context, objects, nullptr);
    if (!executed) return executed.GetStatus();

    Base::Result<void> bound = AttachCompiledTemplateBindings(
        *program, objects, context.Bindings(), &context.TemplatedParent(), {});
    if (!bound) return bound.GetStatus();

    return AttachCompiledTemplateTriggers(
        *program, objects,
        program->controlTemplateDataTriggers,
        program->controlTemplateEventTriggers,
        &context.TemplatedParent(), {}, false);
}

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext,
    BindingEngine* bindings) noexcept {
    const auto* program =
        static_cast<const XamlBytecodeProgram*>(
            factoryContext);
    if (program == nullptr || !program->IsValid()) {
        return InvalidTemplateCompiler(
            "Deferred template bytecode is invalid");
    }

    if (bindings == nullptr && !program->blueprintBindings.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate Binding requires a mounted View binding engine");
    }

    XamlVMContext vmContext;
    vmContext.runtime = program->runtime;
    vmContext.dataItemPayload = payload;
    vmContext.bindings = bindings;

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> executed = XamlVM::Execute(
        *program, vmContext, objects, nullptr);
    if (!executed) return executed.GetStatus();

    Base::Ref<Base::Object> root = objects[0U];

    if (bindings != nullptr) {
        Base::Result<void> bound = AttachCompiledTemplateBindings(
            *program, objects, *bindings, nullptr, payload);
        if (!bound) return bound.GetStatus();
    }

    static const Base::Vector<Base::Ref<EventTrigger>> noEventTriggers;
    Base::Result<void> triggered = AttachCompiledTemplateTriggers(
        *program, objects,
        program->dataTemplateTriggers,
        noEventTriggers, nullptr, payload, true);
    if (!triggered) return triggered.GetStatus();

    return root;
}

} // namespace Aero::Markup
