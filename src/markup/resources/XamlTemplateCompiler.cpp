#include "XamlTemplateCompiler.hpp"

#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/Style.hpp>

#include <utility>

namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Presentation;

Base::Status InvalidTemplateCompiler(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

struct PendingPrototypeNode final {
    Base::Ref<Base::Object> object;
    std::uint32_t parent = UINT32_MAX;
    ContentWriteCallback contentWrite = nullptr;
    void* contentContext = nullptr;
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

Base::Result<CompiledTemplateBlueprint>
CompileBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const NameScope* names,
    Base::Span<const XamlDeferredContentEdge> edges,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (!visualTree) {
        return InvalidTemplateCompiler(
            "Template requires a VisualTree");
    }

    CompiledTemplateBlueprint blueprint;
    blueprint.runtime = &runtime;

    Base::Vector<PendingPrototypeNode> pending;
    Base::Result<void> appended =
        pending.TryPushBack({
            visualTree,
            UINT32_MAX,
            nullptr,
            nullptr});
    if (!appended) return appended.GetStatus();

    for (std::uint32_t index = 0U;
         index < pending.Size(); ++index) {
        const PendingPrototypeNode& source =
            pending[index];
        Base::Object* object = source.object.Get();
        if (object == nullptr ||
            !runtime.Descriptors().IsDerivedFrom(
                object->RuntimeType(),
                Visual::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "ControlTemplate VisualTree contains a non-Visual object");
        }
        if (!runtime.Descriptors().IsDerivedFrom(
                object->RuntimeType(),
                DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "ControlTemplate visual does not support dependency properties");
        }

        TemplatePrototypeNode node;
        node.type = object->RuntimeType();
        node.parent = source.parent;
        node.contentWrite = source.contentWrite;
        node.contentContext = source.contentContext;
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
            appended = node.properties.TryPushBack({
                property.Handle(),
                local.Value()});
            if (!appended) {
                return appended.GetStatus();
            }
        }

        if (blueprint.contentPresenter == UINT32_MAX &&
            runtime.Descriptors().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId())) {
            blueprint.contentPresenter = index;
        }

        appended = blueprint.nodes.TryPushBack(
            std::move(node));
        if (!appended) return appended.GetStatus();

        for (const XamlDeferredContentEdge& edge :
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
                edge.write,
                edge.contentContext});
            if (!appended) {
                return appended.GetStatus();
            }
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
    if (value.Kind() == ValueKind::String &&
        value.Type() != property.ValueType()) {
        Base::Result<Value> converted =
            runtime.TryConvertText(
                property.ValueType(),
                value.AsString());
        if (!converted) {
            return converted.GetStatus();
        }
        value = std::move(converted).Value();
    } else if (value.IsNullObject() &&
        value.Type() != property.ValueType()) {
        value = Value::NullObject(
            property.ValueType());
    }

    const bool objectCompatible =
        value.Kind() == ValueKind::Object &&
        (value.IsNullObject() ||
         runtime.Descriptors().IsDerivedFrom(
             value.Type(),
             property.ValueType()));
    if (value.Type() != property.ValueType() &&
        !objectCompatible) {
        return InvalidTemplateCompiler(
            "Visual state Setter value type does not match its property");
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

Base::Result<Base::Vector<VisualStateGroup>>
CompileVisualStates(
    ControlTemplate& controlTemplate,
    const CompiledTemplateBlueprint& blueprint,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<VisualStateGroup> groups;
    for (const Base::Ref<Base::Object>& groupObject :
         controlTemplate.AuthoredVisualStateGroups()) {
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
                    properties.Find(
                        target->type,
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
        assigned = groups.TryPushBack(
            std::move(group));
        if (!assigned) return assigned.GetStatus();
    }
    return groups;
}

} // namespace

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    ControlTemplate& controlTemplate,
    Base::Span<const XamlDeferredContentEdge> edges,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Result<CompiledTemplateBlueprint> blueprint =
        CompileBlueprint(
            controlTemplate.AuthoredVisualTree(),
            &controlTemplate.AuthoredNames(),
            edges,
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

    CompiledTemplateDefinition definition;
    definition.targetType =
        controlTemplate.TargetType();
    definition.blueprint =
        std::move(blueprint).Value();
    definition.visualStateGroups =
        std::move(groups).Value();
    return definition;
}

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    Base::Span<const XamlDeferredContentEdge> edges,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    return CompileBlueprint(
        visualTree,
        nullptr,
        edges,
        runtime,
        properties);
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

    Base::Vector<Visual*> visuals;
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
        if (!owner ||
            owner->RuntimeType() != node.type ||
            !blueprint->runtime->Descriptors()
                 .IsDerivedFrom(
                     node.type,
                     Visual::StaticTypeId()) ||
            !blueprint->runtime->Descriptors()
                 .IsDerivedFrom(
                     node.type,
                     DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Compiled template factory created an incompatible object");
        }
        auto* visual =
            static_cast<Visual*>(owner.Get());
        auto* dependencyObject =
            static_cast<DependencyObject*>(
                owner.Get());
        for (const TemplatePrototypeProperty& property :
             node.properties) {
            Base::Result<void> applied =
                dependencyObject->SetValue(
                    property.property,
                    property.value);
            if (!applied) {
                return applied.GetStatus();
            }
        }

        if (node.parent != UINT32_MAX) {
            if (node.parent >= visuals.Size() ||
                node.contentWrite == nullptr) {
                return InvalidTemplateCompiler(
                    "Compiled template content edge is invalid");
            }
            Base::Result<void> content =
                node.contentWrite(
                    *visuals[node.parent],
                    owner,
                    node.contentContext);
            if (!content) return content.GetStatus();
        }

        Base::Result<void> added;
        if (node.parent == UINT32_MAX) {
            added = context.SetRoot(
                node.name.View(),
                std::move(owner),
                *visual);
        } else {
            if (node.parent >= visuals.Size()) {
                return InvalidTemplateCompiler(
                    "Compiled template node parent is invalid");
            }
            added = context.AddPart(
                node.name.View(),
                *visuals[node.parent],
                std::move(owner),
                *visual);
        }
        if (!added) return added.GetStatus();
        added = visuals.TryPushBack(visual);
        if (!added) return added.GetStatus();
    }

    if (blueprint->contentPresenter != UINT32_MAX) {
        if (blueprint->contentPresenter >=
                visuals.Size() ||
            !blueprint->runtime->Descriptors()
                 .IsDerivedFrom(
                     context.TemplatedParent()
                         .RuntimeType(),
                     ContentControl::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Compiled ContentPresenter requires a ContentControl target");
        }
        Base::Result<bool> projected =
            context.ProjectContent(
                static_cast<ContentControl&>(
                    context.TemplatedParent()),
                static_cast<ContentPresenter&>(
                    *visuals[
                        blueprint->
                            contentPresenter]));
        if (!projected) {
            return projected.GetStatus();
        }
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

    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const TemplatePrototypeNode& node =
            blueprint->nodes[index];
        Base::Result<Base::Ref<Base::Object>> created =
            blueprint->runtime->CreateObject(node.type);
        if (!created) return created.GetStatus();
        Base::Ref<Base::Object> owner =
            std::move(created).Value();
        if (!owner ||
            !blueprint->runtime->Descriptors()
                 .IsDerivedFrom(
                     node.type,
                     Visual::StaticTypeId()) ||
            !blueprint->runtime->Descriptors()
                 .IsDerivedFrom(
                     node.type,
                     DependencyObject::StaticTypeId())) {
            return InvalidTemplateCompiler(
                "Deferred template created an incompatible object");
        }
        auto& dependencyObject =
            static_cast<DependencyObject&>(
                *owner);
        for (const TemplatePrototypeProperty& property :
             node.properties) {
            Base::Result<void> applied =
                dependencyObject.SetValue(
                    property.property,
                    property.value);
            if (!applied) return applied.GetStatus();
        }
        if (node.parent != UINT32_MAX) {
            if (node.parent >= objects.Size() ||
                node.contentWrite == nullptr) {
                return InvalidTemplateCompiler(
                    "Deferred template content edge is invalid");
            }
            Base::Result<void> written =
                node.contentWrite(
                    *objects[node.parent],
                    owner,
                    node.contentContext);
            if (!written) return written.GetStatus();
        }
        Base::Result<void> added =
            objects.TryPushBack(std::move(owner));
        if (!added) return added.GetStatus();
    }

    Base::Ref<Base::Object> root = objects[0U];
    if (payload &&
        blueprint->runtime->Descriptors().IsDerivedFrom(
            root->RuntimeType(),
            FrameworkElement::StaticTypeId())) {
        Base::Result<void> assigned =
            static_cast<FrameworkElement&>(*root)
                .SetDataContext(payload);
        if (!assigned) return assigned.GetStatus();
    }
    return root;
}

} // namespace Aero::Markup::Detail
