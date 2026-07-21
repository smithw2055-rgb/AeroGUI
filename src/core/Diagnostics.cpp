#include <Aero/Core/Diagnostics.hpp>

#include <utility>

namespace Aero::Core {
namespace {

Base::IAllocator* ResolveAllocator(Base::IAllocator* allocator) noexcept {
    return allocator != nullptr ? allocator : &Base::GetDefaultAllocator();
}

bool IsValidSeverity(DiagnosticSeverity severity) noexcept {
    return static_cast<std::uint8_t>(severity) <=
        static_cast<std::uint8_t>(DiagnosticSeverity::Fatal);
}

void IncrementSaturated(std::uint32_t& value) noexcept {
    if (value != UINT32_MAX) {
        ++value;
    }
}

} // namespace

bool IsValidSourcePosition(SourcePosition position) noexcept {
    return (position.line == 0U && position.column == 0U) ||
        (position.line != 0U && position.column != 0U);
}

bool IsValidSourceSpan(SourceSpan span) noexcept {
    if (!IsValidSourcePosition(span.begin) ||
        !IsValidSourcePosition(span.end) ||
        span.begin.IsKnown() != span.end.IsKnown()) {
        return false;
    }

    if (span.end.byteOffset < span.begin.byteOffset) {
        return false;
    }

    if (!span.begin.IsKnown()) {
        return true;
    }

    if (span.end.line < span.begin.line) {
        return false;
    }
    if (span.end.line == span.begin.line &&
        span.end.column < span.begin.column) {
        return false;
    }
    return true;
}

Base::StringView DiagnosticPrefix(DiagnosticDomain domain) noexcept {
    switch (domain) {
    case DiagnosticDomain::Base:
        return Base::StringView("BASE");
    case DiagnosticDomain::Xaml:
        return Base::StringView("XAML");
    case DiagnosticDomain::DependencyProperty:
        return Base::StringView("DP");
    case DiagnosticDomain::Binding:
        return Base::StringView("BIND");
    case DiagnosticDomain::Layout:
        return Base::StringView("LAYOUT");
    case DiagnosticDomain::Input:
        return Base::StringView("INPUT");
    case DiagnosticDomain::Render:
        return Base::StringView("RENDER");
    case DiagnosticDomain::Rhi:
        return Base::StringView("RHI");
    case DiagnosticDomain::GlContext:
        return Base::StringView("GLCTX");
    case DiagnosticDomain::WebGl:
        return Base::StringView("WEBGL");
    case DiagnosticDomain::Platform:
        return Base::StringView("PLATFORM");
    case DiagnosticDomain::Dependency:
        return Base::StringView("DEPEND");
    case DiagnosticDomain::Invalid:
    case DiagnosticDomain::Count:
        return {};
    }
    return {};
}

Base::Result<void> TryFormatDiagnosticCode(
    DiagnosticCode code,
    Base::String& output) noexcept {
    if (!code.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic code is invalid");
    }

    const Base::StringView prefix = DiagnosticPrefix(code.Domain());
    if (prefix.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic domain has no stable prefix");
    }

    const std::uint16_t number = code.Number();
    const char digits[4] = {
        static_cast<char>('0' + (number / 1000U) % 10U),
        static_cast<char>('0' + (number / 100U) % 10U),
        static_cast<char>('0' + (number / 10U) % 10U),
        static_cast<char>('0' + number % 10U)};

    Base::Result<void> result = output.TryAssignUnchecked(prefix);
    if (!result) {
        return result.GetStatus();
    }
    result = output.TryAppendUnchecked(Base::StringView(digits, 4U));
    return result ? Base::Result<void>() : Base::Result<void>(result.GetStatus());
}

Diagnostic::Diagnostic(Base::IAllocator* allocator) noexcept
    : allocator_(ResolveAllocator(allocator)),
      message_(allocator_),
      notes_(allocator_) {}

Base::Result<Diagnostic> Diagnostic::TryCreate(
    DiagnosticCode code,
    DiagnosticSeverity severity,
    Base::StringView message,
    SourceSpan source,
    DiagnosticObjectId object,
    MemberId member,
    Base::IAllocator* allocator) noexcept {
    if (!code.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic code is invalid");
    }
    if (!IsValidSeverity(severity)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic severity is invalid");
    }
    if (message.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic message must not be empty");
    }
    if (!IsValidSourceSpan(source)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic source span is invalid");
    }

    Diagnostic diagnostic(allocator);
    Base::Result<void> assignResult = diagnostic.message_.TryAssign(message);
    if (!assignResult) {
        return assignResult.GetStatus();
    }

    diagnostic.code_ = code;
    diagnostic.severity_ = severity;
    diagnostic.source_ = source;
    diagnostic.object_ = object;
    diagnostic.member_ = member;
    return std::move(diagnostic);
}

Base::Result<void> Diagnostic::TryAddNote(
    Base::StringView message,
    SourceSpan source) noexcept {
    if (message.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic note message must not be empty");
    }
    if (!IsValidSourceSpan(source)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic note source span is invalid");
    }

    DiagnosticNote note(allocator_);
    note.source_ = source;
    Base::Result<void> assignResult = note.message_.TryAssign(message);
    if (!assignResult) {
        return assignResult.GetStatus();
    }

    return notes_.TryPushBack(std::move(note));
}

DiagnosticBag::DiagnosticBag(
    std::uint32_t maxDiagnostics,
    Base::IAllocator* allocator) noexcept
    : allocator_(ResolveAllocator(allocator)),
      items_(allocator_),
      maxDiagnostics_(maxDiagnostics) {}

Base::Result<void> DiagnosticBag::Report(Diagnostic&& diagnostic) noexcept {
    if (!diagnostic.Code().IsValid() ||
        !IsValidSeverity(diagnostic.Severity()) ||
        diagnostic.Message().Empty() ||
        !IsValidSourceSpan(diagnostic.Source())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Diagnostic is not in a reportable state");
    }

    if (items_.Size() >= maxDiagnostics_) {
        IncrementSaturated(droppedCount_);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Diagnostic limit reached");
    }

    const DiagnosticSeverity severity = diagnostic.Severity();
    Base::Result<void> appendResult = items_.TryPushBack(std::move(diagnostic));
    if (!appendResult) {
        return appendResult.GetStatus();
    }

    if (severity == DiagnosticSeverity::Warning) {
        IncrementSaturated(warningCount_);
    } else if (severity == DiagnosticSeverity::Error ||
        severity == DiagnosticSeverity::Fatal) {
        IncrementSaturated(errorCount_);
    }
    return {};
}

Base::Result<void> DiagnosticBag::TryReport(
    DiagnosticCode code,
    DiagnosticSeverity severity,
    Base::StringView message,
    SourceSpan source,
    DiagnosticObjectId object,
    MemberId member) noexcept {
    Base::Result<Diagnostic> diagnostic = Diagnostic::TryCreate(
        code,
        severity,
        message,
        source,
        object,
        member,
        allocator_);
    if (!diagnostic) {
        return diagnostic.GetStatus();
    }
    return Report(std::move(diagnostic).Value());
}

void DiagnosticBag::Clear() noexcept {
    items_.Clear();
    warningCount_ = 0U;
    errorCount_ = 0U;
    droppedCount_ = 0U;
}

} // namespace Aero::Core
