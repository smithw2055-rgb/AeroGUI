#pragma once

#include "render/RenderTree.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Documents.hpp>
#include <Aero/Layout.hpp>
#include "gui/meta/MetadataState.hpp" 
#include "gui/input/InputState.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/core/facets/VisualFacet.hpp"
#include "gui/core/facets/RenderFacet.hpp"
#include <Aero/FrameworkElement.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Diagnostics {

enum class AccessibilityRole : std::uint8_t {
    Unknown = 0U,
    Window,
    Group,
    Button,
    CheckBox,
    RadioButton,
    Text,
    TextBox,
    List,
    ListItem,
    ScrollBar,
    Slider,
    Document,
    Link,
};

enum class AccessibilityAction : std::uint32_t {
    None = 0U,
    Invoke = 1U << 0U,
    Toggle = 1U << 1U,
    Focus = 1U << 2U,
    SetValue = 1U << 3U,
    Scroll = 1U << 4U,
    Navigate = 1U << 5U,
};

using AccessibilityActionFlags = std::uint32_t;

constexpr AccessibilityActionFlags AccessibilityActionBit(
    AccessibilityAction action) noexcept {
    return static_cast<AccessibilityActionFlags>(action);
}

struct AccessibilityNode {
    std::uint64_t id = 0U;
    std::uint64_t parent = 0U;
    AccessibilityRole role = AccessibilityRole::Unknown;
    AccessibilityActionFlags actions = 0U;
    Base::String name;
    Base::String value;
    Aero::Rect bounds;
    bool enabled = true;
    bool focusable = false;
    bool focused = false;
    bool hidden = false;
};

class AccessibilityTree {
public:
    explicit AccessibilityTree(
        Base::IAllocator* allocator = nullptr) noexcept
        : nodes_(allocator) {}

    Base::Result<void> Capture(
        const Aero::ElementTree& tree) noexcept {
        nodes_.Clear();
        const Aero::Media::Visual* root = tree.Root();
        return root != nullptr ? CaptureNode(*root) : Base::Result<void>{};
    }

    Base::Result<void> Add(
        AccessibilityNode node) noexcept {
        if (node.id == 0U ||
            !Aero::IsValidLayoutRect(node.bounds)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Accessibility node is invalid");
        }
        for (const AccessibilityNode& existing : nodes_) {
            if (existing.id == node.id) {
                return Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    "Accessibility node ID is duplicated");
            }
        }
        if (node.parent != 0U && Find(node.parent) == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Accessibility parent must precede its child");
        }
        if (node.focused && (!node.focusable || node.hidden || !node.enabled)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Focused accessibility node must be enabled, visible and focusable");
        }
        return nodes_.PushBack(std::move(node));
    }

    const AccessibilityNode* Find(std::uint64_t id) const noexcept {
        for (const AccessibilityNode& node : nodes_) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }
    Base::Span<const AccessibilityNode> Nodes() const noexcept {
        return nodes_.AsSpan();
    }
    void Clear() noexcept { nodes_.Clear(); }

