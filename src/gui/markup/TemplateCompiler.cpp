#include "gui/core/TypeRegistryCore.hpp"
#include "gui/templates/TemplateBlueprint.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include "gui/markup/XamlObjectWriterState.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>
#include <cstdio>
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/markup/XamlPropertyResolution.hpp"
#include "gui/core/PropertyStore.hpp"
#include "gui/core/DependencyObjectAccess.hpp"

// ===== TemplateCompiler =====


#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/media/AnimationModel.hpp"
#include <Aero/Documents.hpp>
#include <Aero/TextProperties.hpp>


#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Controls/Control.hpp>


namespace Aero::Markup {
namespace {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace Aero::Media;
using Controls::Primitives::Selector;
using namespace Data;
using Documents::Inline; // not Documents:: -- avoid Span collision with Aero::Span
using namespace Media::Animation;

Base::Status InvalidTemplateCompiler(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

Visual* WalkTemplateAncestor(
    Visual* start,
    Base::StringView ancestorType,
    std::uint32_t level,
    const Meta::TypeRegistry& types) noexcept {
    if (start == nullptr || level == 0U) return nullptr;
    Base::StringView ancestorName = ancestorType;
    for (std::uint32_t index = 0U; index < ancestorName.SizeBytes(); ++index) {
        if (ancestorName[index] == ':') {
            ancestorName = ancestorName.Substr(
                index + 1U, ancestorName.SizeBytes() - index - 1U);
            break;
        }
    }
    std::uint32_t matched = 0U;
    Visual* current = start->GetVisualParent();
    if (current == nullptr) current = TryCast<Visual>(start->GetLogicalParent());
    if (current == nullptr) {
        if (auto* fe = TryCast<FrameworkElement>(start)) {
            current = TryCast<Visual>(fe->GetTemplatedParent());
        }
    }
    while (current != nullptr) {
        const TypeInfo* type = types.FindType(current->RuntimeType());
        if ((ancestorName.Empty() ||
             (type != nullptr && type->Name() == ancestorName)) &&
            ++matched == level) {
            return current;
        }
        Visual* next = current->GetVisualParent();
        if (next == nullptr) next = TryCast<Visual>(current->GetLogicalParent());
        if (next == nullptr) {
            if (auto* fe = TryCast<FrameworkElement>(current)) {
                next = TryCast<Visual>(fe->GetTemplatedParent());
            }
        }
        current = next;
    }
    return nullptr;
}

Base::Object* ResolveTemplateBindingAncestor(
    const TemplatePrototypeBinding& binding,
    Base::Object& target,
    Meta::Registry& runtime) noexcept {
    if (binding.relativeAncestorType.Empty() ||
        binding.relativeAncestorLevel == 0U ||
        !runtime.Types().IsDerivedFrom(
            target.RuntimeType(), Visual::StaticTypeId())) {
        return nullptr;
    }
    return WalkTemplateAncestor(
        static_cast<Visual*>(&target),
        binding.relativeAncestorType.View(),
        binding.relativeAncestorLevel,
        runtime.Types());
}

struct PendingPrototypeNode {
    Base::Ref<Base::Object> object;
    std::uint32_t parent = UINT32_MAX;
    MemberId contentMember = InvalidMemberId;
};

bool RequiresPrototypeObject(
    Base::Span<const DeferredBindingEdge> bindings,
    const Base::Object* object) noexcept {
    for (const DeferredBindingEdge& binding : bindings) {
        if (binding.target == object ||
            binding.source == object) {
            return true;
        }
    }
    return false;
}

bool HasContentEdges(
    Base::Span<const DeferredContentEdge> edges,
    const Base::Object* object) noexcept {
    for (const DeferredContentEdge& edge : edges) {
        if (edge.parent == object) {
            return true;
        }
    }
    return false;
}

Base::Result<Value> ConvertTemplateTextValue(
    Meta::Registry& runtime,
    const DependencyProperty& property,
    Base::StringView text) noexcept {
    // WPF uses -1 for no selection. Internally selection keeps UINT32_MAX;
    // translate only the standard selection properties at the XAML boundary.
    if ((property.Handle() == Selector::SelectedIndexProperty.Handle() ||
         property.Handle() == TabControl::SelectedIndexProperty.Handle()) &&
        text == Base::StringView("-1")) {
        return ValueCodec<std::uint32_t>::Encode(UINT32_MAX);
    }
    if (property.ValueType() == Meta::TypeOf<Base::Object>()) {
        Base::Result<Value> strVal =
            Value::TryFromString(Meta::TypeOf<Base::String>(), text);
        if (!strVal) return strVal.GetStatus();
        Base::Result<Base::Ref<Controls::BoxedItemValue>> boxed =
            Base::MakeRef<Controls::BoxedItemValue>(std::move(strVal).Value());
        if (!boxed) return boxed.GetStatus();
        return Value::FromObject(
            property.ValueType(),
            Base::Ref<Base::Object>(std::move(boxed).Value()));
    }
    if (property.ValueType() == Meta::TypeOf<::Aero::Nullable<bool>>()) {
        if (text == Base::StringView("True") || text == Base::StringView("true")) {
            return ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{true});
        }
        if (text == Base::StringView("False") || text == Base::StringView("false")) {
            return ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{false});
        }
        if (text == Base::StringView("{x:Null}") || text == Base::StringView("Null") || text == Base::StringView("null")) {
            return ValueCodec<::Aero::Nullable<bool>>::Encode(::Aero::Nullable<bool>{});
        }
    }
    return runtime.TryConvertText(property.ValueType(), text);
}

inline const DependencyProperty* ResolveTemplateProperty(
    const DependencyPropertyRegistry& properties,
    TypeId targetType,
    Base::StringView name) noexcept {
    return ResolveXamlDependencyProperty(properties, targetType, name);
}

std::uint32_t FindPrototypeObject(
    const Base::Vector<PendingPrototypeNode>& pending,
    const Base::Object* object) noexcept {
    for (std::uint32_t index = 0U;
         index < pending.Size();
         ++index) {
        if (pending[index].object.Get() == object) {
            return index;
        }
    }
    return UINT32_MAX;
}

bool IsGradientStopObject(
    Meta::Registry& runtime,
    const Base::Object& object) noexcept {
    return runtime.Types().IsDerivedFrom(
        object.RuntimeType(),
        GradientStop::StaticTypeId());
}


Base::Result<CompiledTemplateBlueprint>
CompileBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Base::Span<const TemplateMetadataBindingPlan>
        metadataBindings,
    Base::Span<const TemplateDynamicResourcePlan>
        dynamicResources,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (!visualTree) {
        return InvalidTemplateCompiler(
            "Template requires a VisualTree");
    }

    CompiledTemplateBlueprint blueprint;
    blueprint.runtime = &runtime;
    blueprint.properties = &properties;

    Base::Vector<PendingPrototypeNode> pending;
    Base::Result<void> appended{};
    pending.PushBack({
            visualTree,
            UINT32_MAX,
            InvalidMemberId});

    // Templated-parent bindings may target Freezables nested in a visual
    // property (for example a GradientStop.Color binding). Visuals and
    // TransformGroup children are discovered from content edges so authored
    // order is preserved; pre-inserting RotateTransform made clock hands
    // compile as parentless nodes and drop out of TransformGroup.Children.
    auto preInsertNamedTarget = [&](Base::StringView targetName, const char* targetTypeDesc) -> Base::Result<void> {
        if (targetName.Empty() || names == nullptr) return {};
        Base::Object* target = names->Find(targetName);
        if (target == nullptr ||
            IsGradientStopObject(runtime, *target) ||
            runtime.Types().IsDerivedFrom(
                target->RuntimeType(), Visual::StaticTypeId()) ||
            runtime.Types().IsDerivedFrom(
                target->RuntimeType(), Transform::StaticTypeId())) {
            return {};
        }
        if (!runtime.Types().IsDerivedFrom(
                target->RuntimeType(), DependencyObject::StaticTypeId())) {
            thread_local char message[128];
            std::snprintf(message, sizeof(message), "%s target is not a DependencyObject", targetTypeDesc);
            return InvalidTemplateCompiler(message);
        }
        if (FindPrototypeObject(pending, target) != UINT32_MAX) return {};
        pending.PushBack({
            Base::Ref<Base::Object>::FromBorrowed(*target),
            UINT32_MAX,
            InvalidMemberId});
        return {};
    };

