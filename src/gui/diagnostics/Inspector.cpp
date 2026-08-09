#include "Inspector.hpp"
#include "gui/MetadataRuntime.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/FreezableRuntime.hpp"
#include "gui/ElementRuntime.hpp"
#include "gui/RoutedEventRuntime.hpp"
#include "gui/InputRuntime.hpp"
#include "gui/LayoutRuntime.hpp"
#include "gui/BindingRuntime.hpp"
#include "gui/AnimationRuntime.hpp"
#include "gui/StyleRuntime.hpp"

#include <Aero/Controls.hpp>

#include "gui/controls/ControlBehavior.hpp"

namespace Aero::Diagnostics {
namespace {

using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Meta;
using namespace Aero::Threading;


enum class TreeKind : std::uint8_t {
    Logical = 0U,
    Visual,
};

Base::Result<void> AppendTree(
    ::Aero::Media::Visual& node,
    VisualHandle parent,
    std::uint32_t depth,
    TreeKind kind,
    std::uint32_t maxNodes,
    Vector<InspectorTreeNode>&
        output) noexcept {
    if (output.Size() >= maxNodes) {
        return Status::Failure(
            ErrorCode::OutOfRange,
            "Inspector tree node limit "
            "was reached");
    }
    InspectorTreeNode record;
    record.node = &node;
    record.handle = Aero::ElementPrivate::Handle(node);
    record.parent = parent;
    record.runtimeType =
        node.RuntimeType();
    record.depth = depth;
    Base::Result<void> appended =
        output.PushBack(record);
    if (!appended) {
        return appended.GetStatus();
    }
    const Base::Span<::Aero::Media::Visual* const> children =
        kind == TreeKind::Logical
        ? Aero::ElementPrivate::LogicalChildren(node)
        : Aero::ElementPrivate::VisualChildren(node);
    for (::Aero::Media::Visual* child : children) {
        if (child == nullptr) {
            return Status::Failure(
                ErrorCode::InvalidState,
                "Inspector tree contains "
                "a null child");
        }
        Base::Result<void> childResult =
            AppendTree(
                *child,
                record.handle,
                depth + 1U,
                kind,
                maxNodes,
                output);
        if (!childResult) {
            return childResult.GetStatus();
        }
    }
    return {};
}

} // namespace

Base::Result<void>
Inspector::Capture(
    Aero::Media::Visual& target,
    InspectorSnapshot& output,
    std::uint32_t maxTreeNodes)
    const noexcept {
    using namespace Aero::Base;
    using namespace Aero::Controls;
    using namespace Aero::Meta;
using namespace Aero::Threading;


    if (tree_ == nullptr ||
        values_ == nullptr ||
        bindings_ == nullptr ||
        renderer_ == nullptr ||
        maxTreeNodes == 0U ||
        Aero::ElementPrivate::Tree(target) != tree_) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Inspector render target "
            "is invalid");
    }

    output = InspectorSnapshot{};
    output.target = &target;
    ::Aero::Media::Visual* root = tree_->Root();
    if (root == nullptr) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Inspector object tree has "
            "no root");
    }
    Base::Result<void> logical = AppendTree(
        *root,
        {},
        0U,
        TreeKind::Logical,
        maxTreeNodes,
        output.logicalTree);
    if (!logical) {
        return logical.GetStatus();
    }
    Base::Result<void> visual = AppendTree(
        *root,
        {},
        0U,
        TreeKind::Visual,
        maxTreeNodes,
        output.visualTree);
    if (!visual) {
        return visual.GetStatus();
    }

    for (const DependencyProperty&
        property :
        target.PropertyRegistry().
            Properties()) {
        if (property.MetadataFor(
                target.RuntimeType()) ==
            nullptr) {
            continue;
        }
        Base::Result<PropertyValue> value =
            target.GetValue(
                property.Handle());
        if (!value) {
            return value.GetStatus();
        }
        Base::Result<EffectiveValueSource> source =
            target.GetValueSource(
                property.Handle());
        if (!source) {
            return source.GetStatus();
        }
        InspectorProperty inspected;
        inspected.property =
            property.Handle();
        inspected.value =
            value.Value();
        inspected.valueSource =
            source.Value();
        Base::Result<EffectiveValueDiagnostics>
            diagnostics =
                values_->Diagnostics(
                    target,
                    property.Handle());
        if (diagnostics) {
            inspected.diagnostics =
                diagnostics.Value();
            inspected.
                hasEngineDiagnostics = true;
        } else if (
            diagnostics.GetStatus().code !=
                ErrorCode::NotFound) {
            return diagnostics.GetStatus();
        }
        Base::Result<void> appended =
            output.effectiveProperties.
                PushBack(
                    std::move(inspected));
        if (!appended) {
            return appended.GetStatus();
        }
    }

    Base::Result<std::uint32_t> bindings =
        bindings_->InspectBindings(
            target,
            output.activeBindings);
    if (!bindings) {
        return bindings.GetStatus();
    }

    FrameworkElement* element =
        target.AsFrameworkElement();
    if (element != nullptr) {
        Base::Result<Value> context =
            element->GetDataContextResult();
        if (!context) {
            return context.GetStatus();
        }
        output.dataContext =
            std::move(context).Value();
        output.layoutRect =
            element->GetLayoutSlot();
        output.layoutClip =
            element->GetLayoutClip();
        output.renderSize =
            element->GetRenderSize();
    }
    if (styles_ != nullptr) {
        output.appliedStyle =
            styles_->AppliedStyle(target);
    }
    if (templates_ != nullptr &&
        target.PropertyRegistry().
            Types().IsDerivedFrom(
                target.RuntimeType(),
                Control::StaticTypeId())) {
        output.appliedTemplate =
            templates_->AppliedHandle(
                static_cast<Control&>(
                    target));
    }
    output.render =
        renderer_->Diagnostics();
    output.frameTimings =
        target.GetDispatcher().
            FrameTimings();
    return {};
}

} // namespace Aero::Diagnostics