private:
    Base::Vector<AccessibilityNode> nodes_;

    static std::uint64_t NodeId(
        Aero::VisualHandle handle) noexcept {
        return handle.IsValid()
            ? (static_cast<std::uint64_t>(handle.generation) << 32U) |
                  (static_cast<std::uint64_t>(handle.index) + 1U)
            : 0U;
    }

    Base::Result<void> CaptureNode(
        const Aero::Media::Visual& visual) noexcept {
        AccessibilityNode node;
        node.id = NodeId(Aero::Core::VisualFacet::Handle(visual));
        const Aero::Media::Visual* parent = visual.GetLogicalParent();
        if (parent == nullptr) parent = visual.GetVisualParent();
        node.parent = parent != nullptr ? NodeId(Aero::Core::VisualFacet::Handle(*parent)) : 0U;
        const Aero::UIElement* element = visual.AsUIElement();
        if (element != nullptr) {
            node.bounds = element->GetLayoutSlot();
            node.enabled = element->GetIsEnabled();
            node.focusable = element->GetFocusable();
            node.focused = element->GetIsKeyboardFocused();
            node.hidden = !element->GetIsVisible();
        }
        const Meta::TypeId type = visual.RuntimeType();
        const bool isHyperlink =
            type == Documents::Hyperlink::StaticTypeId();
        const bool isDocumentText = isHyperlink ||
            type == Documents::TextElement::StaticTypeId() ||
            type == Documents::Inline::StaticTypeId() ||
            type == Documents::Run::StaticTypeId() ||
            type == Documents::Span::StaticTypeId() ||
            type == Documents::Bold::StaticTypeId() ||
            type == Documents::Italic::StaticTypeId() ||
            type == Documents::Underline::StaticTypeId() ||
            type == Documents::LineBreak::StaticTypeId() ||
            type == Controls::TextBlock::StaticTypeId();
        if (isDocumentText && element != nullptr) {
            const auto& text =
                static_cast<const Controls::TextBlock&>(*element);
            node.role = isHyperlink
                ? AccessibilityRole::Link
                : (text.InlineCount() != 0U
                    ? AccessibilityRole::Document
                    : AccessibilityRole::Text);
            Base::String flattened;
            Base::Result<void> copied =
                Documents::CopyText(text, flattened);
            if (!copied) return copied.GetStatus();
            Base::Result<void> named =
                node.name.Assign(flattened.View());
            if (!named) return named.GetStatus();
            named = node.value.Assign(flattened.View());
            if (!named) return named.GetStatus();
        }
        if (isHyperlink) {
            const auto& link =
                static_cast<const Documents::Hyperlink&>(*element);
            Base::Result<void> value =
                node.value.Assign(link.NavigateUri());
            if (!value) return value.GetStatus();
            node.actions = AccessibilityActionBit(
                AccessibilityAction::Invoke) |
                AccessibilityActionBit(AccessibilityAction::Focus) |
                AccessibilityActionBit(AccessibilityAction::Navigate);
        }
        if (node.id != 0U) {
            Base::Result<void> added = Add(std::move(node));
            if (!added) return added.GetStatus();
        }
        const Base::Span<Aero::Media::Visual* const> logical =
            visual.GetLogicalChildren();
        for (Aero::Media::Visual* child : logical) {
            if (child == nullptr) continue;
            Base::Result<void> captured = CaptureNode(*child);
            if (!captured) return captured.GetStatus();
        }
        for (Aero::Media::Visual* child : visual.GetVisualChildren()) {
            if (child == nullptr) continue;
            bool alreadyCaptured = false;
            for (Aero::Media::Visual* logicalChild : logical) {
                if (logicalChild == child) {
                    alreadyCaptured = true;
                    break;
                }
            }
            if (alreadyCaptured) continue;
            Base::Result<void> captured = CaptureNode(*child);
            if (!captured) return captured.GetStatus();
        }
        return {};
    }
};

using AccessibilityPublishCallback = Base::Result<void> (*)(
    Base::Span<const AccessibilityNode> nodes,
    void* context) noexcept;
using AccessibilityActionCallback = Base::Result<void> (*)(
    std::uint64_t node,
    AccessibilityAction action,
    Base::StringView value,
    void* context) noexcept;

struct AccessibilityPlatformAdapter {
    std::uint32_t abiVersion = 1U;
    AccessibilityPublishCallback publish = nullptr;
    AccessibilityActionCallback performAction = nullptr;
    void* context = nullptr;

    bool IsValid() const noexcept {
        return abiVersion == 1U && publish != nullptr &&
            performAction != nullptr;
    }
};

struct InspectorTreeNode {
    Aero::VisualHandle handle;
    Aero::VisualHandle logicalParent;
    Aero::VisualHandle visualParent;
    Base::MetaTypeId runtimeType = Base::InvalidMetaTypeId;
    Aero::Rect layoutSlot;
    Aero::Size renderSize;
    std::uint64_t layoutRevision = 0U;
    std::uint64_t renderRevision = 0U;
    bool loaded = false;
    bool measureValid = false;
    bool arrangeValid = false;
    bool renderValid = false;
};

struct InspectorRenderSummary {
    std::uint64_t version = 0U;
    std::uint64_t stableHash = 0U;
    std::uint32_t nodeCount = 0U;
    std::uint32_t commandCount = 0U;
};

class RuntimeInspectorSnapshot {
public:
    explicit RuntimeInspectorSnapshot(
        Base::IAllocator* allocator = nullptr) noexcept
        : nodes_(allocator) {}