    for (const TemplateMetadataBindingPlan& binding : metadataBindings) {
        Base::Result<void> res = preInsertNamedTarget(binding.targetName.View(), "TemplatedParent Binding");
        if (!res) return res.GetStatus();
    }
    for (const TemplateDynamicResourcePlan& resource : dynamicResources) {
        Base::Result<void> res = preInsertNamedTarget(resource.targetName.View(), "DynamicResource");
        if (!res) return res.GetStatus();
    }

    for (std::uint32_t index = 0U;
         index < pending.Size(); ++index) {
        const PendingPrototypeNode& source =
            pending[index];
        Base::Object* object = source.object.Get();
        if (object == nullptr ||
            !runtime.Types().IsDerivedFrom(
                object->RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Template graph contains a non-dependency object");
        }
        const bool visual = runtime.Types().IsDerivedFrom(
            object->RuntimeType(), Visual::StaticTypeId());
        if (index == 0U && !visual) {
            return InvalidTemplateCompiler(
                "Template VisualTree root must be a Visual object");
        }

        TemplatePrototypeNode node;
        node.type = object->RuntimeType();
        node.parent = source.parent;
        node.contentMember = source.contentMember;
        const Base::StringView name = names != nullptr
            ? names->NameOf(*object)
            : Base::StringView{};
        Base::Result<void> named =
            node.name.Assign(name);
        if (!named) return named.GetStatus();

        if (auto* framework = ::Aero::TryCast<FrameworkElement>(object)) {
            for (const auto& b : FrameworkElementSeams::AuthoredBehaviors(*framework)) {
                node.authoredBehaviors.PushBack(b);
            }
            for (const auto& t : FrameworkElementSeams::AuthoredTriggers(*framework)) {
                node.authoredTriggers.PushBack(t);
            }
        }

        auto& dependencyObject =
            static_cast<DependencyObject&>(*object);
        const PropertyStore* store = DependencyObjectAccess::Store(dependencyObject);
        if (store != nullptr) {
            for (const auto& entry : store->entries) {
                if (!entry.Value().HasLocal()) {
                    continue;
                }
                const DependencyProperty* property =
                    properties.Find(DependencyPropertyHandle{entry.Key()});
                if (property == nullptr || property->MetadataFor(node.type) == nullptr) {
                    continue;
                }
                Base::Result<Value> local =
                    dependencyObject.ReadLocalValue(
                        property->Handle());
                if (!local) return local.GetStatus();
                if (local.Value().IsUnset()) continue;
                TemplatePrototypeProperty prototypeProperty;
                prototypeProperty.property = property->Handle();
                prototypeProperty.value = local.Value();
                if (local.Value().Kind() == ValueKind::Object &&
                    !local.Value().IsNullObject() &&
                    local.Value().AsObject() &&
                    runtime.Types().IsDerivedFrom(
                        local.Value().AsObject()->RuntimeType(),
                        DependencyObject::StaticTypeId())) {
                    prototypeProperty.objectNode = FindPrototypeObject(
                        pending, local.Value().AsObject().Get());
                    const bool hasEdges = HasContentEdges(
                        edges, local.Value().AsObject().Get());
                    const bool isFreezable = runtime.Types().IsDerivedFrom(
                        local.Value().AsObject()->RuntimeType(),
                        Freezable::StaticTypeId());
                    const bool requiresClone =
                        prototypeProperty.objectNode != UINT32_MAX ||
                        RequiresPrototypeObject(
                            bindings, local.Value().AsObject().Get()) ||
                        hasEdges ||
                        isFreezable;
                    if (requiresClone &&
                        prototypeProperty.objectNode == UINT32_MAX) {
                        prototypeProperty.objectNode = pending.Size();
                        pending.PushBack({
                            local.Value().AsObject(),
                            UINT32_MAX,
                            InvalidMemberId});
                    }
                }
                node.properties.PushBack(
                    std::move(prototypeProperty));
            }
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                Grid::StaticTypeId())) {
            const auto& grid =
                static_cast<const Grid&>(*object);
            node.gridColumns.Assign(
                grid.GetColumnDefinitions());
            node.gridRows.Assign(
                grid.GetRowDefinitions());
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                StreamGeometry::StaticTypeId())) {
            const auto& geom =
                static_cast<const StreamGeometry&>(*object);
            appended = node.streamGeometryData.Assign(
                geom.GetData());
            if (!appended) {
                return appended.GetStatus();
            }
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                GradientBrush::StaticTypeId())) {
            const auto& gradient =
                static_cast<const GradientBrush&>(*object);
            for (const Base::Ref<GradientStop>& stop : gradient.GetGradientStops()) {
                if (!stop) continue;
                TemplatePrototypeGradientStop record;
                record.offset = stop->GetOffset();
                record.color = stop->GetColor();
                if (names != nullptr) {
                    appended = record.name.Assign(
                        names->NameOf(*stop));
                    if (!appended) return appended.GetStatus();
                }
                node.gradientStops.PushBack(
                    std::move(record));
            }
        }

        const bool contentPresenterCandidate =
            runtime.Types().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId()) ||
            runtime.Types().IsDerivedFrom(
                node.type,
                ScrollContentPresenter::StaticTypeId());
        if (contentPresenterCandidate) {
            constexpr Base::StringView ScrollPresenterPart(
                "PART_ScrollContentPresenter");
            constexpr Base::StringView HeaderSource("Header");
            Base::StringView contentSource;
            for (const TemplatePrototypeProperty& property :
                 node.properties) {
                if (property.property ==
                        ContentPresenter::ContentSourceProperty.Handle() &&
                    property.value.Kind() == ValueKind::String) {
                    contentSource = property.value.AsString();
                    break;
                }
            }
            const bool headerPresenter = contentSource == HeaderSource;
            const bool explicitScrollPresenter =
                node.name.View() == ScrollPresenterPart;
            const bool exactScrollPresenter =
                node.type == ScrollContentPresenter::StaticTypeId();
            // ContentSource=Header presenters display Header, not Content.
            // Projecting Content onto them is wrong and can cycle when the
            // only available "content" is the template root itself.
            if (!headerPresenter ||
                explicitScrollPresenter ||
                exactScrollPresenter) {
                bool replace = blueprint.contentPresenter == UINT32_MAX;
                if (!replace &&
                    blueprint.contentPresenter < blueprint.nodes.Size()) {
                    const TemplatePrototypeNode& selected =
                        blueprint.nodes[blueprint.contentPresenter];
                    const bool selectedExplicit =
                        selected.name.View() == ScrollPresenterPart;
                    const bool selectedExact =
                        selected.type == ScrollContentPresenter::StaticTypeId();
                    replace = explicitScrollPresenter ||
                        (!selectedExplicit && exactScrollPresenter &&
                         !selectedExact);
                }
                if (replace) {
                    blueprint.contentPresenter = index;
                }
            }
        }

        blueprint.nodes.PushBack(
            std::move(node));

        for (const DeferredContentEdge& edge :
             edges) {
            if (edge.parent != object) continue;
            if (!edge.child) {
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a null child");
            }
            if (IsGradientStopObject(runtime, *edge.child)) {
                // GradientStops are cloned from the authored brush collection
                // so TemplatedParent Color bindings target the instance that
                // actually lives in the applied LinearGradientBrush.
                continue;
            }
            const std::uint32_t existing =
                FindPrototypeObject(pending, edge.child.Get());
            if (existing != UINT32_MAX) {
                if (existing == index) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate visual content contains a cycle");
                }
                PendingPrototypeNode& existingNode = pending[existing];
                if (existingNode.parent == UINT32_MAX) {
                    // A dependency object can first enter the prototype graph
                    // because a Binding or object-valued property references
                    // it, and later appear on its actual visual-content edge.
                    // Retain one clone and attach that clone at the authored
                    // location instead of treating the two discovery paths as
                    // duplicate visual content.
                    existingNode.parent = index;
                    existingNode.contentMember = edge.member;
                    if (existing < blueprint.nodes.Size()) {
                        blueprint.nodes[existing].parent = index;
                        blueprint.nodes[existing].contentMember = edge.member;
                    }
                    continue;
                }
                if (existingNode.parent == index &&
                    existingNode.contentMember == edge.member) {
                    // Structural and content facets can report the same edge.
                    // It is one authored child, not two template instances.
                    continue;
                }
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a duplicate child");
            }
            pending.PushBack({
                edge.child,
                index,
                edge.member});
        }

        // String inlines (Hyperlink "Show XAML", inter-Run spaces) live on
        // TextBlock/Span owned collections and may not have DeferredContentEdge
        // records. Clone them so ControlTemplate instances keep the text.
        if (runtime.Types().IsDerivedFrom(
                object->RuntimeType(),
                TextBlock::StaticTypeId()) ||
            runtime.Types().IsDerivedFrom(
                object->RuntimeType(),
                Documents::Span::StaticTypeId())) {
            const Meta::PropertyInfo* inlinesProperty =
                runtime.Types().FindProperty(
                    object->RuntimeType(),
                    Base::StringView("Inlines"),
                    true);
            const MemberId inlinesMember =
                inlinesProperty != nullptr
                ? inlinesProperty->Id()
                : InvalidMemberId;
            const std::uint32_t inlineCount =
                runtime.Types().IsDerivedFrom(
                    object->RuntimeType(),
                    TextBlock::StaticTypeId())
                ? static_cast<TextBlock&>(*object).GetInlineCount()
                : static_cast<Documents::Span&>(*object).GetInlines().GetCount();
            for (std::uint32_t inlineIndex = 0U;
                 inlineIndex < inlineCount;
                 ++inlineIndex) {
                Inline* inlineValue = nullptr;
                if (runtime.Types().IsDerivedFrom(
                        object->RuntimeType(),
                        TextBlock::StaticTypeId())) {
                    inlineValue = static_cast<TextBlock&>(*object)
                        .GetInlines().GetItem(inlineIndex);
                } else {
                    inlineValue = static_cast<Documents::Span&>(*object)
                        .GetInlines().GetItem(inlineIndex);
                }
                if (inlineValue == nullptr) {
                    continue;
                }
                const std::uint32_t existing =
                    FindPrototypeObject(pending, inlineValue);
                if (existing != UINT32_MAX) {
                    PendingPrototypeNode& existingNode = pending[existing];
                    if (existingNode.parent == UINT32_MAX) {
                        existingNode.parent = index;
                        existingNode.contentMember = inlinesMember;
                        if (existing < blueprint.nodes.Size()) {
                            blueprint.nodes[existing].parent = index;
                            blueprint.nodes[existing].contentMember =
                                inlinesMember;
                        }
                    }
                    continue;
                }
                pending.PushBack({
                    Base::Ref<Base::Object>::FromBorrowed(*inlineValue),
                    index,
                    inlinesMember});
            }
        }
    }

    // Generated names from EnsureAuthoredName live on the template NameScope.
    // Unnamed TemplatedParent Binding targets must carry that name onto the
    // prototype node so apply-time FindTarget can find the part. WPF binds the
    // unnamed child itself, not the templated parent.
    auto assignGeneratedTargetName = [&](Base::StringView targetName) -> Base::Result<void> {
        if (targetName.Empty() || names == nullptr) return {};
        Base::Object* target = names->Find(targetName);
        if (target == nullptr) return {};
        const std::uint32_t targetIndex = FindPrototypeObject(pending, target);
        if (targetIndex == UINT32_MAX || targetIndex >= blueprint.nodes.Size()) return {};
        if (!blueprint.nodes[targetIndex].name.Empty()) return {};
        return blueprint.nodes[targetIndex].name.Assign(targetName);
    };

    for (const TemplateMetadataBindingPlan& binding : metadataBindings) {
        Base::Result<void> named = assignGeneratedTargetName(binding.targetName.View());
        if (!named) return named.GetStatus();
    }
    for (const TemplateDynamicResourcePlan& resource : dynamicResources) {
        Base::Result<void> named = assignGeneratedTargetName(resource.targetName.View());
        if (!named) return named.GetStatus();
    }

    for (const DeferredBindingEdge& source :
         bindings) {
        const std::uint32_t target =
            FindPrototypeObject(
                pending, source.target);
        Base::Object* resolvedSource = source.source;
        if (resolvedSource == nullptr &&
            !source.sourceName.Empty() && names != nullptr) {
            resolvedSource = names->Find(source.sourceName.View());
        }
        const std::uint32_t bindingSource =
            resolvedSource != nullptr
            ? FindPrototypeObject(pending, resolvedSource)
            : UINT32_MAX;
        const bool externalElementName =
            resolvedSource == nullptr &&
            !source.sourceName.Empty();
        if (target == UINT32_MAX ||
            (source.source != nullptr && bindingSource == UINT32_MAX) ||
            source.metadata == nullptr) {
            return InvalidTemplateCompiler(
                "Deferred template Binding target or source is outside its VisualTree");
        }
        TemplatePrototypeBinding binding;
        binding.target = target;
        binding.source = bindingSource;
        binding.metadata = source.metadata;
        binding.targetProperty =
            source.targetProperty;
        binding.dataContextProperty =
            source.dataContextProperty;
        binding.bindsToSource =
            source.bindsToSource;
        binding.mode = source.mode;
        binding.updateSourceTrigger =
            source.updateSourceTrigger;
        binding.converter = source.converter;
        binding.converterParameter = source.converterParameter;
        if (externalElementName) {
            Base::Result<void> sourceName =
                binding.sourceName.Assign(
                    source.sourceName.View());
            if (!sourceName) return sourceName.GetStatus();
        }
        Base::Result<void> assigned =
            binding.relativeAncestorType.Assign(
            source.relativeAncestorType.View());
        if (!assigned) return assigned.GetStatus();
        binding.relativeAncestorLevel =
            source.relativeAncestorLevel;
        assigned = binding.path.Assign(
                source.path.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = binding.stringFormat.Assign(
            source.stringFormat.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        blueprint.bindings.PushBack(
            std::move(binding));
    }

    return blueprint;
}

const TemplatePrototypeNode* FindNode(
    const CompiledTemplateBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (const TemplatePrototypeNode& node :
         blueprint.nodes) {
        if (node.name.View() == name) {
            return &node;
        }
    }
    return nullptr;
}

std::uint32_t FindNodeIndex(
    const CompiledTemplateBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (std::uint32_t index = 0U;
         index < blueprint.nodes.Size();
         ++index) {
        if (blueprint.nodes[index].name.View() == name) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<Value> ConvertAndValidateTemplateValue(
    Value value,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    const char* roleName) noexcept {
    if (value.IsUnset()) {
        thread_local char message[128];
        std::snprintf(message, sizeof(message), "%s requires Value", roleName);
        return InvalidTemplateCompiler(message);
    }
    if (value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Binding::StaticTypeId()) {
        return value;
    }
    if (value.IsDynamicResource()) {
        return value;
    }
    if (property.AcceptsAnyValue()) {
        return value;
    }
    if (value.Kind() == ValueKind::String &&
        value.Type() != property.ValueType()) {
        Base::Result<Value> converted =
            ConvertTemplateTextValue(
                runtime, property, value.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        value = std::move(converted).Value();
    }
    Base::Result<Value> coerced = CoerceXamlValueForProperty(
        property, value, runtime.Types());
    if (!coerced) return coerced.GetStatus();
    value = std::move(coerced).Value();

    const bool objectCompatible =
        value.Kind() == ValueKind::Object &&
        (value.IsNullObject() ||
         runtime.Types().IsDerivedFrom(
             value.Type(),
             property.ValueType()));
    if (value.Type() != property.ValueType() &&
        !objectCompatible) {
        thread_local char message[256];
        const TypeInfo* expected = runtime.Types().FindType(
            property.ValueType());
        const TypeInfo* actual = runtime.Types().FindType(value.Type());
        std::snprintf(
            message, sizeof(message),
            "%s '%.*s' expects '%.*s' but received '%.*s'",
            roleName,
            static_cast<int>(property.Name().SizeBytes()), property.Name().Data(),
            expected != nullptr ? static_cast<int>(expected->Name().SizeBytes()) : 9,
            expected != nullptr ? expected->Name().Data() : "<unknown>",
            actual != nullptr ? static_cast<int>(actual->Name().SizeBytes()) : 9,
            actual != nullptr ? actual->Name().Data() : "<unknown>");
        return InvalidTemplateCompiler(message);
    }
    Base::Result<void> valid =
        properties.ValidateValueFor(
            property.Handle(),
            targetType,
            value);
    return valid
        ? Base::Result<Value>(value)
        : Base::Result<Value>(
              valid.GetStatus());
}

Base::Result<Value> ConvertSetterValue(
    const Setter& setter,
    const TemplatePrototypeNode& target,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return ConvertAndValidateTemplateValue(
        setter.GetAuthoredValue(), target.type, property, runtime, properties, "Setter");
}

Base::Status MissingTemplateProperty(
    const char* role,
    Base::StringView property,
    TypeId targetType,
    const TypeRegistry& types) noexcept {
    thread_local char message[384];
    const TypeInfo* target =
        types.FindType(targetType);
    const Base::StringView typeName =
        target != nullptr
        ? target->Name()
        : Base::StringView("<unknown>");
    std::snprintf(
        message,
        sizeof(message),
        "ControlTemplate %s '%.*s' was not found on TargetType '%.*s'",
        role,
        static_cast<int>(property.SizeBytes()),
        property.Data(),
        static_cast<int>(typeName.SizeBytes()),
        typeName.Data());
    return InvalidTemplateCompiler(message);
}

Base::Result<Value> ConvertTriggerValue(
    Value value,
    TypeId targetType,
    const DependencyProperty& property,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return ConvertAndValidateTemplateValue(
        value, targetType, property, runtime, properties, "Template Trigger");
}

Base::Result<Base::Vector<TemplatePropertyTrigger>>
CompilePropertyTriggers(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<TemplatePropertyTrigger> compiled;
    auto addCondition = [&controlTemplate, &blueprint, &runtime, &properties](
        Base::StringView propertyName,
        Base::StringView sourceName,
        const Value& authoredValue,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        if (propertyName.Empty() || authoredValue.IsUnset()) {
            return InvalidTemplateCompiler(
                "ControlTemplate Trigger condition requires Property and Value");
        }
        TypeId targetType = controlTemplate.GetTargetType();
        if (!sourceName.Empty()) {
            const TemplatePrototypeNode* target = FindNode(blueprint, sourceName);
            if (target == nullptr) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger source was not found");
            }
            targetType = target->type;
        }
        const DependencyProperty* property =
            ResolveTemplateProperty(properties, targetType, propertyName);
        if (property == nullptr) {
            return MissingTemplateProperty(
                "Trigger property", propertyName, targetType, runtime.Types());
        }
        Base::Result<Value> value = ConvertTriggerValue(
            authoredValue, targetType, *property, runtime, properties);
        if (!value) return value.GetStatus();
        TemplateTriggerCondition condition;
        Base::Result<void> assigned =
            condition.sourceName.Assign(sourceName);
        if (!assigned) return assigned.GetStatus();
        condition.property = property->Handle();
        condition.value = std::move(value).Value();
        trigger.conditions.PushBack(std::move(condition));
        return {};
    };
    auto appendSetters = [&controlTemplate, &blueprint, &runtime, &properties](
        Base::Span<const Base::Ref<Setter>> setters,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        for (const Base::Ref<Setter>& setterObject : setters) {
            if (!setterObject || setterObject->GetPropertyName().Empty()) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter requires Property");
            }
            const Base::StringView targetName = setterObject->GetTargetName();
            TypeId targetType = controlTemplate.GetTargetType();
            const TemplatePrototypeNode* target = nullptr;
            if (!targetName.Empty()) {
                target = FindNode(blueprint, targetName);
                if (target == nullptr) {
                    // RowDefinition/ColumnDefinition and similar named
                    // declaration objects do not belong to the visual clone
                    // graph. They remain represented by the owning Grid's
                    // serialized definition vectors; their trigger mutation
                    // is deferred until that declaration-object runtime is
                    // materialized.
                    if (FrameworkTemplateState::AuthoredNames(controlTemplate).Find(
                            targetName) != nullptr) {
                        continue;
                    }
                    return InvalidTemplateCompiler(
                        "ControlTemplate Trigger Setter target was not found");
                }
                targetType = target->type;
            }
            const DependencyProperty* property = ResolveTemplateProperty(
                properties, targetType, setterObject->GetPropertyName());
            if (property == nullptr) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter property was not found");
            }
            TemplatePrototypeNode parentTarget;
            parentTarget.type = targetType;
            Base::Result<Value> value = ConvertSetterValue(
                *setterObject,
                target != nullptr ? *target : parentTarget,
                *property, runtime, properties);
            if (!value) return value.GetStatus();
            TemplateTriggerSetter setter;
            Base::Result<void> assigned = setter.targetName.Assign(targetName);
            if (!assigned) return assigned.GetStatus();
            setter.property = property->Handle();
            setter.value = std::move(value).Value();
            trigger.setters.PushBack(std::move(setter));
        }
        return {};
    };
    for (const Base::Ref<Base::Object>& object :
         FrameworkTemplateState::AuthoredTriggers(controlTemplate)) {
        TemplatePropertyTrigger trigger;
        Base::Result<void> configured;
        if (object && object->RuntimeType() == Trigger::StaticTypeId()) {
            const auto& source = static_cast<const Trigger&>(*object);
            configured = addCondition(source.GetPropertyName(), source.GetSourceName(),
                source.GetAuthoredValue(), trigger);
            if (configured) configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else if (object && object->RuntimeType() == MultiTrigger::StaticTypeId()) {
            const auto& source = static_cast<const MultiTrigger&>(*object);
            for (const Base::Ref<Condition>& condition : source.GetConditions()) {
                if (!condition) return InvalidTemplateCompiler(
                    "MultiTrigger contains a null Condition");
                configured = addCondition(condition->GetPropertyName(), condition->GetSourceName(),
                    condition->GetAuthoredValue(), trigger);
                if (!configured) return configured.GetStatus();
            }
            configured = appendSetters(source.GetAuthoredSetters(), trigger);
        } else {
            // Data and event triggers are compiled into the instance runtime
            // plan below. They cannot be represented by the dependency-
            // property trigger table used by TemplateEngine.
            continue;
        }
        if (!configured) return configured.GetStatus();
        Base::Result<void> added{};
        compiled.PushBack(
                std::move(trigger));
    }
    return compiled;
}

Base::Result<Base::Vector<VisualStateGroupPlan>>
CompileVisualStates(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<VisualStateGroupPlan> groups;
    auto compileGroup = [&groups, &blueprint, &runtime, &properties](
        const Base::Ref<Base::Object>& groupObject)
        -> Base::Result<void> {
        if (!groupObject ||
            groupObject->RuntimeType() !=
                VisualStateGroup::
                    StaticTypeId()) {
            return InvalidTemplateCompiler(
                "ControlTemplate VisualStateGroups contains an invalid object");
        }
        const auto& sourceGroup =
            static_cast<
                const VisualStateGroup&>(
                    *groupObject);
        if (sourceGroup.GetName().Empty()) {
            return InvalidTemplateCompiler(
                "VisualStateGroup requires Name");
        }
        VisualStateGroupPlan group;
        Base::Result<void> assigned =
            group.name.Assign(
                sourceGroup.GetName());
        if (!assigned) return assigned.GetStatus();

        for (const Base::Ref<VisualState>& stateObject :
             sourceGroup.GetStates()) {
            if (!stateObject ||
                stateObject->RuntimeType() !=
                    VisualState::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualState");
            }
            const auto& sourceState =
                static_cast<
                const VisualState&>(
                    *stateObject);
            if (sourceState.GetName().Empty()) {
                return InvalidTemplateCompiler(
                    "VisualState requires Name");
            }
            VisualStatePlan state;
            assigned = state.name.Assign(
                sourceState.GetName());
            if (!assigned) {
                return assigned.GetStatus();
            }
            if (auto srcStoryboard = sourceState.GetStoryboard()) {
                auto storyboardCopy =
                    Base::MakeRef<Storyboard>();
                if (!storyboardCopy) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                for (const auto& tl : srcStoryboard->GetTimelines()) {
                    if (!tl) continue;
                    Base::Ref<Timeline> cloned;
                    if (tl->RuntimeType() == DoubleAnimationUsingKeyFrames::StaticTypeId()) {
                        auto* srcDouble = static_cast<DoubleAnimationUsingKeyFrames*>(tl.Get());
                        auto dst = Base::MakeRef<DoubleAnimationUsingKeyFrames>();
                        if (!dst) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                        Base::Result<PropertyValue> nameVal = srcDouble->GetValue(Storyboard::TargetNameProperty.Handle());
                        if (nameVal && nameVal.Value().Kind() == Meta::ValueKind::String) {
                            (void)dst->SetValue(Storyboard::TargetNameProperty.Handle(), nameVal.Value());
                        }
                        Base::Result<PropertyValue> propVal = srcDouble->GetValue(Storyboard::TargetPropertyProperty.Handle());
                        if (propVal && propVal.Value().Kind() == Meta::ValueKind::String) {
                            (void)dst->SetValue(Storyboard::TargetPropertyProperty.Handle(), propVal.Value());
                        }
                        for (auto& kf : srcDouble->GetKeyFrames()) {
                            if (!kf) continue;
                            dst->AddKeyFrame(kf);
                        }
                        cloned = dst;
                    } else if (tl->RuntimeType() == ColorAnimationUsingKeyFrames::StaticTypeId()) {
                        auto* srcColor = static_cast<ColorAnimationUsingKeyFrames*>(tl.Get());
                        auto dst = Base::MakeRef<ColorAnimationUsingKeyFrames>();
                        if (!dst) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                        Base::Result<PropertyValue> nameVal = srcColor->GetValue(Storyboard::TargetNameProperty.Handle());
                        if (nameVal && nameVal.Value().Kind() == Meta::ValueKind::String) {
                            (void)dst->SetValue(Storyboard::TargetNameProperty.Handle(), nameVal.Value());
                        }
                        Base::Result<PropertyValue> propVal = srcColor->GetValue(Storyboard::TargetPropertyProperty.Handle());
                        if (propVal && propVal.Value().Kind() == Meta::ValueKind::String) {
                            (void)dst->SetValue(Storyboard::TargetPropertyProperty.Handle(), propVal.Value());
                        }
                        for (auto& kf : srcColor->GetKeyFrames()) {
                            if (!kf) continue;
                            dst->AddKeyFrame(kf);
                        }
                        cloned = dst;
                    } else {
                        cloned = tl;
                    }
                    storyboardCopy->AddTimeline(cloned);
                }
                state.storyboard = std::move(storyboardCopy);
            } else {
                state.storyboard.Reset();
            }

            for (const Base::Ref<Base::Object>&
                     setterObject :
                 sourceState.GetSetters()) {
                if (!setterObject ||
                    setterObject->RuntimeType() !=
                        Setter::StaticTypeId()) {
                    return InvalidTemplateCompiler(
                        "VisualState contains an invalid Setter");
                }
                const auto& sourceSetter =
                    static_cast<const Setter&>(
                        *setterObject);
                if (sourceSetter.GetTargetName().Empty() ||
                    sourceSetter.GetPropertyName().Empty()) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter requires TargetName and Property");
                }
                const TemplatePrototypeNode* target =
                    FindNode(
                        blueprint,
                        sourceSetter.GetTargetName());
                if (target == nullptr) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter target was not found");
                }
                const DependencyProperty* property =
                    ResolveTemplateProperty(
                        properties, target->type,
                        sourceSetter.GetPropertyName());
                if (property == nullptr) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter property was not found");
                }
                Base::Result<Value> value =
                    ConvertSetterValue(
                        sourceSetter,
                        *target,
                        *property,
                        runtime,
                        properties);
                if (!value) {
                    return value.GetStatus();
                }

                VisualStateSetterPlan setter;
                assigned =
                    setter.targetName.Assign(
                        sourceSetter.GetTargetName());
                if (!assigned) {
                    return assigned.GetStatus();
                }
                setter.property = property->Handle();
                setter.value =
                    std::move(value).Value();
                state.setters.PushBack(
                        std::move(setter));
            }
            group.states.PushBack(
                    std::move(state));
        }
        for (const Base::Ref<VisualTransition>&
                  transitionObject :
              sourceGroup.GetTransitions()) {
            if (!transitionObject ||
                transitionObject->RuntimeType() !=
                    VisualTransition::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualTransition");
            }
            const auto& sourceTransition =
                static_cast<
                const VisualTransition&>(
                    *transitionObject);
            if (sourceTransition.GetFrom().Empty() &&
                sourceTransition.GetTo().Empty() &&
                sourceTransition.GetGeneratedDuration().Empty() &&
                !sourceTransition.GetStoryboard()) {
                return InvalidTemplateCompiler(
                    "VisualTransition must specify a duration or Storyboard");
            }
            if (!sourceTransition.GetFrom().Empty()) {
                bool found = false;
                for (const VisualStatePlan& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.GetFrom();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition From state was not found");
                }
            }
            if (!sourceTransition.GetTo().Empty()) {
                bool found = false;
                for (const VisualStatePlan& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.GetTo();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition To state was not found");
                }
            }

            VisualTransitionPlan transition;
            assigned = transition.from.Assign(
                sourceTransition.GetFrom());
            if (assigned) {
                assigned = transition.to.Assign(
                    sourceTransition.GetTo());
            }
            if (!assigned) return assigned.GetStatus();
            if (!sourceTransition.GetGeneratedDuration().Empty()) {
                Storyboard duration;
                duration.SetDuration(sourceTransition.GetGeneratedDuration());
                transition.generatedDurationMicroseconds =
                    Timing(duration).durationMicroseconds;
            }
            transition.generatedEasingFunction =
                sourceTransition.GetGeneratedEasingFunction();
            if (auto srcStoryboard = sourceTransition.GetStoryboard()) {
                auto storyboardCopy =
                    Base::MakeRef<Storyboard>();
                if (!storyboardCopy) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
                for (const auto& tl : srcStoryboard->GetTimelines()) {
                    if (!tl) continue;
                    storyboardCopy->AddTimeline(tl);
                }
                transition.storyboard = std::move(storyboardCopy);
            } else {
                transition.storyboard.Reset();
            }
            group.transitions.PushBack(
                std::move(transition));
        }
        groups.PushBack(
            std::move(group));
        return {};
    };
    for (const Base::Ref<Base::Object>& groupObject :
         FrameworkTemplateState::AuthoredVisualStateGroups(controlTemplate)) {
        Base::Result<void> compiled = compileGroup(groupObject);
        if (!compiled) return compiled.GetStatus();
    }
    Base::Ref<Base::Object> authoredRoot =
        FrameworkTemplateState::AuthoredVisualTree(controlTemplate);
    if (authoredRoot &&
        runtime.Types().IsDerivedFrom(
            authoredRoot->RuntimeType(),
            DependencyObject::StaticTypeId())) {
        auto& root = static_cast<DependencyObject&>(*authoredRoot);
        Base::Ref<VisualStateGroupCollection> valueStore = root.GetValue(VisualStateManager::VisualStateGroupsProperty);
        if (valueStore) {
            for (const Base::Ref<VisualStateGroup>& groupObject :
                 valueStore->GetItems()) {
                Base::Result<void> compiled = compileGroup(
                    Base::Ref<Base::Object>(groupObject));
                if (!compiled) return compiled.GetStatus();
            }
        }
    }
    return groups;
}

} // namespace

