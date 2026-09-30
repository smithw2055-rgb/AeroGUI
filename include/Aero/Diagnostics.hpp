#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>

#include <cstdint>
#include <utility>

namespace Aero {
class DependencyObject;
class RenderDevice;
}

namespace Aero::Diagnostics {

struct SourcePosition {
    // Line and column are one-based. A zero pair represents an unknown position.
    std::uint32_t line = 0U;
    std::uint32_t column = 0U;
    std::uint64_t byteOffset = 0U;

    constexpr bool IsKnown() const noexcept { return line != 0U || column != 0U; }
};

struct SourceSpan {
    // End is exclusive when the source provider can identify it precisely.
    SourcePosition begin;
    SourcePosition end;
};

} // namespace Aero::Diagnostics

namespace Aero::Meta {

struct DependencyPropertyHandle;

// Effective-value source enums and PropertyValueSourceInfo (PropertyProviderSet follows).
enum class EffectiveValueSource : std::uint8_t {
    Default = 0U,
    Local,
    Current
};

enum class PropertyValueRank : std::uint8_t {
    Default = 0U,
    Inherited = 10U,
    ThemeStyleSetter = 20U,
    ThemeStyle = ThemeStyleSetter,
    ThemeStyleTrigger = 30U,
    StyleSetter = 40U,
    Style = StyleSetter,
    TemplateTrigger = 50U,
    StyleTrigger = 60U,
    Trigger = StyleTrigger,
    ImplicitStyle = 70U,
    TemplatedParentSetter = 80U,
    Template = TemplatedParentSetter,
    TemplatedParentTrigger = 90U,
    Local = 100U,
    LocalExpression = Local,
    VisualState = 105U,
    Animation = 110U,
    Coercion = 120U
};

using EffectiveValueProvider = PropertyValueRank;

enum class PropertyExpressionKind : std::uint8_t {
    Custom = 0U,
    Binding,
    DynamicResource
};

using PropertyExpressionEvaluateCallback = Result<Value> (*)(void* context, DependencyObject& object,
    DependencyPropertyHandle property) noexcept;
using PropertyExpressionCleanupCallback = void (*)(void* context) noexcept;

struct PropertyExpression {
    void* context = nullptr;
    PropertyExpressionEvaluateCallback evaluate = nullptr;
    PropertyExpressionCleanupCallback cleanup = nullptr;
    PropertyExpressionKind kind = PropertyExpressionKind::Custom;

    bool IsValid() const noexcept { return evaluate != nullptr; }
};

struct PropertyProviderToken {
    PropertyValueRank rank = PropertyValueRank::Default;
    std::uint32_t origin = 0U;
    std::uint32_t ordinal = 0U;

    constexpr bool IsValid() const noexcept { return rank != PropertyValueRank::Default && origin != 0U; }
};

constexpr bool operator==(PropertyProviderToken left, PropertyProviderToken right) noexcept {
    return left.rank == right.rank && left.origin == right.origin && left.ordinal == right.ordinal;
}

constexpr bool operator!=(PropertyProviderToken left, PropertyProviderToken right) noexcept { return !(left == right); }

struct PropertyValueSourceInfo {
    PropertyValueRank rank = PropertyValueRank::Default;
    PropertyProviderToken token;
    PropertyExpressionKind expressionKind = PropertyExpressionKind::Custom;
    bool hasExpression = false;
    bool isInherited = false;
    bool isAnimated = false;
    bool isCoerced = false;
    bool isCurrentValue = false;
    std::uint64_t revision = 0U;
};

using DependencyObject = ::Aero::DependencyObject;

using PropertyValueKind = ValueKind;
using PropertyValue = Value;

// Local and animation use fixed engine-owned identities. Manager-owned
// Style, ThemeStyle and Template providers allocate from the canonical range.
inline constexpr std::uint32_t LocalValueProviderOrigin = 1U;
inline constexpr std::uint32_t AnimationValueProviderOrigin = 2U;
inline constexpr std::uint32_t VisualStateProviderOrigin = 3U;
inline constexpr std::uint32_t FirstCanonicalProviderOrigin = 16U;

class PropertyProviderOriginAllocator {
public:
    explicit constexpr PropertyProviderOriginAllocator(std::uint32_t first = FirstCanonicalProviderOrigin) noexcept
        : next_(first) {}