    Base::Result<void> Capture(
        const Aero::ElementTree& tree,
        const ::Aero::Render::RenderFrame* plan = nullptr) noexcept {
        nodes_.Clear();
        const Aero::Media::Visual* root = tree.Root();
        if (root != nullptr) {
            Base::Result<void> captured = CaptureNode(*root);
            if (!captured) return captured.GetStatus();
        }
        render_ = {};
        if (plan != nullptr) {
            render_.version = plan->Version();
            render_.stableHash = plan->StableHash();
            render_.nodeCount = plan->Nodes().Size();
            render_.commandCount = plan->Commands().Size();
        }
        treeVersion_ = tree.Version();
        return {};
    }

    Base::Span<const InspectorTreeNode> Nodes() const noexcept {
        return nodes_.AsSpan();
    }
    InspectorRenderSummary Render() const noexcept { return render_; }
    std::uint64_t TreeVersion() const noexcept { return treeVersion_; }

private:
    Base::Vector<InspectorTreeNode> nodes_;
    InspectorRenderSummary render_;
    std::uint64_t treeVersion_ = 0U;

    Base::Result<void> CaptureNode(
        const Aero::Media::Visual& visual) noexcept {
        InspectorTreeNode node;
        node.handle = Aero::Core::VisualFacet::Handle(visual);
        node.runtimeType = visual.RuntimeType();
        node.loaded = visual.GetIsLoaded();
        if (visual.GetLogicalParent() != nullptr) {
            node.logicalParent = Aero::Core::VisualFacet::Handle(*visual.GetLogicalParent());
        }
        if (visual.GetVisualParent() != nullptr) {
            node.visualParent = Aero::Core::VisualFacet::Handle(*visual.GetVisualParent());
        }
        const Aero::UIElement* element = visual.AsUIElement();
        if (element != nullptr) {
            node.layoutSlot = element->GetLayoutSlot();
            node.renderSize = element->GetRenderSize();
            node.layoutRevision = element->GetLayoutRevision();
            node.measureValid = element->GetIsMeasureValid();
            node.arrangeValid = element->GetIsArrangeValid();
        }
        const Aero::FrameworkElement* framework =
            visual.AsFrameworkElement();
        if (framework != nullptr) {
            node.renderRevision = Aero::Core::RenderFacet::RenderRevision(const_cast<Aero::FrameworkElement&>(*framework));
            node.renderValid = Aero::Core::RenderFacet::RenderValid(const_cast<Aero::FrameworkElement&>(*framework));
        }
        Base::Result<void> appended = nodes_.PushBack(node);
        if (!appended) return appended.GetStatus();
        for (Aero::Media::Visual* child : visual.GetLogicalChildren()) {
            if (child == nullptr) continue;
            Base::Result<void> captured = CaptureNode(*child);
            if (!captured) return captured.GetStatus();
        }
        return {};
    }
};

enum class PerformanceMetric : std::uint8_t {
    EmptyFrameMicroseconds = 0U,
    LayoutMicroseconds,
    PropertyUpdateMicroseconds,
    RenderFrameMicroseconds,
    VirtualizedScrollMicroseconds,
    AllocationBytes,
    DrawCalls,
};

struct PerformanceBudget {
    PerformanceMetric metric = PerformanceMetric::EmptyFrameMicroseconds;
    double limit = 0.0;
};

struct PerformanceMeasurement {
    PerformanceMetric metric = PerformanceMetric::EmptyFrameMicroseconds;
    double value = 0.0;
};

struct PerformanceGateResult {
    std::uint32_t evaluated = 0U;
    std::uint32_t failed = 0U;
    double worstRatio = 0.0;
};

class PerformanceGate {
public:
    explicit PerformanceGate(
        Base::IAllocator* allocator = nullptr) noexcept
        : budgets_(allocator), measurements_(allocator) {}

    Base::Result<void> SetBudget(
        PerformanceMetric metric,
        double limit) noexcept {
        if (!(limit > 0.0)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Performance budget must be positive");
        }
        for (PerformanceBudget& budget : budgets_) {
            if (budget.metric == metric) {
                budget.limit = limit;
                return {};
            }
        }
        return budgets_.PushBack({metric, limit});
    }

    Base::Result<void> Record(
        PerformanceMetric metric,
        double value) noexcept {
        if (!(value >= 0.0)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Performance measurement must be nonnegative");
        }
        return measurements_.PushBack({metric, value});
    }

