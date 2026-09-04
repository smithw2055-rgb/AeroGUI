#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics/SourceSpan.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero::Diagnostics {

using ::Aero::Meta::MemberId;
using ::Aero::Meta::InvalidMemberId;

enum class DiagnosticSeverity : std::uint8_t {
    Info = 0U,
    Warning,
    Error,
    Fatal
};

enum class DiagnosticDomain : std::uint8_t {
    Invalid = 0U,
    Base,
    Xaml,
    DependencyProperty,
    Binding,
    Layout,
    Input,
    Render,
    Graphics,
    GlContext,
    WebGl,
    Platform,
    Dependency,
    Count
};

struct DiagnosticCode  {
    std::uint32_t value = 0U;

    constexpr bool IsValid() const noexcept {
        const DiagnosticDomain domain = Domain();
        const std::uint16_t number = Number();
        return domain > DiagnosticDomain::Invalid &&
            domain < DiagnosticDomain::Count &&
            number > 0U && number <= 9999U;
    }

    constexpr DiagnosticDomain Domain() const noexcept {
        return static_cast<DiagnosticDomain>((value >> 16U) & 0xFFU);
    }

    constexpr std::uint16_t Number() const noexcept {
        return static_cast<std::uint16_t>(value & 0xFFFFU);
    }
};

constexpr DiagnosticCode MakeDiagnosticCode(
    DiagnosticDomain domain,
    std::uint16_t number) noexcept {
    return domain > DiagnosticDomain::Invalid &&
        domain < DiagnosticDomain::Count &&
        number > 0U && number <= 9999U
        ? DiagnosticCode{
            (static_cast<std::uint32_t>(domain) << 16U) |
            static_cast<std::uint32_t>(number)}
        : DiagnosticCode{};
}

constexpr bool operator==(
    DiagnosticCode left,
    DiagnosticCode right) noexcept {
    return left.value == right.value;
}

constexpr bool operator!=(
    DiagnosticCode left,
    DiagnosticCode right) noexcept {
    return !(left == right);
}

using DiagnosticObjectId = std::uint64_t;
inline constexpr DiagnosticObjectId InvalidDiagnosticObjectId = 0U;

AERO_GUI_API bool IsValidSourcePosition(
    SourcePosition position) noexcept;
AERO_GUI_API bool IsValidSourceSpan(SourceSpan span) noexcept;
AERO_GUI_API StringView DiagnosticPrefix(
    DiagnosticDomain domain) noexcept;
AERO_GUI_API Result<void> FormatDiagnosticCode(
    DiagnosticCode code,
    String& output) noexcept;

class AERO_GUI_API DiagnosticNote  {
public:
    DiagnosticNote(DiagnosticNote&&) noexcept = default;
    DiagnosticNote& operator=(DiagnosticNote&&) noexcept = default;

    DiagnosticNote(const DiagnosticNote&) = delete;
    DiagnosticNote& operator=(const DiagnosticNote&) = delete;

    SourceSpan Source() const noexcept { return source_; }
    StringView Message() const noexcept {
        return message_.View();
    }

private:
    friend class Diagnostic;

    DiagnosticNote() noexcept
        : message_(&Base::GetDefaultAllocator()) {}

    SourceSpan source_;
    String message_;
};

class AERO_GUI_API Diagnostic  {
public:
    Diagnostic(Diagnostic&&) noexcept = default;
    Diagnostic& operator=(Diagnostic&&) noexcept = default;

    Diagnostic(const Diagnostic&) = delete;
    Diagnostic& operator=(const Diagnostic&) = delete;

    static Result<Diagnostic> Create(
        DiagnosticCode code,
        DiagnosticSeverity severity,
        StringView message,
        SourceSpan source = {},
        DiagnosticObjectId object = InvalidDiagnosticObjectId,
        MemberId member = InvalidMemberId) noexcept;

    Result<void> AddNote(
        StringView message,
        SourceSpan source = {}) noexcept;

    DiagnosticCode Code() const noexcept { return code_; }
    DiagnosticSeverity Severity() const noexcept {
        return severity_;
    }
    StringView Message() const noexcept {
        return message_.View();
    }
    SourceSpan Source() const noexcept { return source_; }
    DiagnosticObjectId Object() const noexcept { return object_; }
    MemberId Member() const noexcept { return member_; }
    Span<const DiagnosticNote> Notes() const noexcept {
        return {notes_.Data(), notes_.Size()};
    }
    bool IsError() const noexcept {
        return severity_ == DiagnosticSeverity::Error ||
            severity_ == DiagnosticSeverity::Fatal;
    }

private:
    Diagnostic() noexcept;

    DiagnosticCode code_;
    DiagnosticSeverity severity_ = DiagnosticSeverity::Info;
    SourceSpan source_;
    DiagnosticObjectId object_ = InvalidDiagnosticObjectId;
    MemberId member_ = InvalidMemberId;
    String message_;
    Base::Vector<DiagnosticNote> notes_;
};

class AERO_GUI_API IDiagnosticSink {
public:
    virtual ~IDiagnosticSink() = default;

    virtual Result<void> Report(
        Diagnostic&& diagnostic) noexcept = 0;
};

class AERO_GUI_API DiagnosticBag  : public IDiagnosticSink {
public:
    explicit DiagnosticBag(
        std::uint32_t maxDiagnostics = 1024U) noexcept;

    DiagnosticBag(const DiagnosticBag&) = delete;
    DiagnosticBag& operator=(const DiagnosticBag&) = delete;

    Result<void> Report(
        Diagnostic&& diagnostic) noexcept override;

    Result<void> Report(
        DiagnosticCode code,
        DiagnosticSeverity severity,
        StringView message,
        SourceSpan source = {},
        DiagnosticObjectId object = InvalidDiagnosticObjectId,
        MemberId member = InvalidMemberId) noexcept;

    void Clear() noexcept;

    Span<const Diagnostic> Items() const noexcept {
        return {items_.Data(), items_.Size()};
    }
    std::uint32_t Size() const noexcept { return items_.Size(); }
    std::uint32_t MaxDiagnostics() const noexcept {
        return maxDiagnostics_;
    }
    std::uint32_t WarningCount() const noexcept {
        return warningCount_;
    }
    std::uint32_t ErrorCount() const noexcept {
        return errorCount_;
    }
    std::uint32_t DroppedCount() const noexcept {
        return droppedCount_;
    }
    bool HasErrors() const noexcept { return errorCount_ != 0U; }

private:
    Base::Vector<Diagnostic> items_;
    std::uint32_t maxDiagnostics_ = 0U;
    std::uint32_t warningCount_ = 0U;
    std::uint32_t errorCount_ = 0U;
    std::uint32_t droppedCount_ = 0U;
};

} // namespace Aero::Diagnostics
