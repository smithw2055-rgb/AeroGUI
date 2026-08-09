// Consolidated implementation. Keep sections ordered by dependency.

// ===== XmlTokenizer =====

#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include "gui/markup/MarkupRuntime.hpp"
#include "gui/markup/MarkupWriterRuntime.hpp"

// Canonical XML tokenizer implementation.

#include <Aero/Base/Utf8.hpp>

#include <cstring>
#include <utility>

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

        Base::Result<void> appendAttribute = token.attributes_.PushBack(
            std::move(attribute));
        if (!appendAttribute) {
            return appendAttribute.GetStatus();
        }
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
    Base::Result<void> pushResult = openElements_.PushBack(std::move(openName));
    if (!pushResult) {
        return pushResult.GetStatus();
    }
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
    std::uint32_t length = 0U;
    if (codePoint <= 0x7FU) {
        bytes[0] = static_cast<char>(codePoint);
        length = 1U;
    } else if (codePoint <= 0x7FFU) {
        bytes[0] = static_cast<char>(0xC0U | (codePoint >> 6U));
        bytes[1] = static_cast<char>(0x80U | (codePoint & 0x3FU));
        length = 2U;
    } else if (codePoint <= 0xFFFFU) {
        bytes[0] = static_cast<char>(0xE0U | (codePoint >> 12U));
        bytes[1] = static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
        bytes[2] = static_cast<char>(0x80U | (codePoint & 0x3FU));
        length = 3U;
    } else {
        bytes[0] = static_cast<char>(0xF0U | (codePoint >> 18U));
        bytes[1] = static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU));
        bytes[2] = static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU));
        bytes[3] = static_cast<char>(0x80U | (codePoint & 0x3FU));
        length = 4U;
    }
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

    Base::Result<void> scopeResult = scopes_.PushBack(std::move(frame));
    if (!scopeResult) {
        return scopeResult.GetStatus();
    }
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
    return pending_.PushBack(std::move(node));
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
    return bindings_.PushBack(std::move(binding));
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
        Base::Result<void> appended = ignorableNamespaces_.PushBack(
            std::move(copied));
        if (!appended) return appended.GetStatus();
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

#include <expat.h>

#include <climits>

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
    Base::Result<void> stored =
        tokens_.PushBack(std::move(token));
    if (!stored) return stored.GetStatus();
    Base::Result<void> depthStored =
        tokenDepths_.PushBack(depth);
    if (!depthStored) {
        tokens_.PopBack();
        return depthStored.GetStatus();
    }
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
    assigned = token.attributes_.Reserve(
        attributeCount);
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            ExpatMessageMalformed);
        return;
    }
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
            assigned =
                token.attributes_.PushBack(
                    std::move(attribute));
        }
        if (!assigned) {
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
