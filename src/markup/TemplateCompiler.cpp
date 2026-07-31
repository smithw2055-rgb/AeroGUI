#include "TemplateCompiler.hpp"
#include "../runtime/DataTemplateTriggerContext.hpp"

#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Style.hpp>
#include <Aero/Rendering.hpp>

#include <cstdio>
#include <utility>
#include "../controls/RuntimeManagers.hpp"
#include "../ui/RuntimeManagers.hpp"

namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;
using namespace Aero::Controls;


Base::Status InvalidTemplateCompiler(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

struct PendingPrototypeNode final {
    Base::Ref<Base::Object> object;
    std::uint32_t parent = UINT32_MAX;
    MemberId contentMember = InvalidMemberId;
};

bool ContainsObject(
    const Base::Vector<PendingPrototypeNode>& pending,
    const Base::Object* object) noexcept {
    for (const PendingPrototypeNode& current :
         pending) {
        if (current.object.Get() == object) {
            return true;
        }
    }
    return false;
}

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

Base::Result<Value> ConvertTemplateTextValue(
    MetadataRuntime& runtime,
    const DependencyProperty& property,
    Base::StringView text) noexcept {
    // WPF uses -1 for no selection. Internally selection keeps UINT32_MAX;
    // translate only the standard selection properties at the XAML boundary.
    if ((property.Handle() == Selector::SelectedIndexProperty.Handle() ||
         property.Handle() == TabControl::SelectedIndexProperty.Handle()) &&
        text == Base::StringView("-1")) {
        return ValueCodec<std::uint32_t>::Encode(UINT32_MAX);
    }
    return runtime.TryConvertText(property.ValueType(), text);
}

const DependencyProperty* ResolveTemplateProperty(
    const DependencyPropertyRegistry& properties,
    TypeId targetType,
    Base::StringView name) noexcept {
    const DependencyProperty* property = properties.Find(targetType, name);
    if (property != nullptr) return property;
    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == '.') separator = index;
    }
    if (separator == UINT32_MAX || separator == 0U ||
        separator + 1U >= name.SizeBytes()) {
        return nullptr;
    }
    const Base::StringView ownerName = name.Substr(0U, separator);
    const Base::StringView memberName = name.Substr(
        separator + 1U, name.SizeBytes() - separator - 1U);
    const TypeInfo* owner = properties.Types().FindType(
        AeroNamespaceUri(), ownerName);
    if (owner != nullptr) {
        property = properties.Find(owner->Id(), memberName);
        if (property != nullptr) return property;
    }
    return properties.Find(targetType, memberName);
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

