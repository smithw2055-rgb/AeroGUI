#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>

#include <cstdint>

namespace Aero::Markup {

enum class XmlTokenKind : std::uint8_t {
    None = 0U,
    StartElement,
    EndElement,
    Text,
    EndOfDocument
};

struct XmlTokenizerLimits final {
    std::uint64_t maxInputBytes = 16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxDepth = 256U;
    std::uint32_t maxAttributesPerElement = 256U;
    std::uint32_t maxNameBytes = 1024U;
    std::uint32_t maxTextBytes = 1024U * 1024U;
};

namespace XmlDiagnosticCodes {
inline constexpr Core::DiagnosticCode InvalidUtf8 =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 1U);
inline constexpr Core::DiagnosticCode InputLimitExceeded =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 2U);
inline constexpr Core::DiagnosticCode UnexpectedEndOfInput =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 3U);
inline constexpr Core::DiagnosticCode MalformedMarkup =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 4U);
inline constexpr Core::DiagnosticCode UnsupportedDeclaration =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 5U);
inline constexpr Core::DiagnosticCode DepthLimitExceeded =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 6U);
inline constexpr Core::DiagnosticCode AttributeLimitExceeded =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 7U);
inline constexpr Core::DiagnosticCode NameLimitExceeded =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 8U);
inline constexpr Core::DiagnosticCode TextLimitExceeded =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 9U);
inline constexpr Core::DiagnosticCode MismatchedEndElement =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 10U);
inline constexpr Core::DiagnosticCode MultipleDocumentElements =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 11U);
inline constexpr Core::DiagnosticCode UnknownEntity =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 12U);
inline constexpr Core::DiagnosticCode InvalidXmlCharacter =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 13U);
inline constexpr Core::DiagnosticCode DuplicateAttribute =
    Core::MakeDiagnosticCode(Core::DiagnosticDomain::Xaml, 14U);
} // namespace XmlDiagnosticCodes

class AERO_API XmlAttribute final {
public:
    explicit XmlAttribute(Base::IAllocator* allocator = nullptr) noexcept
        : name_(allocator), value_(allocator) {}

    XmlAttribute(XmlAttribute&&) noexcept = default;
    XmlAttribute& operator=(XmlAttribute&&) noexcept = default;

    XmlAttribute(const XmlAttribute&) = delete;
    XmlAttribute& operator=(const XmlAttribute&) = delete;

    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::StringView Value() const noexcept {
        return value_.View();
    }
    Core::SourceSpan Source() const noexcept {
        return source_;
    }
    Core::SourceSpan NameSource() const noexcept {
        return nameSource_;
    }
    Core::SourceSpan ValueSource() const noexcept {
        return valueSource_;
    }

private:
    friend class Utf8XmlTokenizer;

    Base::String name_;
    Base::String value_;
    Core::SourceSpan source_;
    Core::SourceSpan nameSource_;
    Core::SourceSpan valueSource_;
};

class AERO_API XmlToken final {
public:
    explicit XmlToken(Base::IAllocator* allocator = nullptr) noexcept
        : name_(allocator), text_(allocator), attributes_(allocator) {}

    XmlToken(XmlToken&&) noexcept = default;
    XmlToken& operator=(XmlToken&&) noexcept = default;

    XmlToken(const XmlToken&) = delete;
    XmlToken& operator=(const XmlToken&) = delete;

    void Clear() noexcept;

    XmlTokenKind Kind() const noexcept { return kind_; }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::StringView Text() const noexcept {
        return text_.View();
    }
    Base::Span<const XmlAttribute> Attributes() const noexcept {
        return {attributes_.Data(), attributes_.Size()};
    }
    Core::SourceSpan Source() const noexcept {
        return source_;
    }
    Core::SourceSpan NameSource() const noexcept {
        return nameSource_;
    }
    bool IsEmptyElement() const noexcept {
        return emptyElement_;
    }

private:
    friend class Utf8XmlTokenizer;

    XmlTokenKind kind_ = XmlTokenKind::None;
    Base::String name_;
    Base::String text_;
    Base::Vector<XmlAttribute> attributes_;
    Core::SourceSpan source_;
    Core::SourceSpan nameSource_;
    bool emptyElement_ = false;
};

class AERO_API IXmlTokenizer {
public:
    virtual ~IXmlTokenizer() = default;

    virtual Base::Result<void> Reset(
        Base::StringView utf8,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept = 0;
    virtual Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept = 0;
    virtual std::uint32_t Depth() const noexcept = 0;
};

class AERO_API Utf8XmlTokenizer final : public IXmlTokenizer {
public:
    explicit Utf8XmlTokenizer(
        XmlTokenizerLimits limits = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<void> Reset(
        Base::StringView utf8,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept override;
    std::uint32_t Depth() const noexcept override {
        return openElements_.Size();
    }

    const XmlTokenizerLimits& Limits() const noexcept {
        return limits_;
    }
    std::uint64_t ByteOffset() const noexcept {
        return offset_;
    }

private:
    Base::IAllocator* allocator_ = nullptr;
    XmlTokenizerLimits limits_;
    Base::StringView input_;
    Base::Vector<Base::String> openElements_;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    std::uint32_t offset_ = 0U;
    std::uint32_t line_ = 1U;
    std::uint32_t column_ = 1U;
    bool rootSeen_ = false;
    bool rootClosed_ = false;
    bool endEmitted_ = false;
    bool initialized_ = false;
    bool failed_ = false;

    bool AtEnd() const noexcept;
    bool StartsWith(
        const char* literal,
        std::uint32_t length) const noexcept;
    Core::SourcePosition Position() const noexcept;
    Core::SourceSpan SpanFrom(
        Core::SourcePosition begin) const noexcept;
    std::uint32_t CurrentCodePointLength() const noexcept;
    std::uint32_t CurrentCodePoint() const noexcept;
    void AdvanceCodePoint() noexcept;
    void ConsumeAscii(std::uint32_t count) noexcept;
    bool SkipWhitespace() noexcept;

    Base::Result<XmlTokenKind> ParseStartElement(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseEndElement(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseText(
        XmlToken& token) noexcept;
    Base::Result<XmlTokenKind> ParseCdata(
        XmlToken& token) noexcept;
    Base::Result<void> ParseName(
        Base::String& name,
        Core::SourceSpan& source) noexcept;
    Base::Result<void> ParseAttributeValue(
        char quote,
        Base::String& value,
        Core::SourceSpan& source) noexcept;
    Base::Result<void> AppendEntity(
        Base::String& output) noexcept;
    Base::Result<void> AppendCodePoint(
        Base::String& output,
        std::uint32_t codePoint) noexcept;
    Base::Result<void> AppendCurrentCodePoint(
        Base::String& output,
        bool attributeValue = false) noexcept;
    Base::Result<void> SkipComment() noexcept;
    Base::Result<void> SkipProcessingInstruction() noexcept;

    Base::Status Failure(
        Base::ErrorCode error,
        Core::DiagnosticCode diagnostic,
        Base::StringView message,
        Core::SourceSpan source) noexcept;
};

} // namespace Aero::Markup
