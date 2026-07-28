#include <Aero/Markup/CompiledDocument.hpp>

// Optional Expat tokenizer backend.

#include <expat.h>

#include <climits>
#include <cstring>
#include <utility>

namespace Aero::Markup {
namespace {

constexpr Base::StringView MessageMalformed(
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

Core::SourcePosition
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
    Core::DiagnosticCode diagnosticCode,
    Base::StringView message) noexcept {
    if (!failure_.IsOk()) return;
    failure_ = status;
    if (diagnostics_ != nullptr) {
        Base::Result<Core::Diagnostic> diagnostic =
            Core::Diagnostic::TryCreate(
                diagnosticCode,
                Core::DiagnosticSeverity::Error,
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
        tokens_.TryPushBack(std::move(token));
    if (!stored) return stored.GetStatus();
    Base::Result<void> depthStored =
        tokenDepths_.TryPushBack(depth);
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
    const Core::SourcePosition begin = Position();
    token.source_ = {begin, begin};
    token.nameSource_ = {begin, begin};
    Base::Result<void> assigned =
        token.name_.TryAssign(
            Base::StringView(name, nameBytes));
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed);
        return;
    }
    assigned = token.attributes_.TryReserve(
        attributeCount);
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed);
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
        assigned = attribute.name_.TryAssign(
            Base::StringView(
                attributeName,
                attributeNameBytes));
        if (assigned) {
            assigned = attribute.value_.TryAssign(
                Base::StringView(
                    attributeValue,
                    valueBytes));
        }
        if (assigned) {
            attribute.source_ = {begin, begin};
            attribute.nameSource_ = {begin, begin};
            attribute.valueSource_ = {begin, begin};
            assigned =
                token.attributes_.TryPushBack(
                    std::move(attribute));
        }
        if (!assigned) {
            Stop(
                assigned.GetStatus(),
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed);
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
            MessageMalformed);
    }
}

void ExpatXmlTokenizer::HandleEnd(
    const char* name) noexcept {
    if (!failure_.IsOk()) return;
    if (parseDepth_ == 0U) {
        Stop(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageMalformed.Data()),
            XmlDiagnosticCodes::MismatchedEndElement,
            MessageMalformed);
        return;
    }
    XmlToken token;
    token.kind_ = XmlTokenKind::EndElement;
    const Core::SourcePosition begin = Position();
    token.source_ = {begin, begin};
    token.nameSource_ = {begin, begin};
    Base::Result<void> assigned =
        token.name_.TryAssign(
            Base::StringView(
                name,
                static_cast<std::uint32_t>(
                    std::strlen(name))));
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed);
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
            MessageMalformed);
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
            tokens_.Back().text_.TryAppend(
                Base::StringView(text, size));
        if (!appended) {
            Stop(
                appended.GetStatus(),
                XmlDiagnosticCodes::MalformedMarkup,
                MessageMalformed);
        }
        return;
    }
    XmlToken token;
    token.kind_ = XmlTokenKind::Text;
    const Core::SourcePosition begin = Position();
    token.source_ = {begin, begin};
    Base::Result<void> assigned =
        token.text_.TryAssign(
            Base::StringView(text, size));
    if (!assigned) {
        Stop(
            assigned.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed);
        return;
    }
    Base::Result<void> pushed =
        PushToken(
            std::move(token), parseDepth_);
    if (!pushed) {
        Stop(
            pushed.GetStatus(),
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed);
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

Base::Result<void> ExpatXmlTokenizer::Reset(
    Base::StringView utf8,
    Core::IDiagnosticSink* diagnostics) noexcept {
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

    const enum XML_Status parsed = XML_Parse(
        parser,
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
                MessageMalformed.Data()),
            XmlDiagnosticCodes::MalformedMarkup,
            MessageMalformed);
        return failure_;
    }
    XmlToken end;
    end.kind_ = XmlTokenKind::EndOfDocument;
    const Core::SourcePosition position = Position();
    end.source_ = {position, position};
    Base::Result<void> stored =
        PushToken(std::move(end), 0U);
    if (!stored) return stored.GetStatus();
    initialized_ = true;
    return {};
}

Base::Result<XmlTokenKind> ExpatXmlTokenizer::Read(
    XmlToken& token) noexcept {
    if (!initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Expat tokenizer is not initialized");
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