    Base::Result<PerformanceGateResult> Evaluate() const noexcept {
        PerformanceGateResult result;
        for (const PerformanceMeasurement& measurement : measurements_) {
            const PerformanceBudget* budget = nullptr;
            for (const PerformanceBudget& candidate : budgets_) {
                if (candidate.metric == measurement.metric) {
                    budget = &candidate;
                    break;
                }
            }
            if (budget == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Performance measurement has no locked budget");
            }
            ++result.evaluated;
            const double ratio = measurement.value / budget->limit;
            if (ratio > result.worstRatio) result.worstRatio = ratio;
            if (measurement.value > budget->limit) ++result.failed;
        }
        return result;
    }

    void ClearMeasurements() noexcept { measurements_.Clear(); }

private:
    Base::Vector<PerformanceBudget> budgets_;
    Base::Vector<PerformanceMeasurement> measurements_;
};

using FuzzTargetCallback = Base::Result<void> (*)(
    Base::Span<const std::uint8_t> input,
    void* context) noexcept;

struct FuzzTarget {
    Base::String name;
    FuzzTargetCallback callback = nullptr;
    void* context = nullptr;
};

struct FuzzRunResult {
    std::uint32_t targetCount = 0U;
    std::uint32_t caseCount = 0U;
    std::uint32_t failureCount = 0U;
};

class FuzzHarness {
public:
    explicit FuzzHarness(
        Base::IAllocator* allocator = nullptr) noexcept
        : targets_(allocator) {}

    Base::Result<void> AddTarget(
        Base::StringView name,
        FuzzTargetCallback callback,
        void* context = nullptr) noexcept {
        if (name.Empty() || callback == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Fuzz target is incomplete");
        }
        FuzzTarget target;
        Base::Result<void> named = target.name.Assign(name);
        if (!named) return named.GetStatus();
        target.callback = callback;
        target.context = context;
        return targets_.PushBack(std::move(target));
    }

    FuzzRunResult Run(
        Base::Span<const Base::Span<const std::uint8_t>> cases) noexcept {
        FuzzRunResult result;
        result.targetCount = targets_.Size();
        for (FuzzTarget& target : targets_) {
            for (const Base::Span<const std::uint8_t>& input : cases) {
                ++result.caseCount;
                Base::Result<void> status = target.callback(input, target.context);
                if (!status) ++result.failureCount;
            }
        }
        return result;
    }
private:
    Base::Vector<FuzzTarget> targets_;
};

struct CapabilityManifest {
    bool cxx17 = true;
    bool exceptionsOff = true;
    bool rttiOff = true;
    bool d3d11 = false;
    bool openGl33 = false;
    bool gles30 = false;
    bool vulkan = false;
    bool d3d12 = false;
    bool metal = false;
    bool webGl2 = false;
    bool accessibility = false;
    bool inspector = false;

    Base::Result<void> ValidateProductionM4() const noexcept {
        if (!cxx17 || !exceptionsOff || !rttiOff ||
            !d3d11 || !openGl33 || !gles30 || !vulkan ||
            !d3d12 || !metal || !webGl2 ||
            !accessibility || !inspector) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Capability manifest does not satisfy the public M4 matrix");
        }
        return {};
    }
};

class StabilityCounter {
public:
    Base::Result<void> RecordFrame(
        std::uint64_t liveObjects,
        std::uint64_t liveGpuBytes) noexcept {
        if (frames_ == UINT64_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Stability frame counter is exhausted");
        }
        ++frames_;
        if (liveObjects > peakObjects_) peakObjects_ = liveObjects;
        if (liveGpuBytes > peakGpuBytes_) peakGpuBytes_ = liveGpuBytes;
        lastObjects_ = liveObjects;
        lastGpuBytes_ = liveGpuBytes;
        return {};
    }
    std::uint64_t Frames() const noexcept { return frames_; }
    std::uint64_t PeakObjects() const noexcept { return peakObjects_; }
    std::uint64_t PeakGpuBytes() const noexcept { return peakGpuBytes_; }
    bool IsStable(
        std::uint64_t objectLimit,
        std::uint64_t gpuByteLimit) const noexcept {
        return peakObjects_ <= objectLimit && peakGpuBytes_ <= gpuByteLimit;
    }
private:
    std::uint64_t frames_ = 0U;
    std::uint64_t peakObjects_ = 0U;
    std::uint64_t peakGpuBytes_ = 0U;
    std::uint64_t lastObjects_ = 0U;
    std::uint64_t lastGpuBytes_ = 0U;
};

} // namespace Aero::Diagnostics