Base::Result<CompiledTriggerPlan> CompileTriggerPlan(
    Base::Ref<Aero::TriggerBase> trigger,
    const CompiledTemplateBlueprint& blueprint,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept {
    if (!trigger) {
        return InvalidTemplateCompiler("Null trigger");
    }
    CompiledTriggerPlan plan;
    plan.trigger = trigger;
    Base::Span<const Base::Ref<Setter>> setters;
    if (trigger->RuntimeType() == Trigger::StaticTypeId() ||
        trigger->RuntimeType() == DataTrigger::StaticTypeId() ||
        trigger->RuntimeType() == MultiDataTrigger::StaticTypeId()) {
        setters = static_cast<const SetterTriggerBase&>(*trigger).GetAuthoredSetters();
    }
    for (const Base::Ref<Setter>& setter : setters) {
        if (!setter) continue;
        const std::uint32_t target = setter->GetTargetName().Empty()
            ? 0U : FindNodeIndex(blueprint, setter->GetTargetName());
        if (target >= blueprint.nodes.Size()) {
            return InvalidTemplateCompiler(
                "Template trigger Setter target was not found");
        }
        const DependencyProperty* property = ResolveTemplateProperty(
            properties, blueprint.nodes[target].type, setter->GetPropertyName());
        if (property == nullptr) {
            return InvalidTemplateCompiler(
                "Template trigger Setter property was not found");
        }
        Base::Result<Value> value = ConvertSetterValue(
            *setter, blueprint.nodes[target], *property, runtime, properties);
        if (!value) return value.GetStatus();
        CompiledTriggerSetter compiledSetter;
        compiledSetter.target = target;
        compiledSetter.property = property->Handle();
        compiledSetter.value = std::move(value).Value();
        plan.setters.PushBack(std::move(compiledSetter));
    }
    return plan;
}

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<CompiledTemplateBlueprint> blueprint =
        CompileBlueprint(
        FrameworkTemplateState::AuthoredVisualTree(controlTemplate),
        &FrameworkTemplateState::AuthoredNames(controlTemplate),
        edges,
        bindings,
        FrameworkTemplateState::MetadataBindings(
            controlTemplate),
        FrameworkTemplateState::DynamicResources(
            controlTemplate),
        runtime,
        properties);
    if (!blueprint) {
        return blueprint.GetStatus();
    }
    Base::Result<Base::Vector<VisualStateGroupPlan>>
        groups = CompileVisualStates(
            controlTemplate,
            blueprint.Value(),
            runtime,
            properties);
    if (!groups) return groups.GetStatus();
    Base::Result<Base::Vector<TemplatePropertyTrigger>>
        triggers = CompilePropertyTriggers(
            controlTemplate,
            blueprint.Value(),
            runtime,
            properties);
    if (!triggers) return triggers.GetStatus();

    for (const Base::Ref<Base::Object>& authored :
         FrameworkTemplateState::AuthoredTriggers(controlTemplate)) {
        if (!authored) continue;
        if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
            authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
            (authored->RuntimeType() == Trigger::StaticTypeId() &&
             (!static_cast<const Trigger&>(*authored).
                    GetEnterActions().Empty() ||
              !static_cast<const Trigger&>(*authored).
                    GetExitActions().Empty()))) {
            Base::Ref<TriggerBase> retained =
                Base::Ref<TriggerBase>::TryFromBorrowed(
                    static_cast<TriggerBase&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate instance trigger cannot be retained");
            }
            Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                std::move(retained), blueprint.Value(), runtime, properties);
            if (!plan) return plan.GetStatus();
            blueprint.Value().controlTemplateDataTriggers.PushBack(
                std::move(plan).Value());
        } else if (runtime.Types().IsDerivedFrom(
                       authored->RuntimeType(),
                       EventTrigger::StaticTypeId())) {
            Base::Ref<EventTrigger> retained =
                Base::Ref<EventTrigger>::TryFromBorrowed(
                    static_cast<EventTrigger&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate EventTrigger cannot be retained");
            }
            Base::Result<void> added{};
            blueprint.Value().controlTemplateEventTriggers.PushBack(
                    std::move(retained));
        }
    }

    CompiledTemplateDefinition definition;
    definition.targetType =
        controlTemplate.GetTargetType();
    definition.blueprint =
        std::move(blueprint).Value();
    for (std::uint32_t nodeIndex = 0U;
         nodeIndex < definition.blueprint.nodes.Size(); ++nodeIndex) {
        TemplatePrototypeNode& node =
            definition.blueprint.nodes[nodeIndex];
        if (!runtime.Types().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId())) {
            continue;
        }
        const DependencyProperty* contentSourceProperty =
            properties.Find(
                ContentPresenter::
                    ContentSourceProperty.Handle());
        const DependencyProperty* contentProperty =
            properties.Find(
                ContentPresenter::
                    ContentProperty.Handle());
        const DependencyProperty* contentTemplateProperty =
            properties.Find(
                ContentPresenter::
                    ContentTemplateProperty.Handle());
        if (contentSourceProperty == nullptr ||
            contentProperty == nullptr) {
            return InvalidTemplateCompiler(
                "ContentPresenter content properties were not registered");
        }
        Base::StringView contentSource;
        bool hasContentProperty = false;
        bool hasContentTemplateProperty = false;
        for (const TemplatePrototypeProperty& property :
             node.properties) {
            if (property.property ==
                    contentSourceProperty->Handle() &&
                property.value.Kind() ==
                    ValueKind::String) {
                contentSource =
                    property.value.AsString();
            }
            if (property.property ==
                    contentProperty->Handle()) {
                hasContentProperty = true;
            }
            if (contentTemplateProperty != nullptr &&
                property.property == contentTemplateProperty->Handle()) {
                hasContentTemplateProperty = true;
            }
        }
        // Explicit Content= (including Content="{TemplateBinding Content}")
        // already wires the presenter. Do not add a second binding.
        if (hasContentProperty) {
            continue;
        }
        const bool defaultedContent = contentSource.Empty();
        if (defaultedContent) {
            // WPF ContentPresenter.ContentSource defaults to "Content".
            // Unnamed presenters in ListBoxItem/Button templates rely on that
            // implicit TemplateBinding to show DisplayMemberPath text.
            contentSource = Base::StringView("Content");
        }
        if (node.name.Empty()) {
            char generatedName[48];
            const int written = std::snprintf(
                generatedName, sizeof(generatedName),
                "__ContentSource%u", nodeIndex);
            if (written <= 0 || static_cast<std::size_t>(written) >=
                                    sizeof(generatedName)) {
                return InvalidTemplateCompiler(
                    "ContentPresenter generated template name is invalid");
            }
            Base::Result<void> named = node.name.Assign(
                Base::StringView(
                    generatedName,
                    static_cast<std::uint32_t>(written)));
            if (!named) return named.GetStatus();
        }
        const DependencyProperty* source =
            properties.Find(
                controlTemplate.GetTargetType(),
                contentSource);
        if (source == nullptr) {
            if (defaultedContent) {
                continue;
            }
            return MissingTemplateProperty(
                "ContentSource property",
                contentSource,
                controlTemplate.GetTargetType(),
                runtime.Types());
        }
        TemplateBindingPlan binding;
        Base::Result<void> assigned =
            binding.targetName.Assign(
                node.name.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        binding.sourceProperty =
            source->Handle();
        binding.targetProperty =
            contentProperty->Handle();
        definition.contentSourceBindings.
            PushBack(std::move(binding));

        if (!hasContentTemplateProperty && contentTemplateProperty != nullptr) {
            char templatePropName[64];
            std::snprintf(templatePropName, sizeof(templatePropName), "%.*sTemplate",
                static_cast<int>(contentSource.SizeBytes()), contentSource.Data());
            const DependencyProperty* sourceTemplate = properties.Find(
                controlTemplate.GetTargetType(), Base::StringView(templatePropName));
            if (sourceTemplate == nullptr && contentSource == Base::StringView("SelectedContent")) {
                sourceTemplate = properties.Find(controlTemplate.GetTargetType(), Base::StringView("ContentTemplate"));
            }
            if (sourceTemplate != nullptr) {
                TemplateBindingPlan tplBinding;
                if (tplBinding.targetName.Assign(node.name.View())) {
                    tplBinding.sourceProperty = sourceTemplate->Handle();
                    tplBinding.targetProperty = contentTemplateProperty->Handle();
                    definition.contentSourceBindings.PushBack(std::move(tplBinding));
                }
            }
        }
    }
    definition.propertyTriggers =
        std::move(triggers).Value();
    definition.visualStateGroups =
        std::move(groups).Value();
    EnsureBytecode(definition.blueprint);
    for (auto& node : definition.blueprint.nodes) {
        node.properties.Clear();
        node.gridColumns.Clear();
        node.gridRows.Clear();
        node.streamGeometryData.Clear();
        node.gradientStops.Clear();
        node.authoredBehaviors.Clear();
        node.authoredTriggers.Clear();
    }
    return definition;
}

