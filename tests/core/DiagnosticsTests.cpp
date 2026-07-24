#include <Aero/Core/Diagnostics.hpp>
#include "TestAllocatorScope.hpp"

#include <cstdio>
#include <utility>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class RejectAllocator final : public IAllocator {
public:
    void* Allocate(const AllocationRequest&) noexcept override {
        return nullptr;
    }

    void Deallocate(
        void*,
        std::size_t,
        std::size_t,
        MemoryTag) noexcept override {}
};

bool TestDiagnosticCodeFormatting() {
    const DiagnosticCode xaml = MakeDiagnosticCode(DiagnosticDomain::Xaml, 7U);
    CHECK(xaml.IsValid());
    CHECK(xaml.Domain() == DiagnosticDomain::Xaml);
    CHECK(xaml.Number() == 7U);
    CHECK(DiagnosticPrefix(xaml.Domain()) == StringView("XAML"));

    String formatted;
    CHECK(TryFormatDiagnosticCode(xaml, formatted));
    CHECK(formatted == StringView("XAML0007"));

    const DiagnosticCode property = MakeDiagnosticCode(
        DiagnosticDomain::DependencyProperty, 42U);
    CHECK(TryFormatDiagnosticCode(property, formatted));
    CHECK(formatted == StringView("DP0042"));

    const DiagnosticCode invalid = MakeDiagnosticCode(
        DiagnosticDomain::Xaml, 0U);
    CHECK(!invalid.IsValid());
    Result<void> invalidFormat = TryFormatDiagnosticCode(invalid, formatted);
    CHECK(!invalidFormat &&
        invalidFormat.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

bool TestSourceSpanValidation() {
    CHECK(IsValidSourceSpan({}));
    CHECK(IsValidSourceSpan({{0U, 0U, 4U}, {0U, 0U, 9U}}));
    CHECK(IsValidSourceSpan({{2U, 3U, 10U}, {2U, 8U, 15U}}));
    CHECK(IsValidSourceSpan({{2U, 8U, 15U}, {3U, 1U, 20U}}));

    CHECK(!IsValidSourcePosition({2U, 0U, 0U}));
    CHECK(!IsValidSourceSpan({{0U, 0U, 0U}, {1U, 1U, 0U}}));
    CHECK(!IsValidSourceSpan({{3U, 1U, 20U}, {2U, 8U, 21U}}));
    CHECK(!IsValidSourceSpan({{2U, 8U, 15U}, {2U, 7U, 16U}}));
    CHECK(!IsValidSourceSpan({{2U, 3U, 10U}, {2U, 8U, 9U}}));
    return true;
}

bool TestDiagnosticBagOrderingAndCounts() {
    DiagnosticBag bag(3U);
    const DiagnosticCode warningCode = MakeDiagnosticCode(
        DiagnosticDomain::Layout, 12U);
    const DiagnosticCode errorCode = MakeDiagnosticCode(
        DiagnosticDomain::Xaml, 21U);
    const DiagnosticCode infoCode = MakeDiagnosticCode(
        DiagnosticDomain::Base, 3U);

    CHECK(bag.TryReport(
        warningCode,
        DiagnosticSeverity::Warning,
        StringView("Layout value was clamped"),
        {{1U, 1U, 0U}, {1U, 5U, 4U}}));

    Result<Diagnostic> error = Diagnostic::TryCreate(
        errorCode,
        DiagnosticSeverity::Error,
        StringView("Unknown object element"),
        {{4U, 2U, 18U}, {4U, 10U, 26U}},
        99U,
        55U);
    CHECK(error);
    CHECK(error.Value().TryAddNote(
        StringView("The namespace resolved successfully"),
        {{1U, 1U, 0U}, {1U, 20U, 19U}}));
    CHECK(bag.Report(std::move(error).Value()));

    CHECK(bag.TryReport(
        infoCode,
        DiagnosticSeverity::Info,
        StringView("Parser initialized")));

    CHECK(bag.Size() == 3U);
    CHECK(bag.WarningCount() == 1U);
    CHECK(bag.ErrorCount() == 1U);
    CHECK(bag.HasErrors());

    const Span<const Diagnostic> items = bag.Items();
    CHECK(items[0].Code() == warningCode);
    CHECK(items[1].Code() == errorCode);
    CHECK(items[2].Code() == infoCode);
    CHECK(items[1].Object() == 99U);
    CHECK(items[1].Member() == 55U);
    CHECK(items[1].Notes().Size() == 1U);
    CHECK(items[1].Notes()[0].Message() ==
        StringView("The namespace resolved successfully"));

    Result<void> overflow = bag.TryReport(
        MakeDiagnosticCode(DiagnosticDomain::Render, 1U),
        DiagnosticSeverity::Fatal,
        StringView("This diagnostic exceeds the configured limit"));
    CHECK(!overflow && overflow.GetStatus().code == ErrorCode::OutOfRange);
    CHECK(bag.Size() == 3U);
    CHECK(bag.DroppedCount() == 1U);

    bag.Clear();
    CHECK(bag.Size() == 0U);
    CHECK(bag.WarningCount() == 0U);
    CHECK(bag.ErrorCount() == 0U);
    CHECK(bag.DroppedCount() == 0U);
    CHECK(!bag.HasErrors());
    return true;
}

bool TestValidationAndOutOfMemory() {
    Result<Diagnostic> invalidCode = Diagnostic::TryCreate(
        {},
        DiagnosticSeverity::Error,
        StringView("Invalid code"));
    CHECK(!invalidCode &&
        invalidCode.GetStatus().code == ErrorCode::InvalidArgument);

    Result<Diagnostic> emptyMessage = Diagnostic::TryCreate(
        MakeDiagnosticCode(DiagnosticDomain::Base, 1U),
        DiagnosticSeverity::Error,
        StringView());
    CHECK(!emptyMessage &&
        emptyMessage.GetStatus().code == ErrorCode::InvalidArgument);

    Result<Diagnostic> diagnostic = Diagnostic::TryCreate(
        MakeDiagnosticCode(DiagnosticDomain::Base, 2U),
        DiagnosticSeverity::Warning,
        StringView("Inline message"));
    CHECK(diagnostic);

    RejectAllocator reject;
    Aero::Tests::ScopedDefaultAllocator allocatorScope(reject);
    DiagnosticBag bag(1U);
    Result<void> report = bag.Report(std::move(diagnostic).Value());
    CHECK(!report && report.GetStatus().code == ErrorCode::OutOfMemory);
    CHECK(bag.Size() == 0U);
    CHECK(bag.WarningCount() == 0U);
    CHECK(bag.DroppedCount() == 0U);
    return true;
}

} // namespace

int main() {
    if (!TestDiagnosticCodeFormatting()) return 1;
    if (!TestSourceSpanValidation()) return 1;
    if (!TestDiagnosticBagOrderingAndCounts()) return 1;
    if (!TestValidationAndOutOfMemory()) return 1;
    std::puts("Aero diagnostics tests passed");
    return 0;
}