Base::Result<CompiledTemplateBlueprint>
CompileBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (!visualTree) {
        return InvalidTemplateCompiler(
            "Template requires a VisualTree");
    }

    CompiledTemplateBlueprint blueprint;
    blueprint.runtime = &runtime;
    blueprint.properties = &properties;

    Base::Vector<PendingPrototypeNode> pending;
    Base::Result<void> appended =
        pending.TryPushBack({
            visualTree,
            UINT32_MAX,
            InvalidMemberId});
    if (!appended) return appended.GetStatus();

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
        if ((index == 0U || source.parent != UINT32_MAX) &&
            !visual) {
            return InvalidTemplateCompiler(
                "Template VisualTree contains a non-Visual object");
        }

        TemplatePrototypeNode node;
        node.type = object->RuntimeType();
        node.parent = source.parent;
        node.contentMember = source.contentMember;
        const Base::StringView name = names != nullptr
            ? names->NameOf(*object)
            : Base::StringView{};
        Base::Result<void> named =
            node.name.TryAssign(name);
        if (!named) return named.GetStatus();

        auto& dependencyObject =
            static_cast<DependencyObject&>(*object);
        for (const DependencyProperty& property :
             properties.Properties()) {
            if (property.MetadataFor(node.type) == nullptr) {
                continue;
            }
            Base::Result<Value> local =
                dependencyObject.ReadLocalValue(
                    property.Handle());
            if (!local) return local.GetStatus();
            if (local.Value().IsUnset()) continue;
            TemplatePrototypeProperty prototypeProperty;
            prototypeProperty.property = property.Handle();
            prototypeProperty.value = local.Value();
            if (local.Value().Kind() == ValueKind::Object &&
                !local.Value().IsNullObject() &&
                local.Value().AsObject() &&
                runtime.Types().IsDerivedFrom(
                    local.Value().AsObject()->RuntimeType(),
                    DependencyObject::StaticTypeId()) &&
                RequiresPrototypeObject(
                    bindings, local.Value().AsObject().Get())) {
                prototypeProperty.objectNode =
                    FindPrototypeObject(
                        pending,
                        local.Value().AsObject().Get());
                if (prototypeProperty.objectNode == UINT32_MAX) {
                    prototypeProperty.objectNode = pending.Size();
                    appended = pending.TryPushBack({
                        local.Value().AsObject(),
                        UINT32_MAX,
                        InvalidMemberId});
                    if (!appended) return appended.GetStatus();
                }
            }
            appended = node.properties.TryPushBack(
                std::move(prototypeProperty));
            if (!appended) {
                return appended.GetStatus();
            }
        }

        if (runtime.Types().IsDerivedFrom(
                node.type,
                Grid::StaticTypeId())) {
            const auto& grid =
                static_cast<const Grid&>(*object);
            appended = node.gridColumns.TryAssign(
                grid.ColumnDefinitions());
            if (!appended) {
                return appended.GetStatus();
            }
            appended = node.gridRows.TryAssign(
                grid.RowDefinitions());
            if (!appended) {
                return appended.GetStatus();
            }
        }

        if (blueprint.contentPresenter == UINT32_MAX &&
            (runtime.Types().IsDerivedFrom(
                 node.type,
                 ContentPresenter::StaticTypeId()) ||
             runtime.Types().IsDerivedFrom(
                 node.type,
                 ScrollContentPresenter::
                     StaticTypeId()))) {
            blueprint.contentPresenter = index;
        }

        appended = blueprint.nodes.TryPushBack(
            std::move(node));
        if (!appended) return appended.GetStatus();

        for (const DeferredContentEdge& edge :
             edges) {
            if (edge.parent != object) continue;
            if (!edge.child ||
                ContainsObject(
                    pending, edge.child.Get())) {
                return InvalidTemplateCompiler(
                    "ControlTemplate visual content contains a cycle or duplicate");
            }
            appended = pending.TryPushBack({
                edge.child,
                index,
                edge.member});
            if (!appended) {
                return appended.GetStatus();
            }
        }
    }

    for (const DeferredBindingEdge& source :
         bindings) {
        const std::uint32_t target =
            FindPrototypeObject(
                pending, source.target);
        const std::uint32_t bindingSource =
            source.source != nullptr
            ? FindPrototypeObject(
                  pending, source.source)
            : UINT32_MAX;
        if (target == UINT32_MAX ||
            (source.source != nullptr &&
             bindingSource == UINT32_MAX) ||
            source.manager == nullptr ||
            source.metadata == nullptr) {
            return InvalidTemplateCompiler(
                "Deferred template Binding target or source is outside its VisualTree");
        }
        TemplatePrototypeBinding binding;
        binding.target = target;
        binding.source = bindingSource;
        binding.manager = source.manager;
        binding.metadata = source.metadata;
        binding.targetProperty =
            source.targetProperty;
        binding.dataContextProperty =
            source.dataContextProperty;
        binding.mode = source.mode;
        binding.updateSourceTrigger =
            source.updateSourceTrigger;
        Base::Result<void> assigned =
            binding.path.TryAssign(
                source.path.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = binding.stringFormat.TryAssign(
            source.stringFormat.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        assigned = blueprint.bindings.TryPushBack(
            std::move(binding));
        if (!assigned) {
            return assigned.GetStatus();
        }
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

Base::Result<Value> ConvertSetterValue(
    const Setter& setter,
    const TemplatePrototypeNode& target,
    const DependencyProperty& property,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Value value = setter.AuthoredValue();
    if (value.IsUnset()) {
        return InvalidTemplateCompiler(
            "Visual state Setter requires Value");
    }
    // Bindings in template setters are declarations. Their source is only
    // available after the template has an instance and a templated parent.
    if (value.Kind() == ValueKind::Object &&
        !value.IsNullObject() &&
        value.Type() == Data::Binding::StaticTypeId()) {
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
    } else if (value.IsNullObject() &&
        value.Type() != property.ValueType()) {
        value = Value::NullObject(
            property.ValueType());
    }
    if (property.ValueType() == Length::StaticTypeId() &&
        value.Type() == TypeOf<double>()) {
        Base::Result<double> numeric = ValueCodec<double>::Decode(value);
        if (!numeric) return numeric.GetStatus();
        Base::Result<Value> length = ValueCodec<Length>::Encode(
            Length::Pixels(numeric.Value()));
        if (!length) return length.GetStatus();
        value = std::move(length).Value();
    }
    if (property.ValueType() ==
            Brush::StaticTypeId() &&
        value.Type() ==
            TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            ValueCodec<Base::Color>::Decode(
                value);
        if (!color) return color.GetStatus();
        Base::Result<Base::Ref<Brush>> brush =
            MakeSolidColorBrush(color.Value());
        if (!brush) return brush.GetStatus();
        value = Value::FromObject(
            Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    // The legacy editor backend stores text colors as Color values, while
    // WPF themes expose Foreground/Caret/Selection values as brushes. Accept
    // the common SolidColorBrush case at the markup boundary without
    // changing the authored resource or template semantics.
    if (property.ValueType() == TypeOf<Base::Color>() &&
        value.Kind() == ValueKind::Object &&
        runtime.Types().IsDerivedFrom(
            value.Type(), Brush::StaticTypeId())) {
        Base::Result<Base::Ref<Brush>> decoded =
            ValueCodec<Base::Ref<Brush>>::Decode(value);
        if (!decoded) return decoded.GetStatus();
        Brush* brush = decoded.Value().Get();
        if (brush == nullptr || brush->RuntimeType() !=
                SolidColorBrush::StaticTypeId()) {
            return InvalidTemplateCompiler(
                "Color-backed text property requires SolidColorBrush");
        }
        Base::Result<Value> color = ValueCodec<Base::Color>::Encode(
            static_cast<SolidColorBrush*>(brush)->GetColor());
        if (!color) return color.GetStatus();
        value = std::move(color).Value();
    }

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
            "Setter '%.*s' expects '%.*s' but received '%.*s'",
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
            target.type,
            value);
    return valid
        ? Base::Result<Value>(value)
        : Base::Result<Value>(
              valid.GetStatus());
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
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (value.IsUnset()) {
        return InvalidTemplateCompiler(
            "Template Trigger requires Value");
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
    } else if (value.IsNullObject() &&
        value.Type() != property.ValueType()) {
        value = Value::NullObject(
            property.ValueType());
    }
    if (property.ValueType() == Length::StaticTypeId() &&
        value.Type() == TypeOf<double>()) {
        Base::Result<double> numeric = ValueCodec<double>::Decode(value);
        if (!numeric) return numeric.GetStatus();
        Base::Result<Value> length = ValueCodec<Length>::Encode(
            Length::Pixels(numeric.Value()));
        if (!length) return length.GetStatus();
        value = std::move(length).Value();
    }
    const bool objectCompatible =
        value.Kind() == ValueKind::Object &&
        (value.IsNullObject() ||
         runtime.Types().IsDerivedFrom(
             value.Type(),
             property.ValueType()));
    if (value.Type() != property.ValueType() &&
        !objectCompatible) {
        return InvalidTemplateCompiler(
            "Template Trigger value type does not match its property");
    }
    Base::Result<void> valid =
        properties.ValidateValueFor(
            property.Handle(), targetType, value);
    return valid
        ? Base::Result<Value>(value)
        : Base::Result<Value>(
              valid.GetStatus());
}

Base::Result<Base::Vector<TemplatePropertyTrigger>>
CompilePropertyTriggers(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    MetadataRuntime& runtime,
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
        TypeId targetType = controlTemplate.TargetType();
        if (!sourceName.Empty()) {
            const TemplatePrototypeNode* target = FindNode(blueprint, sourceName);
            if (target == nullptr) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger source was not found");
            }
            targetType = target->type;
        }
        // The runtime represents ToggleButton's nullable WPF IsChecked value
        // as a bool plus the read-only IsIndeterminate state. Preserve the
        // observable template condition `IsChecked == {x:Null}` by compiling
        // it to that state property instead of attempting to assign an object
        // null to a Boolean dependency property.
        if (authoredValue.IsNullObject() &&
            propertyName == Base::StringView("IsChecked") &&
            runtime.Types().IsDerivedFrom(
                targetType, ToggleButton::StaticTypeId())) {
            const DependencyProperty* indeterminate = properties.Find(
                targetType, "IsIndeterminate");
            if (indeterminate == nullptr) {
                return InvalidTemplateCompiler(
                    "ToggleButton IsIndeterminate property was not found");
            }
            TemplateTriggerCondition condition;
            Base::Result<void> assigned =
                condition.sourceName.TryAssign(sourceName);
            if (!assigned) return assigned.GetStatus();
            condition.property = indeterminate->Handle();
            condition.value = Value::FromBoolean(
                TypeOf<bool>(), true);
            return trigger.conditions.TryPushBack(std::move(condition));
        }
        const DependencyProperty* property =
            propertyName == Base::StringView(
                "local:Element.IsFocusEngaged")
            ? properties.Find(
                Aero::Element::
                    IsFocusEngagedProperty.Handle())
            : propertyName == Base::StringView(
                "local:Text.PasswordLength")
            ? properties.Find(
                Aero::TextProperties::
                    PasswordLengthProperty.Handle())
            : ResolveTemplateProperty(properties, targetType, propertyName);
        if (property == nullptr) {
            return MissingTemplateProperty(
                "Trigger property", propertyName, targetType, runtime.Types());
        }
        Base::Result<Value> value = ConvertTriggerValue(
            authoredValue, targetType, *property, runtime, properties);
        if (!value) return value.GetStatus();
        TemplateTriggerCondition condition;
        Base::Result<void> assigned =
            condition.sourceName.TryAssign(sourceName);
        if (!assigned) return assigned.GetStatus();
        condition.property = property->Handle();
        condition.value = std::move(value).Value();
        return trigger.conditions.TryPushBack(std::move(condition));
    };
    auto appendSetters = [&controlTemplate, &blueprint, &runtime, &properties](
        Base::Span<const Base::Ref<Setter>> setters,
        TemplatePropertyTrigger& trigger) -> Base::Result<void> {
        for (const Base::Ref<Setter>& setterObject : setters) {
            if (!setterObject || setterObject->PropertyName().Empty()) {
                return InvalidTemplateCompiler(
                    "ControlTemplate Trigger Setter requires Property");
            }
            const Base::StringView targetName = setterObject->TargetName();
            TypeId targetType = controlTemplate.TargetType();
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
                    if (controlTemplate.AuthoredNames().Find(
                            targetName) != nullptr) {
                        continue;
                    }
                    return InvalidTemplateCompiler(
                        "ControlTemplate Trigger Setter target was not found");
                }
                targetType = target->type;
            }
            const DependencyProperty* property = ResolveTemplateProperty(
                properties, targetType, setterObject->PropertyName());
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
            Base::Result<void> assigned = setter.targetName.TryAssign(targetName);
            if (!assigned) return assigned.GetStatus();
            setter.property = property->Handle();
            setter.value = std::move(value).Value();
            assigned = trigger.setters.TryPushBack(std::move(setter));
            if (!assigned) return assigned.GetStatus();
        }
        return {};
    };
    for (const Base::Ref<Base::Object>& object :
         controlTemplate.AuthoredTriggers()) {
        TemplatePropertyTrigger trigger;
        Base::Result<void> configured;
        if (object && object->RuntimeType() == PropertyTrigger::StaticTypeId()) {
            const auto& source = static_cast<const PropertyTrigger&>(*object);
            configured = addCondition(source.PropertyName(), source.SourceName(),
                source.AuthoredValue(), trigger);
            if (configured) configured = appendSetters(source.AuthoredSetters(), trigger);
        } else if (object && object->RuntimeType() == MultiTrigger::StaticTypeId()) {
            const auto& source = static_cast<const MultiTrigger&>(*object);
            for (const Base::Ref<Condition>& condition : source.Conditions()) {
                if (!condition) return InvalidTemplateCompiler(
                    "MultiTrigger contains a null Condition");
                configured = addCondition(condition->PropertyName(), condition->SourceName(),
                    condition->AuthoredValue(), trigger);
                if (!configured) return configured.GetStatus();
            }
            configured = appendSetters(source.AuthoredSetters(), trigger);
        } else {
            // Data and event triggers are compiled into the instance runtime
            // plan below. They cannot be represented by the dependency-
            // property trigger table used by TemplateManager.
            continue;
        }
        if (!configured) return configured.GetStatus();
        Base::Result<void> added =
            compiled.TryPushBack(
                std::move(trigger));
        if (!added) return added.GetStatus();
    }
    return compiled;
}