// Compiles a derived ControlTemplate that inherits its visual tree from a
// sealed XAML-compiled base template (BasedOn without own VisualTree).
// Derived-authored property triggers and visual states resolve
// TargetName/SourceName against the BASE blueprint, i.e. the shared tree.
// Action-bearing Trigger/DataTrigger/EventTrigger declarations cannot be
// represented in the property-trigger table; they must live in a template
// that authors its own VisualTree.
Base::Result<void>
CompileInheritedControlTemplate(
    ControlTemplate& derivedTemplate,
    const CompiledTemplateBlueprint& baseBlueprint,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    for (const Base::Ref<Base::Object>& object :
        FrameworkTemplateState::AuthoredTriggers(derivedTemplate)) {
        if (!object) {
            continue;
        }
        const Meta::TypeId kind = object->RuntimeType();
        if (kind == EventTrigger::StaticTypeId() ||
            kind == DataTrigger::StaticTypeId() ||
            kind == MultiDataTrigger::StaticTypeId()) {
            return InvalidTemplateCompiler(
                "Inherited ControlTemplate authoring an EventTrigger or DataTrigger requires its own VisualTree");
        }
        if (kind == Trigger::StaticTypeId()) {
            const auto& source = static_cast<const Trigger&>(*object);
            if (!source.GetEnterActions().Empty() ||
                !source.GetExitActions().Empty()) {
                return InvalidTemplateCompiler(
                    "Inherited ControlTemplate Trigger with Enter/ExitActions requires its own VisualTree");
            }
        }
    }
    Base::Result<Base::Vector<TemplatePropertyTrigger>> triggers =
        CompilePropertyTriggers(
            derivedTemplate, baseBlueprint, runtime, properties);
    if (!triggers) {
        return triggers.GetStatus();
    }
    for (TemplatePropertyTrigger& trigger : triggers.Value()) {
        Base::Result<void> added =
            FrameworkTemplateState::AddPropertyTrigger(
                derivedTemplate, std::move(trigger));
        if (!added) {
            return added.GetStatus();
        }
    }
    Base::Result<Base::Vector<VisualStateGroupPlan>> groups =
        CompileVisualStates(
            derivedTemplate, baseBlueprint, runtime, properties);
    if (!groups) {
        return groups.GetStatus();
    }
    for (VisualStateGroupPlan& group : groups.Value()) {
        Base::Result<void> added =
            FrameworkTemplateState::AddVisualStateGroup(
                derivedTemplate, std::move(group));
        if (!added) {
            return added.GetStatus();
        }
    }
    return {};
}

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return CompileBlueprint(
        visualTree,
        names,
        edges,
        bindings,
        {},
        {},
        runtime,
        properties);
}