    Result<std::uint32_t> Allocate() noexcept {
        if (next_ < FirstCanonicalProviderOrigin || next_ == UINT32_MAX) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange, "Property provider origin limit reached");
        }
        return next_++;
    }

    std::uint32_t Next() const noexcept { return next_; }

private:
    std::uint32_t next_ = FirstCanonicalProviderOrigin;
};

struct PropertyProviderContribution {
    PropertyProviderToken token;
    PropertyValue value;
};

// Canonical token-scoped provider storage. Exact-token writes replace one
// contribution; distinct ordinals represent simultaneous declarations. Higher
// ranks win first, followed by later provider origins and declaration ordinals.
// Origins are allocated by EffectiveValueEngine and are unique across all
// provider sessions attached to that engine.
class PropertyProviderSet {
public:
    bool Set(PropertyProviderToken token, const PropertyValue& value) noexcept {
        if (!token.IsValid() || value.IsUnset()) { return false; } const std::uint32_t existing = Find(token);
        if (existing != UINT32_MAX) {
            contributions_[existing].value = value;
            return true;
        }
        contributions_.PushBack({token, value});
        NoteInserted(contributions_.Size() - 1U);
        return true;
    }

    bool Set(PropertyProviderToken token, PropertyValue&& value) noexcept {
        if (!token.IsValid() || value.IsUnset()) { return false; } const std::uint32_t existing = Find(token);
        if (existing != UINT32_MAX) {
            contributions_[existing].value = std::move(value);
            return true;
        }
        PropertyProviderContribution contribution;
        contribution.token = token;
        contribution.value = std::move(value);
        contributions_.PushBack(std::move(contribution));
        NoteInserted(contributions_.Size() - 1U);
        return true;
    }

    bool Remove(PropertyProviderToken token) noexcept { const std::uint32_t index = Find(token);
        if (index == UINT32_MAX) return false;
        RemoveAt(index);
        return true;
    }

    std::uint32_t RemoveOrigin(std::uint32_t origin) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < contributions_.Size()) {
            if (contributions_[index].token.origin == origin) {
                EraseAtUnchecked(index);
                ++removed;
            } else { ++index; }
        }
        if (removed != 0U) RefreshWinner();
        return removed;
    }

    std::uint32_t Remove(PropertyValueRank rank, std::uint32_t origin) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < contributions_.Size()) { const PropertyProviderToken token = contributions_[index].token;
            if (token.rank == rank && token.origin == origin) {
                EraseAtUnchecked(index);
                ++removed;
            } else { ++index; }
        }
        if (removed != 0U) RefreshWinner();
        return removed;
    }

    std::uint32_t RemoveRank(PropertyValueRank rank) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < contributions_.Size()) {
            if (contributions_[index].token.rank == rank) {
                EraseAtUnchecked(index);
                ++removed;
            } else { ++index; }
        }
        if (removed != 0U) RefreshWinner();
        return removed;
    }

    void Clear() noexcept {
        contributions_.Clear();
        winner_ = UINT32_MAX;
    }

    const PropertyProviderContribution* Winner() const noexcept {
        // C1: cached winner index. Set() maintains it incrementally (one
        // comparison); removals fix it up or rescan once. Hot recompute path
        // drops from O(k) to O(1).
        return winner_ != UINT32_MAX && winner_ < contributions_.Size() ? &contributions_[winner_] : nullptr;
    }

    const PropertyProviderContribution* FindContribution(PropertyProviderToken token) const noexcept {
        const std::uint32_t index = Find(token);
        return index != UINT32_MAX ? &contributions_[index] : nullptr;
    }

    Span<const PropertyProviderContribution> Contributions() const noexcept { return contributions_.AsSpan(); }

    std::uint32_t GetCount() const noexcept { return contributions_.Size(); }

    bool GetIsEmpty() const noexcept { return contributions_.Empty(); }

