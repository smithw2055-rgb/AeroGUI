#pragma once

#include <Aero/Markup/Parsing/XmlTokenizer.hpp>

namespace Aero::Markup {

// Optional Expat-backed implementation of the public tokenizer contract.
// It rejects DTDs, parameter/external entities, and never performs I/O.
class AERO_API ExpatXmlTokenizer final
    : public IXmlTokenizer {
public:
    explicit ExpatXmlTokenizer(
        XmlTokenizerLimits limits = {}) noexcept;
    ~ExpatXmlTokenizer() noexcept override;

    ExpatXmlTokenizer(const ExpatXmlTokenizer&) = delete;
    ExpatXmlTokenizer& operator=(
        const ExpatXmlTokenizer&) = delete;

    Base::Result<void> Reset(
        Base::StringView utf8,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept override;
    Base::Result<XmlTokenKind> Read(
        XmlToken& token) noexcept override;
    std::uint32_t Depth() const noexcept override {
        return depth_;
    }

    const XmlTokenizerLimits& Limits() const noexcept {
        return limits_;
    }

private:
    XmlTokenizerLimits limits_;
    Base::Vector<XmlToken> tokens_;
    Base::Vector<std::uint32_t> tokenDepths_;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    void* parser_ = nullptr;
    Base::Status failure_;
    std::uint32_t readIndex_ = 0U;
    std::uint32_t parseDepth_ = 0U;
    std::uint32_t depth_ = 0U;
    bool initialized_ = false;

    void HandleStart(
        const char* name,
        const char** attributes) noexcept;
    void HandleEnd(const char* name) noexcept;
    void HandleText(
        const char* text,
        int length) noexcept;
    void RejectDeclaration() noexcept;
    int RejectExternalEntity() noexcept;
    Core::SourcePosition Position() const noexcept;
    void Stop(
        Base::Status status,
        Core::DiagnosticCode diagnostic,
        Base::StringView message) noexcept;
    Base::Result<void> PushToken(
        XmlToken&& token,
        std::uint32_t depth) noexcept;
};

} // namespace Aero::Markup