static Base::Result<void> AttachCompiledTemplateBindings(
    CompiledTemplateBlueprint& blueprint,
    const Base::Vector<Base::Ref<Base::Object>>& objects,
    BindingEngine& bindings,
    FrameworkElement* templatedParent,
    const Base::Ref<Base::Object>& payload) noexcept {
    for (const TemplatePrototypeBinding& binding : blueprint.bindings) {
        if (binding.metadata == nullptr ||
            binding.target >= objects.Size() ||
            (binding.source != UINT32_MAX &&
             binding.source >= objects.Size())) {
            return InvalidTemplateCompiler(
                "Compiled template Binding declaration is invalid");
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = binding.metadata;
        descriptor.source =
            binding.source != UINT32_MAX
            ? objects[binding.source].Get()
            : nullptr;
        if (descriptor.source == nullptr &&
            !binding.sourceName.Empty()) {
            if (templatedParent != nullptr) {
                descriptor.source = templatedParent->FindName(
                    binding.sourceName.View());
                if (descriptor.source == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "ControlTemplate Binding ElementName was not found in the outer NameScope");
                }
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "DataTemplate Binding cannot resolve an ElementName outside its template NameScope");
            }
        }
        if (descriptor.source == nullptr &&
            !binding.relativeAncestorType.Empty()) {
            descriptor.source = ResolveTemplateBindingAncestor(
                binding,
                *objects[binding.target],
                *blueprint.runtime);
            if (descriptor.source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Template Binding ancestor was not found");
            }
        }
        descriptor.target =
            static_cast<DependencyObject*>(
                objects[binding.target].Get());
        descriptor.targetProperty =
            binding.targetProperty;
        descriptor.dataContextProperty =
            binding.dataContextProperty;
        descriptor.dataContextOwner =
            descriptor.target != nullptr
            ? descriptor.target
            : (!objects.Empty() ? static_cast<DependencyObject*>(objects[0].Get()) : nullptr);
        descriptor.path = binding.path.View();
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.bindsToSource =
            binding.bindsToSource;
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        descriptor.converterResource = binding.converter;
        descriptor.converterParameter = binding.converterParameter;
        Base::Result<void> queued =
            bindings.QueueDeferred(descriptor);
        if (!queued) return queued.GetStatus();

        if (payload &&
            templatedParent == nullptr &&
            binding.source == UINT32_MAX &&
            binding.sourceName.Empty() &&
            binding.relativeAncestorType.Empty()) {
            Base::Result<void> activated =
                bindings.ActivateDeferredWhenReady(*descriptor.target);
            if (!activated) return activated.GetStatus();
        }
    }
    return {};
}