private:
    Base::Vector<PropertyProviderContribution> contributions_;
    // Cached index of the IsStronger-maximum element, or UINT32_MAX when
    // empty. Order-independent because Winner is defined by comparison.
    std::uint32_t winner_ = UINT32_MAX;

    std::uint32_t Find(PropertyProviderToken token) const noexcept {
        for (std::uint32_t index = 0U;
             index < contributions_.Size();
             ++index) {
            if (contributions_[index].token == token) { return index; }
        }
        return UINT32_MAX;
    }

    void RemoveAt(std::uint32_t index) noexcept {
        // C1: order-independent swap-remove. Winner() selects by
        // IsStronger(rank/origin/ordinal), so element order is irrelevant.
        const std::uint32_t last = contributions_.Size() - 1U;
        if (index != last) { contributions_[index] = std::move(contributions_[last]); }
        contributions_.PopBack();
        if (contributions_.Empty()) {
            winner_ = UINT32_MAX;
        } else if (winner_ == last) {
            // Removed the tail: either the winner itself (rescan) or the
            // element swapped into the removed slot (winner moved).
            winner_ = (index != last) ? index : UINT32_MAX;
            if (winner_ == UINT32_MAX) RefreshWinner();
        } else if (winner_ == index) { RefreshWinner(); }
    }

    void EraseAtUnchecked(std::uint32_t index) noexcept { const std::uint32_t last = contributions_.Size() - 1U;
        if (index != last) { contributions_[index] = std::move(contributions_[last]); }
        contributions_.PopBack();
    }

    void NoteInserted(std::uint32_t index) noexcept {
        if (winner_ == UINT32_MAX || IsStronger(contributions_[index].token,
                contributions_[winner_].token)) { winner_ = index; }
    }

    void RefreshWinner() noexcept {
        winner_ = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < contributions_.Size();
             ++index) {
            if (winner_ == UINT32_MAX || IsStronger(contributions_[index].token,
                    contributions_[winner_].token)) { winner_ = index; }
        }
    }

    static constexpr bool IsStronger(PropertyProviderToken left, PropertyProviderToken right) noexcept {
        if (left.rank != right.rank) {
            return static_cast<std::uint8_t>(left.rank) > static_cast<std::uint8_t>(right.rank);
        }
        if (left.origin != right.origin) { return left.origin > right.origin; }
        return left.ordinal > right.ordinal;
    }
};

} // namespace Aero::Meta

namespace Aero {

struct LayoutDiagnostics {
    std::uint64_t passVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
};

} // namespace Aero

namespace Aero::Diagnostics {

struct RenderDeviceStatistics {
    std::uint64_t acceptedFrameCount = 0U;
    std::uint64_t completedFrameCount = 0U;
    std::uint64_t failedFrameCount = 0U;
    std::uint64_t lastAcceptedVersion = 0U;
    std::uint64_t lastCompletedVersion = 0U;
    std::uint64_t generation = 1U;
};

struct RenderFrameStatistics {
    std::uint32_t sourceCommandCount = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t stateBindingCount = 0U;
    bool batchingEnabled = true;
};

AERO_GUI_API RenderDeviceStatistics GetRenderDeviceStatistics(const Aero::RenderDevice& device) noexcept;
AERO_GUI_API RenderFrameStatistics GetLastRenderFrameStatistics(const Aero::RenderDevice& device) noexcept;

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

    constexpr bool IsValid() const noexcept { const DiagnosticDomain domain = Domain();
        const std::uint16_t number = Number();
        return domain > DiagnosticDomain::Invalid && domain < DiagnosticDomain::Count && number > 0U && number <= 9999U;
    }

    constexpr DiagnosticDomain Domain() const noexcept { return static_cast<DiagnosticDomain>((value >> 16U) & 0xFFU); }

    constexpr std::uint16_t Number() const noexcept { return static_cast<std::uint16_t>(value & 0xFFFFU); }
};

constexpr DiagnosticCode MakeDiagnosticCode(DiagnosticDomain domain, std::uint16_t number) noexcept {
    return domain > DiagnosticDomain::Invalid && domain < DiagnosticDomain::Count && number > 0U && number <= 9999U
        ? DiagnosticCode{
            (static_cast<std::uint32_t>(domain) << 16U) |
            static_cast<std::uint32_t>(number)}
        : DiagnosticCode{};
}

constexpr bool operator==(DiagnosticCode left, DiagnosticCode right) noexcept { return left.value == right.value; }

constexpr bool operator!=(DiagnosticCode left, DiagnosticCode right) noexcept { return !(left == right); }

using DiagnosticObjectId = std::uint64_t;
inline constexpr DiagnosticObjectId InvalidDiagnosticObjectId = 0U;

