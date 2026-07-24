#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {

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
    Rhi,
    GlContext,
    WebGl,
    Platform,
    Dependency,
    Count
};

struct DiagnosticCode final {
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

struct SourcePosition final {
    // Line and column are one-based. A zero pair represents an unknown position.
    std::uint32_t line = 0U;
    std::uint32_t column = 0U;
    std::uint64_t byteOffset = 0U;

    constexpr bool IsKnown() const noexcept {
        return line != 0U || column != 0U;
    }
};

struct SourceSpan final {
    // End is exclusive when the source provider can identify it precisely.
    SourcePosition begin;
    SourcePosition end;
};

using DiagnosticObjectId = std::uint64_t;
inline constexpr DiagnosticObjectId InvalidDiagnosticObjectId = 0U;

AERO_API bool IsValidSourcePosition(
    SourcePosition position) noexcept;
AERO_API bool IsValidSourceSpan(SourceSpan span) noexcept;
AERO_API Base::StringView DiagnosticPrefix(
    DiagnosticDomain domain) noexcept;
AERO_API Base::Result<void> TryFormatDiagnosticCode(
    DiagnosticCode code,
    Base::String& output) noexcept;

class AERO_API DiagnosticNote final {
public:
    DiagnosticNote(DiagnosticNote&&) noexcept = default;
    DiagnosticNote& operator=(DiagnosticNote&&) noexcept = default;

    DiagnosticNote(const DiagnosticNote&) = delete;
    DiagnosticNote& operator=(const DiagnosticNote&) = delete;

    SourceSpan Source() const noexcept { return source_; }
    Base::StringView Message() const noexcept {
        return message_.View();
    }

private:
    friend class Diagnostic;

    DiagnosticNote() noexcept
        : message_(&Base::GetDefaultAllocator()) {}

    SourceSpan source_;
    Base::String message_;
};

class AERO_API Diagnostic final {
public:
    Diagnostic(Diagnostic&&) noexcept = default;
    Diagnostic& operator=(Diagnostic&&) noexcept = default;

    Diagnostic(const Diagnostic&) = delete;
    Diagnostic& operator=(const Diagnostic&) = delete;

    static Base::Result<Diagnostic> TryCreate(
        DiagnosticCode code,
        DiagnosticSeverity severity,
        Base::StringView message,
        SourceSpan source = {},
        DiagnosticObjectId object = InvalidDiagnosticObjectId,
        MemberId member = InvalidMemberId) noexcept;

    Base::Result<void> TryAddNote(
        Base::StringView message,
        SourceSpan source = {}) noexcept;

    DiagnosticCode Code() const noexcept { return code_; }
    DiagnosticSeverity Severity() const noexcept {
        return severity_;
    }
    Base::StringView Message() const noexcept {
        return message_.View();
    }
    SourceSpan Source() const noexcept { return source_; }
    DiagnosticObjectId Object() const noexcept { return object_; }
    MemberId Member() const noexcept { return member_; }
    Base::Span<const DiagnosticNote> Notes() const noexcept {
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
    Base::String message_;
    Base::Vector<DiagnosticNote> notes_;
};

class AERO_API IDiagnosticSink {
public:
    virtual ~IDiagnosticSink() = default;

    virtual Base::Result<void> Report(
        Diagnostic&& diagnostic) noexcept = 0;
};

class AERO_API DiagnosticBag final : public IDiagnosticSink {
public:
    explicit DiagnosticBag(
        std::uint32_t maxDiagnostics = 1024U) noexcept;

    Base::Result<void> Report(
        Diagnostic&& diagnostic) noexcept override;

    Base::Result<void> TryReport(
        DiagnosticCode code,
        DiagnosticSeverity severity,
        Base::StringView message,
        SourceSpan source = {},
        DiagnosticObjectId object = InvalidDiagnosticObjectId,
        MemberId member = InvalidMemberId) noexcept;

    void Clear() noexcept;

    Base::Span<const Diagnostic> Items() const noexcept {
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

} // namespace Aero::Core
