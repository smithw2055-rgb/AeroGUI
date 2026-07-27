#include <Aero/Markup/CompiledDocument.hpp>

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
    Core::IDiagnosticSink* diagnostics) noexcept {
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
                const Core::SourcePosition begin = Position();
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
            const Core::SourcePosition begin = Position();
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

Core::SourcePosition Utf8XmlTokenizer::Position() const noexcept {
    return {line_, column_, offset_};
}

Core::SourceSpan Utf8XmlTokenizer::SpanFrom(
    Core::SourcePosition begin) const noexcept {
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
    const Core::SourcePosition begin = Position();
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
        const Core::SourcePosition attributeBegin = Position();
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

        Base::Result<void> appendAttribute = token.attributes_.TryPushBack(
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
    Base::Result<void> copyResult = openName.TryAssignUnchecked(token.Name());
    if (!copyResult) {
        return copyResult.GetStatus();
    }
    Base::Result<void> pushResult = openElements_.TryPushBack(std::move(openName));
    if (!pushResult) {
        return pushResult.GetStatus();
    }
    return token.kind_;
}

Base::Result<XmlTokenKind> Utf8XmlTokenizer::ParseEndElement(
    XmlToken& token) noexcept {
    const Core::SourcePosition begin = Position();
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
    const Core::SourcePosition begin = Position();

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
    const Core::SourcePosition begin = Position();
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
    Core::SourceSpan& source) noexcept {
    const Core::SourcePosition begin = Position();
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

    Base::Result<void> assignResult = name.TryAssignUnchecked(
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
    Core::SourceSpan& source) noexcept {
    const Core::SourcePosition begin = Position();

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
    const Core::SourcePosition begin = Position();
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
        return output.TryAppendUnchecked(Base::StringView("<"));
    }
    if (Equals(entity, "gt", 2U)) {
        return output.TryAppendUnchecked(Base::StringView(">"));
    }
    if (Equals(entity, "amp", 3U)) {
        return output.TryAppendUnchecked(Base::StringView("&"));
    }
    if (Equals(entity, "quot", 4U)) {
        return output.TryAppendUnchecked(Base::StringView("\""));
    }
    if (Equals(entity, "apos", 4U)) {
        return output.TryAppendUnchecked(Base::StringView("'"));
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
    return output.TryAppendUnchecked(Base::StringView(bytes, length));
}

Base::Result<void> Utf8XmlTokenizer::AppendCurrentCodePoint(
    Base::String& output,
    bool attributeValue) noexcept {
    const Core::SourcePosition begin = Position();
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
        Base::Result<void> appendResult = output.TryAppendUnchecked(
            attributeValue ? Base::StringView(" ") : Base::StringView("\n"));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        AdvanceCodePoint();
        return {};
    }
    if (attributeValue && (codePoint == 0x9U || codePoint == 0xAU)) {
        Base::Result<void> appendResult = output.TryAppendUnchecked(
            Base::StringView(" "));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        AdvanceCodePoint();
        return {};
    }

    Base::Result<void> appendResult = output.TryAppendUnchecked(
        Base::StringView(input_.Data() + offset_, length));
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    AdvanceCodePoint();
    return {};
}

Base::Result<void> Utf8XmlTokenizer::SkipComment() noexcept {
    const Core::SourcePosition begin = Position();
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
    const Core::SourcePosition begin = Position();
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
    Core::DiagnosticCode diagnostic,
    Base::StringView message,
    Core::SourceSpan source) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<Core::Diagnostic> item = Core::Diagnostic::TryCreate(
            diagnostic,
            Core::DiagnosticSeverity::Error,
            message,
            source,
            Core::InvalidDiagnosticObjectId,
            Core::InvalidMemberId);
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