Base::Result<Base::Vector<VisualStateGroup>>
CompileVisualStates(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<VisualStateGroup> groups;
    auto compileGroup = [&groups, &blueprint, &runtime, &properties](
        const Base::Ref<Base::Object>& groupObject)
        -> Base::Result<void> {
        if (!groupObject ||
            groupObject->RuntimeType() !=
                XamlVisualStateGroupObject::
                    StaticTypeId()) {
            return InvalidTemplateCompiler(
                "ControlTemplate VisualStateGroups contains an invalid object");
        }
        const auto& sourceGroup =
            static_cast<
                const XamlVisualStateGroupObject&>(
                    *groupObject);
        if (sourceGroup.Name().Empty()) {
            return InvalidTemplateCompiler(
                "VisualStateGroup requires Name");
        }
        VisualStateGroup group;
        Base::Result<void> assigned =
            group.name.TryAssign(
                sourceGroup.Name());
        if (!assigned) return assigned.GetStatus();

        for (const Base::Ref<Base::Object>& stateObject :
             sourceGroup.States()) {
            if (!stateObject ||
                stateObject->RuntimeType() !=
                    XamlVisualStateObject::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualState");
            }
            const auto& sourceState =
                static_cast<
                    const XamlVisualStateObject&>(
                        *stateObject);
            if (sourceState.Name().Empty()) {
                return InvalidTemplateCompiler(
                    "VisualState requires Name");
            }
            VisualState state;
            assigned = state.name.TryAssign(
                sourceState.Name());
            if (!assigned) {
                return assigned.GetStatus();
            }
            state.storyboard =
                sourceState.StoryboardValue();

            for (const Base::Ref<Base::Object>&
                     setterObject :
                 sourceState.Setters()) {
                if (!setterObject ||
                    setterObject->RuntimeType() !=
                        Setter::StaticTypeId()) {
                    return InvalidTemplateCompiler(
                        "VisualState contains an invalid Setter");
                }
                const auto& sourceSetter =
                    static_cast<const Setter&>(
                        *setterObject);
                if (sourceSetter.TargetName().Empty() ||
                    sourceSetter.PropertyName().Empty()) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter requires TargetName and Property");
                }
                const TemplatePrototypeNode* target =
                    FindNode(
                        blueprint,
                        sourceSetter.TargetName());
                if (target == nullptr) {
                    return InvalidTemplateCompiler(
                        "Visual state Setter target was not found");
                }
                const DependencyProperty* property =
                    ResolveTemplateProperty(
                        properties, target->type,
                        sourceSetter.PropertyName());
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

                VisualStateSetter setter;
                assigned =
                    setter.targetName.TryAssign(
                        sourceSetter.TargetName());
                if (!assigned) {
                    return assigned.GetStatus();
                }
                setter.property = property->Handle();
                setter.value =
                    std::move(value).Value();
                assigned =
                    state.setters.TryPushBack(
                        std::move(setter));
                if (!assigned) {
                    return assigned.GetStatus();
                }
            }
            assigned =
                group.states.TryPushBack(
                    std::move(state));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        for (const Base::Ref<Base::Object>&
                 transitionObject :
             sourceGroup.Transitions()) {
            if (!transitionObject ||
                transitionObject->RuntimeType() !=
                    XamlVisualTransitionObject::
                        StaticTypeId()) {
                return InvalidTemplateCompiler(
                    "VisualStateGroup contains an invalid VisualTransition");
            }
            const auto& sourceTransition =
                static_cast<
                    const XamlVisualTransitionObject&>(
                        *transitionObject);
            if (sourceTransition.From().Empty() &&
                sourceTransition.To().Empty() &&
                sourceTransition.GeneratedDuration().Empty() &&
                !sourceTransition.StoryboardValue()) {
                return InvalidTemplateCompiler(
                    "VisualTransition must specify a duration or Storyboard");
            }
            if (!sourceTransition.From().Empty()) {
                bool found = false;
                for (const VisualState& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.From();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition From state was not found");
                }
            }
            if (!sourceTransition.To().Empty()) {
                bool found = false;
                for (const VisualState& state :
                     group.states) {
                    found = found ||
                        state.name.View() ==
                            sourceTransition.To();
                }
                if (!found) {
                    return InvalidTemplateCompiler(
                        "VisualTransition To state was not found");
                }
            }

            VisualTransition transition;
            assigned = transition.from.TryAssign(
                sourceTransition.From());
            if (assigned) {
                assigned = transition.to.TryAssign(
                    sourceTransition.To());
            }
            if (!assigned) return assigned.GetStatus();
            if (!sourceTransition.GeneratedDuration().Empty()) {
                Media::Animation::Storyboard duration;
                assigned = duration.SetDuration(
                    sourceTransition.GeneratedDuration());
                if (!assigned) return assigned.GetStatus();
                transition.generatedDurationMicroseconds =
                    duration.Timing().durationMicroseconds;
            }
            transition.generatedEasingFunction =
                sourceTransition.GeneratedEasingFunction();
            transition.storyboard =
                sourceTransition.StoryboardValue();
            assigned = group.transitions.TryPushBack(
                std::move(transition));
            if (!assigned) return assigned.GetStatus();
        }
        assigned = groups.TryPushBack(
            std::move(group));
        if (!assigned) return assigned.GetStatus();
        return {};
    };
    for (const Base::Ref<Base::Object>& groupObject :
         controlTemplate.AuthoredVisualStateGroups()) {
        Base::Result<void> compiled = compileGroup(groupObject);
        if (!compiled) return compiled.GetStatus();
    }
    Base::Ref<Base::Object> authoredRoot =
        controlTemplate.AuthoredVisualTree();
    if (authoredRoot &&
        runtime.Types().IsDerivedFrom(
            authoredRoot->RuntimeType(),
            Core::DependencyObject::StaticTypeId())) {
        auto& root = static_cast<Core::DependencyObject&>(*authoredRoot);
        Base::Ref<Base::Object> valueStore = root.GetValueOr(
            XamlVisualStateManagerObject::
                VisualStateGroupStoreProperty,
            Base::Ref<Base::Object>{});
        if (valueStore && valueStore->RuntimeType() ==
                XamlVisualStateGroupStore::StaticTypeId()) {
            for (const Base::Ref<Base::Object>& groupObject :
                 static_cast<XamlVisualStateGroupStore&>(
                     *valueStore).Groups()) {
                Base::Result<void> compiled = compileGroup(groupObject);
                if (!compiled) return compiled.GetStatus();
            }
        }
    }
    return groups;
}

} // namespace

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<CompiledTemplateBlueprint> blueprint =
        CompileBlueprint(
        controlTemplate.AuthoredVisualTree(),
        &controlTemplate.AuthoredNames(),
        edges,
        bindings,
        runtime,
            properties);
    if (!blueprint) {
        return blueprint.GetStatus();
    }
    Base::Result<Base::Vector<VisualStateGroup>>
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
         controlTemplate.AuthoredTriggers()) {
        if (!authored) continue;
        if (authored->RuntimeType() == DataTrigger::StaticTypeId() ||
            authored->RuntimeType() == MultiDataTrigger::StaticTypeId() ||
            (authored->RuntimeType() == PropertyTrigger::StaticTypeId() &&
             (!static_cast<const PropertyTrigger&>(*authored).
                    EnterActions().Empty() ||
              !static_cast<const PropertyTrigger&>(*authored).
                    ExitActions().Empty()))) {
            Base::Ref<TriggerBase> retained =
                Base::Ref<TriggerBase>::TryFromBorrowed(
                    static_cast<TriggerBase&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate instance trigger cannot be retained");
            }
            Base::Result<void> added =
                blueprint.Value().controlTemplateDataTriggers.TryPushBack(
                    std::move(retained));
            if (!added) return added.GetStatus();
        } else if (authored->RuntimeType() ==
                   Media::Animation::EventTrigger::StaticTypeId()) {
            Base::Ref<Media::Animation::EventTrigger> retained =
                Base::Ref<Media::Animation::EventTrigger>::TryFromBorrowed(
                    static_cast<Media::Animation::EventTrigger&>(*authored));
            if (!retained) {
                return InvalidTemplateCompiler(
                    "ControlTemplate EventTrigger cannot be retained");
            }
            Base::Result<void> added =
                blueprint.Value().controlTemplateEventTriggers.TryPushBack(
                    std::move(retained));
            if (!added) return added.GetStatus();
        }
    }

    CompiledTemplateDefinition definition;
    definition.targetType =
        controlTemplate.TargetType();
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
        if (contentSourceProperty == nullptr ||
            contentProperty == nullptr) {
            return InvalidTemplateCompiler(
                "ContentPresenter content properties were not registered");
        }
        Base::StringView contentSource;
        for (const TemplatePrototypeProperty& property :
             node.properties) {
            if (property.property ==
                    contentSourceProperty->Handle() &&
                property.value.Kind() ==
                    ValueKind::String) {
                contentSource =
                    property.value.AsString();
                break;
            }
        }
        if (contentSource.Empty()) continue;
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
            Base::Result<void> named = node.name.TryAssign(
                Base::StringView(
                    generatedName,
                    static_cast<std::uint32_t>(written)));
            if (!named) return named.GetStatus();
        }
        const DependencyProperty* source =
            properties.Find(
                controlTemplate.TargetType(),
                contentSource);
        if (source == nullptr) {
            return MissingTemplateProperty(
                "ContentSource property",
                contentSource,
                controlTemplate.TargetType(),
                runtime.Types());
        }
        TemplateBindingPlan binding;
        Base::Result<void> assigned =
            binding.targetName.TryAssign(
                node.name.View());
        if (!assigned) {
            return assigned.GetStatus();
        }
        binding.sourceProperty =
            source->Handle();
        binding.targetProperty =
            contentProperty->Handle();
        assigned =
            definition.contentSourceBindings.
                TryPushBack(std::move(binding));
        if (!assigned) {
            return assigned.GetStatus();
        }
    }
    definition.propertyTriggers =
        std::move(triggers).Value();
    definition.visualStateGroups =
        std::move(groups).Value();
    return definition;
}

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return CompileBlueprint(
        visualTree,
        names,
        edges,
        bindings,
        runtime,
        properties);
}