static Base::Result<void> AttachCompiledTemplateTriggers(
    CompiledTemplateBlueprint& blueprint,
    const Base::Vector<Base::Ref<Base::Object>>& objects,
    const Base::Vector<CompiledTriggerPlan>& triggers,
    const Base::Vector<Base::Ref<EventTrigger>>& eventTriggers,
    FrameworkElement* templatedParent,
    const Base::Ref<Base::Object>& payload,
    bool registerNamesInRoot) noexcept {
    if (triggers.Empty() && eventTriggers.Empty()) {
        return {};
    }
    if (objects.Empty() ||
        !blueprint.runtime->Types().IsDerivedFrom(
            objects[0U]->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        return InvalidTemplateCompiler(
            "Template root does not support runtime triggers");
    }
    Base::Result<Base::Ref<DataTemplateTriggerInstance>> created =
        Base::MakeRef<DataTemplateTriggerInstance>();
    if (!created) return created.GetStatus();
    Base::Ref<DataTemplateTriggerInstance> triggerContext =
        std::move(created).Value();
    auto* root = static_cast<FrameworkElement*>(objects[0U].Get());
    triggerContext->root = root;

    for (std::uint32_t index = 0U; index < objects.Size(); ++index) {
        if (blueprint.nodes[index].name.Empty()) continue;
        DataTemplateTriggerInstance::NamedObject named;
        Base::Result<void> namedAssigned = named.name.Assign(
            blueprint.nodes[index].name.View());
        if (!namedAssigned) return namedAssigned.GetStatus();
        named.object = Base::WeakRef<Base::Object>(
            Base::Ref<Base::Object>::FromBorrowed(*objects[index]));
        triggerContext->names.PushBack(std::move(named));
        if (registerNamesInRoot) {
            static_cast<void>(root->RegisterName(
                blueprint.nodes[index].name.View(),
                *objects[index]));
        }
    }

    auto sourceFor = [&](const Binding& binding) noexcept -> Base::Object* {
        if (!binding.GetElementName().Empty()) {
            return triggerContext->FindName(binding.GetElementName());
        }
        if (templatedParent != nullptr) {
            const Base::Ref<RelativeSource> relative =
                binding.GetRelativeSource();
            if (!relative) {
                return templatedParent;
            }
            if (relative->GetMode() == RelativeSourceMode::FindAncestor) {
                return WalkTemplateAncestor(
                    templatedParent,
                    relative->GetAncestorType(),
                    relative->GetAncestorLevel(),
                    blueprint.runtime->Types());
            }
            return templatedParent;
        }
        return payload.Get();
    };

    for (const CompiledTriggerPlan& plan : triggers) {
        const Base::Ref<TriggerBase>& authored = plan.trigger;
        if (!authored) continue;
        DataTemplatePropertyTrigger runtimeTrigger;
        if (authored->RuntimeType() == Trigger::StaticTypeId()) {
            const auto& property = static_cast<const Trigger&>(*authored);
            Base::Object* source = nullptr;
            if (templatedParent != nullptr) {
                source = property.GetSourceName().Empty()
                    ? static_cast<Base::Object*>(templatedParent)
                    : triggerContext->FindName(property.GetSourceName());
            } else {
                source = root;
            }
            if (source == nullptr ||
                !blueprint.runtime->Types().IsDerivedFrom(
                    source->RuntimeType(),
                    DependencyObject::StaticTypeId())) {
                return InvalidTemplateCompiler(
                    "Template action trigger source was not found");
            }
            const TypeId sourceType = source->RuntimeType();
            const DependencyProperty* sourceProperty =
                ResolveTemplateProperty(
                    *blueprint.properties,
                    sourceType,
                    property.GetPropertyName());
            if (sourceProperty == nullptr) {
                return MissingTemplateProperty(
                    "Template action trigger property",
                    property.GetPropertyName(), sourceType,
                    blueprint.runtime->Types());
            }
            DataTemplateTriggerCondition condition;
            condition.source = Base::WeakRef<Base::Object>(
                Base::Ref<Base::Object>::FromBorrowed(*source));
            condition.dependencySource =
                Base::WeakRef<DependencyObject>(
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(source)));
            condition.property = sourceProperty->Handle();
            Base::Result<Value> converted = ConvertTriggerValue(
                property.GetAuthoredValue(), sourceType, *sourceProperty,
                *blueprint.runtime, *blueprint.properties);
            if (!converted) return converted.GetStatus();
            condition.value = std::move(converted).Value();
            runtimeTrigger.conditions.PushBack(std::move(condition));
        } else if (authored->RuntimeType() == DataTrigger::StaticTypeId()) {
            const auto& data = static_cast<const DataTrigger&>(*authored);
            if (!data.GetBinding()) return InvalidTemplateCompiler(
                "Template DataTrigger requires Binding");
            DataTemplateTriggerCondition condition;
            Base::Object* source = sourceFor(*data.GetBinding());
            if (source != nullptr) {
                condition.source =
                    Base::WeakRef<Base::Object>(
                        Base::Ref<Base::Object>::FromBorrowed(*source));
            }
            condition.usesDataContext =
                data.GetBinding()->GetElementName().Empty() &&
                !data.GetBinding()->GetRelativeSource();
            condition.binding = data.GetBinding();
            condition.value = data.GetAuthoredValue();
            runtimeTrigger.conditions.PushBack(std::move(condition));
        } else if (authored->RuntimeType() == MultiDataTrigger::StaticTypeId()) {
            const auto& multi = static_cast<const MultiDataTrigger&>(*authored);
            for (const Base::Ref<Condition>& authoredCondition : multi.GetConditions()) {
                if (!authoredCondition || !authoredCondition->GetBinding()) {
                    return InvalidTemplateCompiler(
                        "Template MultiDataTrigger requires complete Conditions");
                }
                DataTemplateTriggerCondition condition;
                Base::Object* source = sourceFor(*authoredCondition->GetBinding());
                if (source != nullptr) {
                    condition.source =
                        Base::WeakRef<Base::Object>(
                            Base::Ref<Base::Object>::FromBorrowed(*source));
                }
                condition.usesDataContext =
                    authoredCondition->GetBinding()->GetElementName().Empty() &&
                    !authoredCondition->GetBinding()->GetRelativeSource();
                condition.binding = authoredCondition->GetBinding();
                condition.value = authoredCondition->GetAuthoredValue();
                runtimeTrigger.conditions.PushBack(std::move(condition));
            }
        } else {
            continue;
        }
        if (templatedParent == nullptr || authored->RuntimeType() != Trigger::StaticTypeId()) {
            for (const CompiledTriggerSetter& cs : plan.setters) {
                DataTemplateTriggerSetter runtimeSetter;
                runtimeSetter.target =
                    Base::WeakRef<DependencyObject>(
                        Base::Ref<DependencyObject>::FromBorrowed(
                            *static_cast<DependencyObject*>(objects[cs.target].Get())));
                runtimeSetter.property = cs.property;
                runtimeSetter.value = cs.value;
                runtimeTrigger.setters.PushBack(std::move(runtimeSetter));
            }
        }
        runtimeTrigger.enterActions.Append(authored->GetEnterActions());
        runtimeTrigger.exitActions.Append(authored->GetExitActions());
        triggerContext->triggers.PushBack(std::move(runtimeTrigger));
    }
    for (const Base::Ref<EventTrigger>& eventTrigger : eventTriggers) {
        if (!eventTrigger) continue;
        triggerContext->eventTriggers.PushBack(eventTrigger);
    }
    FrameworkElement* const triggerRoot = triggerContext->root;
    Base::Ref<Base::Object> triggerOwner(triggerContext);
    FrameworkElementSeams::AddAuthoredTrigger(*triggerRoot, std::move(triggerOwner));
    return {};
}