AERO_GUI_API bool IsValidSourcePosition(SourcePosition position) noexcept;
AERO_GUI_API bool IsValidSourceSpan(SourceSpan span) noexcept;
AERO_GUI_API StringView DiagnosticPrefix(DiagnosticDomain domain) noexcept;
AERO_GUI_API Result<void> FormatDiagnosticCode(DiagnosticCode code, String& output) noexcept;

class AERO_GUI_API DiagnosticNote  {
public:
    DiagnosticNote(DiagnosticNote&&) noexcept = default;
    DiagnosticNote& operator=(DiagnosticNote&&) noexcept = default;

    DiagnosticNote(const DiagnosticNote&) = delete;
    DiagnosticNote& operator=(const DiagnosticNote&) = delete;

    SourceSpan Source() const noexcept { return source_; }
    StringView Message() const noexcept { return message_.View(); }

private:
    friend class Diagnostic;

    DiagnosticNote() noexcept : message_(&Base::GetDefaultAllocator()) {}

    SourceSpan source_;
    String message_;
};

class AERO_GUI_API Diagnostic  {
public:
    Diagnostic(Diagnostic&&) noexcept = default;
    Diagnostic& operator=(Diagnostic&&) noexcept = default;

    Diagnostic(const Diagnostic&) = delete;
    Diagnostic& operator=(const Diagnostic&) = delete;

    static Result<Diagnostic> Create(DiagnosticCode code, DiagnosticSeverity severity, StringView message,
        SourceSpan source = {},
        DiagnosticObjectId object = InvalidDiagnosticObjectId,
        MemberId member = InvalidMemberId) noexcept;

    void AddNote(StringView message, SourceSpan source = {}) noexcept;

    DiagnosticCode Code() const noexcept { return code_; }
    DiagnosticSeverity Severity() const noexcept { return severity_; }
    StringView Message() const noexcept { return message_.View(); }
    SourceSpan Source() const noexcept { return source_; }
    DiagnosticObjectId Object() const noexcept { return object_; }
    MemberId Member() const noexcept { return member_; }
    Span<const DiagnosticNote> Notes() const noexcept { return {notes_.Data(), notes_.Size()}; }
    bool IsError() const noexcept {
        return severity_ == DiagnosticSeverity::Error || severity_ == DiagnosticSeverity::Fatal;
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

    virtual Result<void> Report(Diagnostic&& diagnostic) noexcept = 0;
};

class AERO_GUI_API DiagnosticBag  : public IDiagnosticSink {
public:
    explicit DiagnosticBag(std::uint32_t maxDiagnostics = 1024U) noexcept;

    DiagnosticBag(const DiagnosticBag&) = delete;
    DiagnosticBag& operator=(const DiagnosticBag&) = delete;

    Result<void> Report(Diagnostic&& diagnostic) noexcept override;

    Result<void> Report(DiagnosticCode code, DiagnosticSeverity severity, StringView message, SourceSpan source = {},
        DiagnosticObjectId object = InvalidDiagnosticObjectId,
        MemberId member = InvalidMemberId) noexcept;

    void Clear() noexcept;

    Span<const Diagnostic> Items() const noexcept { return {items_.Data(), items_.Size()}; }
    std::uint32_t Size() const noexcept { return items_.Size(); }
    std::uint32_t MaxDiagnostics() const noexcept { return maxDiagnostics_; }
    std::uint32_t WarningCount() const noexcept { return warningCount_; }
    std::uint32_t ErrorCount() const noexcept { return errorCount_; }
    std::uint32_t DroppedCount() const noexcept { return droppedCount_; }
    bool HasErrors() const noexcept { return errorCount_ != 0U; }

private:
    Base::Vector<Diagnostic> items_;
    std::uint32_t maxDiagnostics_ = 0U;
    std::uint32_t warningCount_ = 0U;
    std::uint32_t errorCount_ = 0U;
    std::uint32_t droppedCount_ = 0U;
};


using PropertyValueRank = Meta::PropertyValueRank;
using PropertyValueSourceInfo = Meta::PropertyValueSourceInfo;
using PropertyProviderToken = Meta::PropertyProviderToken;
using PropertyExpressionKind = Meta::PropertyExpressionKind;

AERO_GUI_API Result<PropertyValueSourceInfo> GetValueSource(const DependencyObject& object,
    Meta::DependencyPropertyHandle property) noexcept;

} // namespace Aero::Diagnostics