std::uint32_t FindNodeIndex(
    const CompiledTemplateBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (std::uint32_t index = 0U;
         index < blueprint.nodes.Size();
         ++index) {
        if (blueprint.nodes[index].name.View() ==
                name) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<bool> DeferredTriggerValuesMatch(
    const Value& actual,
    Value expected,
    MetadataRuntime& runtime) noexcept {
    if (actual.Kind() == ValueKind::Object &&
        !actual.IsNullObject() &&
        actual.AsObject() &&
        actual.AsObject()->RuntimeType() ==
            BoxedItemValue::StaticTypeId()) {
        return DeferredTriggerValuesMatch(
            static_cast<const BoxedItemValue&>(
                *actual.AsObject()).Value(),
            std::move(expected),
            runtime);
    }
    if (expected.Kind() == ValueKind::String &&
        expected.Type() != actual.Type()) {
        Base::Result<Value> converted =
            runtime.TryConvertText(
                actual.Type(),
                expected.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        expected = std::move(converted).Value();
    }
    return actual == expected;
}

Base::Result<Value> ReadDeferredTriggerBinding(
    const Data::Binding& binding,
    const Base::Ref<Base::Object>& payload,
    MetadataRuntime& runtime) noexcept {
    if (!payload || binding.Path().Empty()) {
        return InvalidTemplateCompiler(
            "DataTemplate trigger Binding requires a data item and Path");
    }
    if (!binding.ElementName().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DataTemplate trigger ElementName source is outside the item scope");
    }
    Base::Result<BindingPathPlan> compiled =
        BindingPathPlan::Compile(
            runtime,
            payload->RuntimeType(),
            binding.Path());
    if (!compiled) return compiled.GetStatus();
    return compiled.Value().Get(
        runtime, *payload);
}

Base::Result<void> ApplyDeferredTriggerSetters(
    const CompiledTemplateBlueprint& blueprint,
    Base::Span<const Base::Ref<Setter>> setters,
    Base::Vector<Base::Ref<Base::Object>>& objects) noexcept {
    if (blueprint.runtime == nullptr ||
        blueprint.properties == nullptr) {
        return InvalidTemplateCompiler(
            "DataTemplate trigger services are unavailable");
    }
    for (const Base::Ref<Setter>& setter :
         setters) {
        if (!setter || !setter->IsAuthored()) {
            return InvalidTemplateCompiler(
                "DataTemplate trigger Setter is incomplete");
        }
        const std::uint32_t target =
            setter->TargetName().Empty()
            ? 0U
            : FindNodeIndex(
                  blueprint,
                  setter->TargetName());
        if (target >= objects.Size()) {
            return InvalidTemplateCompiler(
                "DataTemplate trigger Setter target was not found");
        }
        auto* dependencyObject =
            static_cast<DependencyObject*>(
                objects[target].Get());
        const DependencyProperty* property =
            blueprint.properties->Find(
                objects[target]->RuntimeType(),
                setter->PropertyName());
        if (property == nullptr) {
            return InvalidTemplateCompiler(
                "DataTemplate trigger Setter property was not found");
        }
        Base::Result<Value> converted =
            ConvertSetterValue(
                *setter,
                blueprint.nodes[target],
                *property,
                *blueprint.runtime,
                *blueprint.properties);
        if (!converted) return converted.GetStatus();
        Base::Result<void> applied =
            dependencyObject->SetValue(
                property->Handle(),
                std::move(converted).Value());
        if (!applied) return applied.GetStatus();
    }
    return {};
}

Base::Result<void> ApplyInitialDataTemplateTriggers(
    const CompiledTemplateBlueprint& blueprint,
    const Base::Ref<Base::Object>& payload,
    Base::Vector<Base::Ref<Base::Object>>& objects) noexcept {
    if (blueprint.dataTemplateTriggers.Empty()) {
        return {};
    }
    if (blueprint.runtime == nullptr) {
        return InvalidTemplateCompiler(
            "DataTemplate trigger metadata is unavailable");
    }
    for (const Base::Ref<TriggerBase>& trigger :
         blueprint.dataTemplateTriggers) {
        if (!trigger) continue;
        if (trigger->RuntimeType() ==
                DataTrigger::StaticTypeId()) {
            const auto& dataTrigger =
                static_cast<const DataTrigger&>(
                    *trigger);
            if (!dataTrigger.Binding()) {
                return InvalidTemplateCompiler(
                    "DataTrigger requires Binding");
            }
            Base::Result<Value> current =
                ReadDeferredTriggerBinding(
                    *dataTrigger.Binding(),
                    payload,
                    *blueprint.runtime);
            if (!current) {
                if (current.GetStatus().code ==
                        Base::ErrorCode::NotFound) {
                    continue;
                }
                return current.GetStatus();
            }
            Base::Result<bool> matches =
                DeferredTriggerValuesMatch(
                    current.Value(),
                    dataTrigger.AuthoredValue(),
                    *blueprint.runtime);
            if (!matches) return matches.GetStatus();
            if (matches.Value()) {
                Base::Result<void> applied =
                    ApplyDeferredTriggerSetters(
                        blueprint,
                        dataTrigger.AuthoredSetters(),
                        objects);
                if (!applied) return applied.GetStatus();
            }
        } else if (trigger->RuntimeType() ==
                   MultiDataTrigger::StaticTypeId()) {
            const auto& multi =
                static_cast<const MultiDataTrigger&>(
                    *trigger);
            bool active = true;
            for (const Base::Ref<Condition>& condition :
                 multi.Conditions()) {
                if (!condition ||
                    !condition->Binding()) {
                    return InvalidTemplateCompiler(
                        "MultiDataTrigger requires complete Conditions");
                }
                Base::Result<Value> current =
                    ReadDeferredTriggerBinding(
                        *condition->Binding(),
                        payload,
                        *blueprint.runtime);
                if (!current &&
                    current.GetStatus().code ==
                        Base::ErrorCode::NotFound) {
                    active = false;
                    break;
                }
                if (!current) {
                    return current.GetStatus();
                }
                Base::Result<bool> matches =
                    DeferredTriggerValuesMatch(
                        current.Value(),
                        condition->AuthoredValue(),
                        *blueprint.runtime);
                if (!matches) {
                    return matches.GetStatus();
                }
                if (!matches.Value()) {
                    active = false;
                    break;
                }
            }
            if (active) {
                Base::Result<void> applied =
                    ApplyDeferredTriggerSetters(
                        blueprint,
                        multi.AuthoredSetters(),
                        objects);
                if (!applied) return applied.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void> BuildCompiledTemplate(
    TemplateBuildContext& context,
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

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> reserved =
        objects.TryReserve(blueprint->nodes.Size());
    if (!reserved) return reserved.GetStatus();
    Base::Vector<Visual*> visuals;
    reserved = visuals.TryReserve(blueprint->nodes.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node =
            blueprint->nodes[index];
        Base::Result<Base::Ref<Base::Object>> created =
            blueprint->runtime->CreateObject(
                node.type);
        if (!created) return created.GetStatus();
        Base::Ref<Base::Object> owner =
            std::move(created).Value();
        if (!owner || owner->RuntimeType() != node.type ||
            !blueprint->runtime->Types().IsDerivedFrom(
                node.type, DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Compiled template factory created an incompatible object");
        }
        Visual* visual = blueprint->runtime->Types().IsDerivedFrom(
            node.type, Visual::StaticTypeId())
            ? static_cast<Visual*>(owner.Get()) : nullptr;
        Base::Result<void> added = objects.TryPushBack(std::move(owner));
        if (!added) return added.GetStatus();
        added = visuals.TryPushBack(visual);
        if (!added) return added.GetStatus();
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        auto& dependencyObject = static_cast<DependencyObject&>(*objects[index]);
        for (const TemplatePrototypeProperty& property : node.properties) {
            Value value = property.value;
            if (property.objectNode != UINT32_MAX) {
                if (property.objectNode >= objects.Size()) {
                    return InvalidTemplateCompiler(
                        "Compiled template property object is invalid");
                }
                value = Value::FromObject(
                    property.value.Type(), objects[property.objectNode]);
            }
            Base::Result<void> applied = dependencyObject.SetValue(
                property.property, std::move(value));
            if (!applied) return applied.GetStatus();
        }
        if (node.type == Grid::StaticTypeId()) {
            auto& grid = static_cast<Grid&>(*objects[index]);
            Base::Result<void> restored = grid.SetColumnDefinitions(
                node.gridColumns.AsSpan());
            if (restored) restored = grid.SetRowDefinitions(node.gridRows.AsSpan());
            if (!restored) return restored.GetStatus();
        }
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        if (node.parent != UINT32_MAX) {
            if (node.parent >= objects.Size() || visuals[index] == nullptr ||
                visuals[node.parent] == nullptr ||
                node.contentMember == InvalidMemberId) {
                return InvalidTemplateCompiler(
                    "Compiled template content edge is invalid");
            }
            const PropertyInfo* structuralProperty = blueprint->runtime->Types()
                .FindProperty(node.contentMember);
            const bool propertyEdge = structuralProperty != nullptr &&
                (static_cast<std::uint32_t>(structuralProperty->Flags()) &
                 static_cast<std::uint32_t>(PropertyFlags::Structural)) != 0U &&
                blueprint->runtime->CanWriteProperty(node.contentMember);
            Base::Result<void> content = propertyEdge
                ? blueprint->runtime->SetProperty(
                    *objects[node.parent], node.contentMember,
                    Value::FromObject(structuralProperty->ValueType(), objects[index]))
                : blueprint->runtime->WriteContent(
                    *objects[node.parent], node.contentMember, objects[index]);
            if (!content) return content.GetStatus();
        }
        if (visuals[index] == nullptr) continue;
        Base::Result<void> added = node.parent == UINT32_MAX
            ? context.SetRoot(node.name.View(),
                Base::Ref<Base::Object>::FromBorrowed(*objects[index]), *visuals[index])
            : context.AddPart(node.name.View(), *visuals[node.parent],
                Base::Ref<Base::Object>::FromBorrowed(*objects[index]), *visuals[index]);
        if (!added) return added.GetStatus();
    }

    if (blueprint->contentPresenter != UINT32_MAX) {
        if (blueprint->contentPresenter >=
                visuals.Size()) {
            return InvalidTemplateCompiler(
                "Compiled ContentPresenter index is invalid");
        }
        if (blueprint->runtime->Types()
                .IsDerivedFrom(
                    context.TemplatedParent()
                        .RuntimeType(),
                    ContentControl::StaticTypeId())) {
            Visual& contentHost =
                *visuals[
                    blueprint->contentPresenter];
            Base::Result<bool> projected =
                blueprint->runtime->Types().
                    IsDerivedFrom(
                        contentHost.RuntimeType(),
                        ContentPresenter::
                            StaticTypeId())
                ? context.ProjectContent(
                      static_cast<ContentControl&>(
                          context.
                              TemplatedParent()),
                      static_cast<ContentPresenter&>(
                          contentHost))
                : context.ProjectContent(
                      static_cast<ContentControl&>(
                          context.
                              TemplatedParent()),
                      static_cast<ContentControl&>(
                          contentHost));
            if (!projected) {
                return projected.GetStatus();
            }
        }
    }
    for (const TemplatePrototypeBinding& binding :
         blueprint->bindings) {
        if (binding.manager == nullptr ||
            binding.metadata == nullptr ||
            binding.target >= visuals.Size() ||
            (binding.source != UINT32_MAX &&
             binding.source >= visuals.Size())) {
            return InvalidTemplateCompiler(
                "Compiled template Binding declaration is invalid");
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = binding.metadata;
        descriptor.source =
            binding.source != UINT32_MAX
            ? objects[binding.source].Get()
            : nullptr;
        descriptor.target =
            static_cast<DependencyObject*>(
                objects[binding.target].Get());
        descriptor.targetProperty =
            binding.targetProperty;
        descriptor.dataContextProperty =
            binding.dataContextProperty;
        descriptor.path = binding.path.View();
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        Base::Result<void> queued =
            binding.manager->QueueDeferred(
                descriptor);
        if (!queued) return queued.GetStatus();
    }

    if (!blueprint->controlTemplateDataTriggers.Empty()) {
        if (!blueprint->runtime->Types().IsDerivedFrom(
                visuals[0U]->RuntimeType(),
                FrameworkElement::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "ControlTemplate root does not support runtime triggers");
        }
        Base::Result<Base::Ref<
            Aero::Detail::DataTemplateTriggerContext>> created =
            Base::MakeRef<Aero::Detail::DataTemplateTriggerContext>();
        if (!created) return created.GetStatus();
        Base::Ref<Aero::Detail::DataTemplateTriggerContext> triggerContext =
            std::move(created).Value();
        triggerContext->root =
            static_cast<FrameworkElement*>(visuals[0U]);
        for (std::uint32_t index = 0U; index < visuals.Size(); ++index) {
            if (blueprint->nodes[index].name.Empty()) continue;
            Aero::Detail::DataTemplateTriggerContext::NamedObject named;
            Base::Result<void> namedAssigned = named.name.TryAssign(
                blueprint->nodes[index].name.View());
            if (!namedAssigned) return namedAssigned.GetStatus();
            named.object = Base::Ref<Base::Object>::FromBorrowed(
                *static_cast<Base::Object*>(visuals[index]));
            namedAssigned = triggerContext->names.TryPushBack(
                std::move(named));
            if (!namedAssigned) return namedAssigned.GetStatus();
        }
        auto appendSetters =
            [&](Base::Span<const Base::Ref<Setter>> setters,
                Aero::Detail::DataTemplatePropertyTrigger& runtimeTrigger)
                noexcept -> Base::Result<void> {
            for (const Base::Ref<Setter>& setter : setters) {
                if (!setter) continue;
                const std::uint32_t target = setter->TargetName().Empty()
                    ? 0U : FindNodeIndex(*blueprint, setter->TargetName());
                if (target >= visuals.Size()) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate data trigger Setter target was not found");
                }
                const DependencyProperty* property = ResolveTemplateProperty(
                    *blueprint->properties, visuals[target]->RuntimeType(),
                    setter->PropertyName());
                if (property == nullptr) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate data trigger Setter property was not found");
                }
                Base::Result<Value> value = ConvertSetterValue(
                    *setter, blueprint->nodes[target], *property,
                    *blueprint->runtime, *blueprint->properties);
                if (!value) return value.GetStatus();
                Aero::Detail::DataTemplateTriggerSetter runtimeSetter;
                runtimeSetter.target =
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(visuals[target]));
                runtimeSetter.property = property->Handle();
                runtimeSetter.value = std::move(value).Value();
                Base::Result<void> added = runtimeTrigger.setters.TryPushBack(
                    std::move(runtimeSetter));
                if (!added) return added.GetStatus();
            }
            return {};
        };
        auto sourceFor = [&](const Data::Binding& binding) noexcept
            -> Base::Object* {
            if (!binding.ElementName().Empty()) {
                return triggerContext->FindName(binding.ElementName());
            }
            // A control-template binding's Self, TemplatedParent, and
            // ancestor-at-the-template-boundary all resolve to the templated
            // control before the visual subtree is mounted.
            return static_cast<Base::Object*>(&context.TemplatedParent());
        };
        for (const Base::Ref<TriggerBase>& authored :
             blueprint->controlTemplateDataTriggers) {
            if (!authored) continue;
            Aero::Detail::DataTemplatePropertyTrigger runtimeTrigger;
            Base::Span<const Base::Ref<Setter>> setters;
            if (authored->RuntimeType() == PropertyTrigger::StaticTypeId()) {
                const auto& property =
                    static_cast<const PropertyTrigger&>(*authored);
                Base::Object* source = property.SourceName().Empty()
                    ? static_cast<Base::Object*>(&context.TemplatedParent())
                    : triggerContext->FindName(property.SourceName());
                if (source == nullptr ||
                    !blueprint->runtime->Types().IsDerivedFrom(
                        source->RuntimeType(),
                        DependencyObject::StaticTypeId())) {
                    return InvalidTemplateCompiler(
                        "ControlTemplate action trigger source was not found");
                }
                const TypeId sourceType = source->RuntimeType();
                const DependencyProperty* sourceProperty =
                    property.PropertyName() == Base::StringView(
                        "local:Element.IsFocusEngaged")
                    ? blueprint->properties->Find(
                        Aero::Element::
                            IsFocusEngagedProperty.Handle())
                    : blueprint->properties->Find(
                        sourceType, property.PropertyName());
                if (sourceProperty == nullptr) {
                    return MissingTemplateProperty(
                        "ControlTemplate action trigger property",
                        property.PropertyName(), sourceType,
                        blueprint->runtime->Types());
                }
                Aero::Detail::DataTemplateTriggerCondition condition;
                condition.source = Base::Ref<Base::Object>::FromBorrowed(
                    *source);
                condition.dependencySource =
                    Base::Ref<DependencyObject>::FromBorrowed(
                        *static_cast<DependencyObject*>(source));
                condition.property = sourceProperty->Handle();
                Base::Result<Value> converted = ConvertTriggerValue(
                    property.AuthoredValue(), sourceType, *sourceProperty,
                    *blueprint->runtime, *blueprint->properties);
                if (!converted) return converted.GetStatus();
                condition.value = std::move(converted).Value();
                Base::Result<void> added = runtimeTrigger.conditions.TryPushBack(
                    std::move(condition));
                if (!added) return added.GetStatus();
                // Property-trigger setters continue to be owned by
                // TemplateManager, which preserves their trigger precedence.
                // This per-instance plan supplies only the action lifecycle.
            } else if (authored->RuntimeType() == DataTrigger::StaticTypeId()) {
                const auto& data = static_cast<const DataTrigger&>(*authored);
                if (!data.Binding()) return InvalidTemplateCompiler(
                    "ControlTemplate DataTrigger requires Binding");
                Aero::Detail::DataTemplateTriggerCondition condition;
                Base::Object* source = sourceFor(*data.Binding());
                if (source != nullptr) {
                    condition.source =
                        Base::Ref<Base::Object>::FromBorrowed(*source);
                }
                condition.binding = data.Binding();
                condition.value = data.AuthoredValue();
                Base::Result<void> added = runtimeTrigger.conditions.TryPushBack(
                    std::move(condition));
                if (!added) return added.GetStatus();
                setters = data.AuthoredSetters();
            } else if (authored->RuntimeType() ==
                       MultiDataTrigger::StaticTypeId()) {
                const auto& multi = static_cast<const MultiDataTrigger&>(*authored);
                for (const Base::Ref<Condition>& authoredCondition :
                     multi.Conditions()) {
                    if (!authoredCondition || !authoredCondition->Binding()) {
                        return InvalidTemplateCompiler(
                            "ControlTemplate MultiDataTrigger requires complete Conditions");
                    }
                    Aero::Detail::DataTemplateTriggerCondition condition;
                    Base::Object* source = sourceFor(*authoredCondition->Binding());
                    if (source != nullptr) {
                        condition.source =
                            Base::Ref<Base::Object>::FromBorrowed(*source);
                    }
                    condition.binding = authoredCondition->Binding();
                    condition.value = authoredCondition->AuthoredValue();
                    Base::Result<void> added =
                        runtimeTrigger.conditions.TryPushBack(std::move(condition));
                    if (!added) return added.GetStatus();
                }
                setters = multi.AuthoredSetters();
            } else {
                continue;
            }
            Base::Result<void> configured = appendSetters(setters, runtimeTrigger);
            if (configured) {
                configured = runtimeTrigger.enterActions.TryAppend(
                    authored->EnterActions());
            }
            if (configured) {
                configured = runtimeTrigger.exitActions.TryAppend(
                    authored->ExitActions());
            }
            if (configured) {
                configured = triggerContext->triggers.TryPushBack(
                    std::move(runtimeTrigger));
            }
            if (!configured) return configured.GetStatus();
        }
        Base::Result<void> attached =
            triggerContext->root->TryAddAuthoredTrigger(
                Base::Ref<Base::Object>(std::move(triggerContext)));
        if (!attached) return attached.GetStatus();
    }
    return {};
}

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext) noexcept {
    auto* blueprint =
        static_cast<CompiledTemplateBlueprint*>(
            factoryContext);
    if (blueprint == nullptr ||
        blueprint->runtime == nullptr ||
        blueprint->nodes.Empty()) {
        return InvalidTemplateCompiler(
            "Deferred template blueprint is invalid");
    }

    Base::Vector<Base::Ref<Base::Object>> objects;
    Base::Result<void> reserved =
        objects.TryReserve(blueprint->nodes.Size());
    if (!reserved) return reserved.GetStatus();

    for (const TemplatePrototypeNode& node : blueprint->nodes) {
        Base::Result<Base::Ref<Base::Object>> created =
            blueprint->runtime->CreateObject(node.type);
        if (!created) return created.GetStatus();
        Base::Ref<Base::Object> owner = std::move(created).Value();
        if (!owner || !blueprint->runtime->Types().IsDerivedFrom(
                          node.type, DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Deferred template created an incompatible object");
        }
        Base::Result<void> added = objects.TryPushBack(std::move(owner));
        if (!added) return added.GetStatus();
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        auto& dependencyObject =
            static_cast<DependencyObject&>(*objects[index]);
        for (const TemplatePrototypeProperty& property : node.properties) {
            Value value = property.value;
            if (property.objectNode != UINT32_MAX) {
                if (property.objectNode >= objects.Size()) {
                    return InvalidTemplateCompiler(
                        "Deferred template property object is invalid");
                }
                value = Value::FromObject(
                    property.value.Type(), objects[property.objectNode]);
            }
            Base::Result<void> applied = dependencyObject.SetValue(
                property.property, std::move(value));
            if (!applied) return applied.GetStatus();
        }
        if (node.type == Grid::StaticTypeId()) {
            auto& grid = static_cast<Grid&>(*objects[index]);
            Base::Result<void> restored = grid.SetColumnDefinitions(
                node.gridColumns.AsSpan());
            if (restored) restored = grid.SetRowDefinitions(
                node.gridRows.AsSpan());
            if (!restored) return restored.GetStatus();
        }
    }
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node = blueprint->nodes[index];
        if (node.parent == UINT32_MAX) continue;
        if (node.parent >= objects.Size() ||
            node.contentMember == InvalidMemberId) {
            return InvalidTemplateCompiler(
                "Deferred template content edge is invalid");
        }
        const PropertyInfo* structuralProperty = blueprint->runtime->Types()
            .FindProperty(node.contentMember);
        const bool propertyEdge = structuralProperty != nullptr &&
            (static_cast<std::uint32_t>(structuralProperty->Flags()) &
             static_cast<std::uint32_t>(PropertyFlags::Structural)) != 0U &&
            blueprint->runtime->CanWriteProperty(node.contentMember);
        Base::Result<void> written = propertyEdge
            ? blueprint->runtime->SetProperty(
                *objects[node.parent], node.contentMember,
                Value::FromObject(structuralProperty->ValueType(), objects[index]))
            : blueprint->runtime->WriteContent(
                *objects[node.parent], node.contentMember, objects[index]);
        if (!written) return written.GetStatus();
    }

    Base::Ref<Base::Object> root = objects[0U];
    if (payload &&
        blueprint->runtime->Types().IsDerivedFrom(
            root->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        Base::Result<void> assigned =
            static_cast<FrameworkElement&>(*root)
                .SetDataContext(payload);
        if (!assigned) return assigned.GetStatus();
    }
    for (const TemplatePrototypeBinding& binding :
         blueprint->bindings) {
        if (binding.manager == nullptr ||
            binding.metadata == nullptr ||
            binding.target >= objects.Size() ||
            (binding.source != UINT32_MAX &&
             binding.source >= objects.Size())) {
            return InvalidTemplateCompiler(
                "Deferred template Binding declaration is invalid");
        }
        MetadataBindingDescriptor descriptor;
        descriptor.metadata = binding.metadata;
        descriptor.source =
            binding.source != UINT32_MAX
            ? objects[binding.source].Get()
            : nullptr;
        descriptor.target =
            static_cast<DependencyObject*>(
                objects[binding.target].Get());
        descriptor.targetProperty =
            binding.targetProperty;
        descriptor.dataContextProperty =
            binding.dataContextProperty;
        descriptor.path = binding.path.View();
        descriptor.stringFormat =
            binding.stringFormat.View();
        descriptor.mode = binding.mode;
        descriptor.updateSourceTrigger =
            binding.updateSourceTrigger;
        Base::Result<void> queued =
            binding.manager->QueueDeferred(
                descriptor);
        if (!queued) return queued.GetStatus();
    }
    Base::Ref<Aero::Detail::DataTemplateTriggerContext>
        triggerContext;
    auto ensureTriggerContext =
        [&]() noexcept
        -> Base::Result<
            Aero::Detail::DataTemplateTriggerContext*> {
        if (triggerContext) {
            return triggerContext.Get();
        }
        Base::Result<Base::Ref<
            Aero::Detail::DataTemplateTriggerContext>>
            created = Base::MakeRef<
                Aero::Detail::
                    DataTemplateTriggerContext>();
        if (!created) {
            return created.GetStatus();
        }
        triggerContext = std::move(created).Value();
        triggerContext->root =
            static_cast<FrameworkElement*>(root.Get());
        for (std::uint32_t index = 0U;
             index < objects.Size();
             ++index) {
            if (blueprint->nodes[index].name.Empty()) {
                continue;
            }
            Aero::Detail::DataTemplateTriggerContext::
                NamedObject named;
            Base::Result<void> assigned =
                named.name.TryAssign(
                    blueprint->nodes[index].name.View());
            if (!assigned) {
                return assigned.GetStatus();
            }
            named.object = objects[index];
            assigned =
                triggerContext->names.TryPushBack(
                    std::move(named));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        return triggerContext.Get();
    };
    auto appendRuntimeSetters =
        [&](Base::Span<const Base::Ref<Setter>> setters,
            Aero::Detail::DataTemplatePropertyTrigger&
                runtimeTrigger) noexcept
        -> Base::Result<void> {
        for (const Base::Ref<Setter>& setter : setters) {
            if (!setter) continue;
            const std::uint32_t targetIndex =
                setter->TargetName().Empty()
                ? 0U
                : FindNodeIndex(
                      *blueprint,
                      setter->TargetName());
            if (targetIndex >= objects.Size()) {
                return InvalidTemplateCompiler(
                    "DataTemplate Trigger Setter target was not found");
            }
            const DependencyProperty* targetProperty =
                blueprint->properties->Find(
                    objects[targetIndex]->RuntimeType(),
                    setter->PropertyName());
            if (targetProperty == nullptr) {
                return InvalidTemplateCompiler(
                    "DataTemplate Trigger Setter property was not found");
            }
            Base::Result<Value> converted =
                ConvertSetterValue(
                    *setter,
                    blueprint->nodes[targetIndex],
                    *targetProperty,
                    *blueprint->runtime,
                    *blueprint->properties);
            if (!converted) {
                return converted.GetStatus();
            }
            Aero::Detail::DataTemplateTriggerSetter
                runtimeSetter;
            runtimeSetter.target =
                Base::Ref<DependencyObject>::FromBorrowed(
                    *static_cast<DependencyObject*>(
                        objects[targetIndex].Get()));
            runtimeSetter.property =
                targetProperty->Handle();
            runtimeSetter.value =
                std::move(converted).Value();
            Base::Result<void> added =
                runtimeTrigger.setters.TryPushBack(
                    std::move(runtimeSetter));
            if (!added) return added.GetStatus();
        }
        return {};
    };
    for (const Base::Ref<TriggerBase>& authored :
         blueprint->dataTemplateTriggers) {
        if (!authored) continue;
        const Core::TypeId triggerType =
            authored->RuntimeType();
        if (triggerType !=
                PropertyTrigger::StaticTypeId() &&
            triggerType != DataTrigger::StaticTypeId() &&
            triggerType !=
                MultiDataTrigger::StaticTypeId()) {
            continue;
        }
        Base::Result<
            Aero::Detail::DataTemplateTriggerContext*>
            ensured = ensureTriggerContext();
        if (!ensured) return ensured.GetStatus();
        Aero::Detail::DataTemplatePropertyTrigger
            runtimeTrigger;
        Base::Span<const Base::Ref<Setter>>
            authoredSetters;
        if (triggerType ==
            PropertyTrigger::StaticTypeId()) {
            const auto& propertyTrigger =
                static_cast<const PropertyTrigger&>(
                    *authored);
            const DependencyProperty* sourceProperty =
                blueprint->properties->Find(
                    root->RuntimeType(),
                    propertyTrigger.PropertyName());
            if (sourceProperty == nullptr) {
                return InvalidTemplateCompiler(
                    "DataTemplate Trigger source property was not found");
            }
            Aero::Detail::DataTemplateTriggerCondition
                condition;
            condition.source = root;
            condition.dependencySource =
                Base::Ref<DependencyObject>::FromBorrowed(
                    *static_cast<DependencyObject*>(
                        root.Get()));
            condition.property =
                sourceProperty->Handle();
            condition.value =
                propertyTrigger.AuthoredValue();
            if (condition.value.Kind() ==
                    ValueKind::String &&
                condition.value.Type() !=
                    sourceProperty->ValueType()) {
                Base::Result<Value> converted =
                    ConvertTemplateTextValue(
                        *blueprint->runtime,
                        *sourceProperty,
                        condition.value.AsString());
                if (!converted) {
                    return converted.GetStatus();
                }
                condition.value =
                    std::move(converted).Value();
            }
            Base::Result<void> added =
                runtimeTrigger.conditions.TryPushBack(
                    std::move(condition));
            if (!added) return added.GetStatus();
            authoredSetters =
                propertyTrigger.AuthoredSetters();
        } else if (triggerType ==
                   DataTrigger::StaticTypeId()) {
            const auto& dataTrigger =
                static_cast<const DataTrigger&>(
                    *authored);
            if (!dataTrigger.Binding()) {
                return InvalidTemplateCompiler(
                    "DataTemplate DataTrigger requires Binding");
            }
            Aero::Detail::DataTemplateTriggerCondition
                condition;
            condition.source = payload;
            condition.binding =
                dataTrigger.Binding();
            condition.value =
                dataTrigger.AuthoredValue();
            Base::Result<void> added =
                runtimeTrigger.conditions.TryPushBack(
                    std::move(condition));
            if (!added) return added.GetStatus();
            authoredSetters =
                dataTrigger.AuthoredSetters();
        } else {
            const auto& multi =
                static_cast<const MultiDataTrigger&>(
                    *authored);
            for (const Base::Ref<Condition>& authoredCondition :
                 multi.Conditions()) {
                if (!authoredCondition ||
                    !authoredCondition->Binding()) {
                    return InvalidTemplateCompiler(
                        "DataTemplate MultiDataTrigger requires complete Conditions");
                }
                Aero::Detail::DataTemplateTriggerCondition
                    condition;
                condition.source = payload;
                condition.binding =
                    authoredCondition->Binding();
                condition.value =
                    authoredCondition->AuthoredValue();
                Base::Result<void> added =
                    runtimeTrigger.conditions.TryPushBack(
                        std::move(condition));
                if (!added) return added.GetStatus();
            }
            authoredSetters =
                multi.AuthoredSetters();
        }
        Base::Result<void> retained =
            appendRuntimeSetters(
                authoredSetters, runtimeTrigger);
        if (!retained) return retained.GetStatus();
        retained = runtimeTrigger.enterActions.TryAppend(
                authored->EnterActions());
        if (retained) {
            retained =
                runtimeTrigger.exitActions.TryAppend(
                    authored->ExitActions());
        }
        if (retained) {
            retained =
                triggerContext->triggers.TryPushBack(
                    std::move(runtimeTrigger));
        }
        if (!retained) return retained.GetStatus();
    }
    if (triggerContext) {
        Base::Result<void> attached =
            static_cast<FrameworkElement&>(*root).
                TryAddAuthoredTrigger(
                    Base::Ref<Base::Object>(
                        triggerContext));
        if (!attached) return attached.GetStatus();
    }
    return root;
}

} // namespace Aero::Markup::Detail