Base::Result<void> BuildCompiledTemplate(
    TemplateBuilder& context,
    void* factoryContext) noexcept {
    auto* blueprint =
        static_cast<CompiledTemplateBlueprint*>(
            factoryContext);
    if (blueprint == nullptr ||
        blueprint->runtime == nullptr ||
        blueprint->nodes.Empty()) {
        return InvalidTemplateCompiler(
            "Compiled template blueprint is invalid");
    }

    XamlBytecodeProgram& program = EnsureBytecode(*blueprint);
    if (!program.IsValid()) {
        return InvalidTemplateCompiler(
            "Compiled template bytecode is invalid");
    }

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> executed = XamlVM::Execute(
        program, *blueprint->runtime, &context, objects, nullptr);
    if (!executed) return executed.GetStatus();

    Base::Result<void> bound = AttachCompiledTemplateBindings(
        *blueprint, objects, context.Bindings(), &context.TemplatedParent(), {});
    if (!bound) return bound.GetStatus();

    return AttachCompiledTemplateTriggers(
        *blueprint, objects,
        blueprint->controlTemplateDataTriggers,
        blueprint->controlTemplateEventTriggers,
        &context.TemplatedParent(), {}, false);
}

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext,
    BindingEngine* bindings) noexcept {
    auto* blueprint =
        static_cast<CompiledTemplateBlueprint*>(
            factoryContext);
    if (blueprint == nullptr ||
        blueprint->runtime == nullptr ||
        blueprint->nodes.Empty()) {
        return InvalidTemplateCompiler(
            "Deferred template blueprint is invalid");
    }

    XamlBytecodeProgram& program = EnsureBytecode(*blueprint);
    if (!program.IsValid()) {
        return InvalidTemplateCompiler(
            "Deferred template bytecode is invalid");
    }

    if (bindings == nullptr && !blueprint->bindings.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DataTemplate Binding requires a mounted View binding engine");
    }

    XamlVMContext vmContext;
    vmContext.runtime = blueprint->runtime;
    vmContext.dataItemPayload = payload;
    vmContext.bindings = bindings;

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> executed = XamlVM::Execute(
        program, vmContext, objects, nullptr);
    if (!executed) return executed.GetStatus();

    Base::Ref<Base::Object> root = objects[0U];

    if (bindings != nullptr) {
        Base::Result<void> bound = AttachCompiledTemplateBindings(
            *blueprint, objects, *bindings, nullptr, payload);
        if (!bound) return bound.GetStatus();
    }

    static const Base::Vector<Base::Ref<EventTrigger>> noEventTriggers;
    Base::Result<void> triggered = AttachCompiledTemplateTriggers(
        *blueprint, objects,
        blueprint->dataTemplateTriggers,
        noEventTriggers, nullptr, payload, true);
    if (!triggered) return triggered.GetStatus();

    return root;
}

} // namespace Aero::Markup
