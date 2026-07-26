#include "XamlThemeObjectModel.hpp"

#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <utility>

namespace Aero::Markup::Detail {
namespace {

using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Presentation;

Base::Status InvalidTheme(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

struct PrototypeEdge final {
    Base::Object* parent = nullptr;
    Base::Ref<Base::Object> child;
};

class ThemeSchemaLoader final {
public:
    ThemeSchemaLoader(
        MetadataRuntime& runtime,
        DependencyPropertyRegistry& properties) noexcept
        : runtime_(&runtime), properties_(&properties) {}

    Base::Result<void> Register(XamlSchemaContext& schema) noexcept {
        schema_ = &schema;
        Base::Result<void> registered = schema.TryRegisterMemberProvider({
            &HandlesVisualContent,
            &WriteVisualContent,
            this,
            XamlMemberWriteMode::SetOnce,
            false});
        if (!registered) return registered.GetStatus();

        const MetadataPropertyDescriptor* setterValue =
            schema.Descriptors().FindProperty(
                ThemeSetterObject::StaticTypeId(),
                Base::StringView("Value"),
                false);
        if (setterValue == nullptr) {
            return InvalidTheme("Theme Setter.Value metadata is missing");
        }
        registered = schema.TryRegisterMemberAdapter({
            setterValue->Id(),
            XamlMemberWriteMode::SetOnce,
            &SetSetterValue,
            this,
            nullptr,
            true});
        if (!registered) return registered.GetStatus();

        return schema.TryRegisterTypeAdapter({
            ThemeControlTemplateObject::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            this,
            true,
            false,
            &RegisterTemplateName,
            nullptr});
    }

    Base::Span<const PrototypeEdge> Edges() const noexcept {
        return {edges_.Data(), edges_.Size()};
    }

private:
    MetadataRuntime* runtime_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    XamlSchemaContext* schema_ = nullptr;
    Base::Vector<PrototypeEdge> edges_;

    static bool HandlesVisualContent(
        const XamlResolvedMember& member,
        void* context) noexcept {
        auto* loader = static_cast<ThemeSchemaLoader*>(context);
        if (loader == nullptr || loader->schema_ == nullptr ||
            member.kind != MemberKind::Property) {
            return false;
        }
        const ContentFacet* content =
            loader->schema_->Facets().FindContentByMember(member.id);
        return content != nullptr && content->write != nullptr &&
            HasContentFlag(content->flags, ContentFlags::Visual);
    }

    static Base::Result<void> WriteVisualContent(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept {
        auto* loader = static_cast<ThemeSchemaLoader*>(context);
        if (loader == nullptr || loader->schema_ == nullptr ||
            services.targetObject != &object ||
            value.Kind() != ValueKind::Object || value.IsNullObject() ||
            !value.AsObject()) {
            return InvalidTheme("Theme visual content is invalid");
        }
        const ContentFacet* content =
            loader->schema_->Facets().FindContentByMember(
                services.targetMember);
        if (content == nullptr || content->write == nullptr ||
            !HasContentFlag(content->flags, ContentFlags::Visual)) {
            return InvalidTheme("Theme visual content facet is missing");
        }

        PrototypeEdge edge;
        edge.parent = &object;
        edge.child = value.AsObject();
        Base::Result<void> tracked = loader->edges_.TryPushBack(
            std::move(edge));
        if (!tracked) return tracked.GetStatus();

        Base::Result<void> written = content->write(
            object,
            value.AsObject(),
            content->context);
        if (!written) {
            loader->edges_.PopBack();
            return written.GetStatus();
        }
        return {};
    }

    static Base::Result<void> SetSetterValue(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept {
        if (context == nullptr ||
            object.RuntimeType() != ThemeSetterObject::StaticTypeId()) {
            return InvalidTheme("Theme Setter.Value target is invalid");
        }
        return static_cast<ThemeSetterObject&>(object).SetValue(value);
    }

    static Base::Result<void> RegisterTemplateName(
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object,
        void* context) noexcept {
        if (context == nullptr || scopeOwner.RuntimeType() !=
                ThemeControlTemplateObject::StaticTypeId()) {
            return InvalidTheme("Theme template name scope is invalid");
        }
        return static_cast<ThemeControlTemplateObject&>(scopeOwner)
            .RegisterName(name, object);
    }
};

Base::Result<XamlLoadResult> LoadDocument(
    Base::StringView source,
    XamlSchemaContext& schema,
    const ResourceDictionary* resources = nullptr) noexcept {
    Utf8XmlTokenizer tokenizer;
    Base::Result<void> reset = tokenizer.Reset(source);
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer);
    XamlObjectWriter writer(schema);
    if (resources == nullptr) {
        return writer.LoadDocument(reader);
    }
    XamlLoadContext context;
    context.resources = resources;
    return writer.LoadDocument(reader, context);
}

template<class T>
Base::Result<T*> RequireRoot(
    const XamlLoadResult& document,
    Core::TypeId expectedType,
    const char* message) noexcept {
    if (!document.root || document.root->RuntimeType() != expectedType) {
        return InvalidTheme(message);
    }
    return static_cast<T*>(document.root.Get());
}

struct PendingPrototypeNode final {
    Base::Ref<Base::Object> object;
    std::uint32_t parent = UINT32_MAX;
};

bool ContainsObject(
    const Base::Vector<PendingPrototypeNode>& pending,
    const Base::Object* object) noexcept {
    for (const PendingPrototypeNode& current : pending) {
        if (current.object.Get() == object) return true;
    }
    return false;
}

Base::Result<ThemeTemplateBlueprint> CompileBlueprint(
    ThemeControlTemplateObject& controlTemplate,
    Base::Span<const PrototypeEdge> edges,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    if (!controlTemplate.VisualTree()) {
        return InvalidTheme("ControlTemplate requires a VisualTree");
    }

    ThemeTemplateBlueprint blueprint;
    blueprint.runtime = &runtime;

    Base::Vector<PendingPrototypeNode> pending;
    Base::Result<void> appended = pending.TryPushBack({
        controlTemplate.VisualTree(),
        UINT32_MAX});
    if (!appended) return appended.GetStatus();

    for (std::uint32_t index = 0U; index < pending.Size(); ++index) {
        const PendingPrototypeNode& source = pending[index];
        Base::Object* object = source.object.Get();
        if (object == nullptr || !runtime.Descriptors().IsDerivedFrom(
                object->RuntimeType(), Visual::StaticTypeId())) {
            return InvalidTheme(
                "ControlTemplate VisualTree contains a non-Visual object");
        }
        if (!runtime.Descriptors().IsDerivedFrom(
                object->RuntimeType(), DependencyObject::StaticTypeId())) {
            return InvalidTheme(
                "ControlTemplate visual does not support dependency properties");
        }

        ThemePrototypeNode node;
        node.type = object->RuntimeType();
        node.parent = source.parent;
        const Base::StringView name = controlTemplate.Names().NameOf(*object);
        if (source.parent != UINT32_MAX && name.Empty()) {
            return InvalidTheme(
                "Non-root ControlTemplate visuals require x:Name");
        }
        Base::Result<void> named = node.name.TryAssign(name);
        if (!named) return named.GetStatus();

        auto& dependencyObject = static_cast<DependencyObject&>(*object);
        for (const DependencyProperty& property : properties.Properties()) {
            if (property.MetadataFor(node.type) == nullptr) continue;
            Base::Result<Value> local = dependencyObject.ReadLocalValue(
                property.Handle());
            if (!local) return local.GetStatus();
            if (local.Value().IsUnset()) continue;
            appended = node.properties.TryPushBack({
                property.Handle(),
                local.Value()});
            if (!appended) return appended.GetStatus();
        }

        if (blueprint.contentPresenter == UINT32_MAX &&
            runtime.Descriptors().IsDerivedFrom(
                node.type,
                ContentPresenter::StaticTypeId())) {
            blueprint.contentPresenter = index;
        }

        appended = blueprint.nodes.TryPushBack(std::move(node));
        if (!appended) return appended.GetStatus();

        for (const PrototypeEdge& edge : edges) {
            if (edge.parent != object) continue;
            if (!edge.child || ContainsObject(pending, edge.child.Get())) {
                return InvalidTheme(
                    "ControlTemplate visual content contains a cycle or duplicate");
            }
            appended = pending.TryPushBack({edge.child, index});
            if (!appended) return appended.GetStatus();
        }
    }

    return blueprint;
}

const ThemePrototypeNode* FindNode(
    const ThemeTemplateBlueprint& blueprint,
    Base::StringView name) noexcept {
    for (const ThemePrototypeNode& node : blueprint.nodes) {
        if (node.name.View() == name) return &node;
    }
    return nullptr;
}

Base::Result<Value> ConvertSetterValue(
    const ThemeSetterObject& setter,
    const ThemePrototypeNode& target,
    const DependencyProperty& property,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Value value = setter.Value();
    if (value.IsUnset()) {
        return InvalidTheme("Visual state Setter requires Value");
    }
    if (value.Kind() == ValueKind::String &&
        value.Type() != property.ValueType()) {
        Base::Result<Value> converted = runtime.TryConvertText(
            property.ValueType(),
            value.AsString());
        if (!converted) return converted.GetStatus();
        value = std::move(converted).Value();
    } else if (value.IsNullObject() &&
        value.Type() != property.ValueType()) {
        value = Value::NullObject(property.ValueType());
    }

    const bool objectCompatible = value.Kind() == ValueKind::Object &&
        (value.IsNullObject() || runtime.Descriptors().IsDerivedFrom(
            value.Type(), property.ValueType()));
    if (value.Type() != property.ValueType() && !objectCompatible) {
        return InvalidTheme(
            "Visual state Setter value type does not match its property");
    }
    Base::Result<void> valid = properties.ValidateValueFor(
        property.Handle(),
        target.type,
        value);
    if (!valid) return valid.GetStatus();
    return value;
}

Base::Result<Base::Vector<VisualStateGroup>> CompileVisualStates(
    ThemeControlTemplateObject& controlTemplate,
    const ThemeTemplateBlueprint& blueprint,
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties) noexcept {
    Base::Vector<VisualStateGroup> groups;
    for (const Base::Ref<Base::Object>& groupObject :
         controlTemplate.VisualStateGroups()) {
        if (!groupObject || groupObject->RuntimeType() !=
                ThemeVisualStateGroupObject::StaticTypeId()) {
            return InvalidTheme(
                "ControlTemplate VisualStateGroups contains an invalid object");
        }
        const auto& sourceGroup =
            static_cast<const ThemeVisualStateGroupObject&>(*groupObject);
        if (sourceGroup.Name().Empty()) {
            return InvalidTheme("VisualStateGroup requires Name");
        }
        VisualStateGroup group;
        Base::Result<void> assigned = group.name.TryAssign(
            sourceGroup.Name());
        if (!assigned) return assigned.GetStatus();

        for (const Base::Ref<Base::Object>& stateObject :
             sourceGroup.States()) {
            if (!stateObject || stateObject->RuntimeType() !=
                    ThemeVisualStateObject::StaticTypeId()) {
                return InvalidTheme(
                    "VisualStateGroup contains an invalid VisualState");
            }
            const auto& sourceState =
                static_cast<const ThemeVisualStateObject&>(*stateObject);
            if (sourceState.Name().Empty()) {
                return InvalidTheme("VisualState requires Name");
            }
            VisualState state;
            assigned = state.name.TryAssign(sourceState.Name());
            if (!assigned) return assigned.GetStatus();

            for (const Base::Ref<Base::Object>& setterObject :
                 sourceState.Setters()) {
                if (!setterObject || setterObject->RuntimeType() !=
                        ThemeSetterObject::StaticTypeId()) {
                    return InvalidTheme(
                        "VisualState contains an invalid Setter");
                }
                const auto& sourceSetter =
                    static_cast<const ThemeSetterObject&>(*setterObject);
                if (sourceSetter.TargetName().Empty() ||
                    sourceSetter.Property().Empty()) {
                    return InvalidTheme(
                        "Visual state Setter requires TargetName and Property");
                }
                const ThemePrototypeNode* target = FindNode(
                    blueprint,
                    sourceSetter.TargetName());
                if (target == nullptr) {
                    return InvalidTheme(
                        "Visual state Setter target was not found");
                }
                const DependencyProperty* property = properties.Find(
                    target->type,
                    sourceSetter.Property());
                if (property == nullptr) {
                    return InvalidTheme(
                        "Visual state Setter property was not found");
                }
                Base::Result<Value> value = ConvertSetterValue(
                    sourceSetter,
                    *target,
                    *property,
                    runtime,
                    properties);
                if (!value) return value.GetStatus();

                VisualStateSetter setter;
                assigned = setter.targetName.TryAssign(
                    sourceSetter.TargetName());
                if (!assigned) return assigned.GetStatus();
                setter.property = property->Handle();
                setter.value = std::move(value).Value();
                assigned = state.setters.TryPushBack(std::move(setter));
                if (!assigned) return assigned.GetStatus();
            }
            assigned = group.states.TryPushBack(std::move(state));
            if (!assigned) return assigned.GetStatus();
        }
        assigned = groups.TryPushBack(std::move(group));
        if (!assigned) return assigned.GetStatus();
    }
    return groups;
}

Base::Result<TypeId> ResolveTargetType(
    Base::StringView name,
    MetadataRuntime& runtime) noexcept {
    if (name.Empty()) {
        return InvalidTheme("ControlTemplate requires TargetType");
    }
    const MetadataTypeDescriptor* type = runtime.Descriptors().FindType(
        AeroNamespaceUri(),
        name);
    if (type == nullptr || !runtime.Descriptors().IsDerivedFrom(
            type->Id(), Control::StaticTypeId())) {
        return InvalidTheme(
            "ControlTemplate TargetType is not a registered Control");
    }
    return type->Id();
}

} // namespace

Base::Result<ThemeObjectModel> LoadThemeObjectModel(
    Base::StringView genericXaml,
    Base::StringView paletteXaml,
    MetadataRuntime& runtime) noexcept {
    if (!runtime.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Theme loading requires a frozen MetadataRuntime");
    }

    DependencyPropertyRegistry& properties =
        runtime.Domain().DependencyProperties();
    Core::Dispatcher dispatcher;
    ObjectServicesScope services(dispatcher, properties, runtime);

    XamlSchemaContext schema(runtime.Domain(), runtime);
    ThemeSchemaLoader loader(runtime, properties);
    Base::Result<void> registered = loader.Register(schema);
    if (!registered) return registered.GetStatus();
    registered = schema.Freeze();
    if (!registered) return registered.GetStatus();

    Base::Result<XamlLoadResult> palette = LoadDocument(
        paletteXaml,
        schema);
    if (!palette) return palette.GetStatus();
    Base::Result<ResourceDictionaryObject*> paletteRoot =
        RequireRoot<ResourceDictionaryObject>(
            palette.Value(),
            ResourceDictionaryObject::StaticTypeId(),
            "Theme palette root must be ResourceDictionary");
    if (!paletteRoot) return paletteRoot.GetStatus();

    ThemeObjectModel model;
    if (paletteRoot.Value()->Variant() == Base::StringView("Light")) {
        model.variant = ThemeVariant::Light;
    } else if (paletteRoot.Value()->Variant() == Base::StringView("Dark")) {
        model.variant = ThemeVariant::Dark;
    } else {
        return InvalidTheme(
            "Theme palette Variant must be Light or Dark");
    }
    model.resources = std::move(palette.Value().resources);

    Base::Result<XamlLoadResult> generic = LoadDocument(
        genericXaml,
        schema,
        &model.resources);
    if (!generic) return generic.GetStatus();
    Base::Result<ResourceDictionaryObject*> genericRoot =
        RequireRoot<ResourceDictionaryObject>(
            generic.Value(),
            ResourceDictionaryObject::StaticTypeId(),
            "Generic theme root must be ResourceDictionary");
    if (!genericRoot) return genericRoot.GetStatus();

    for (const Base::Ref<Base::Object>& entryObject :
         genericRoot.Value()->Entries()) {
        if (!entryObject || entryObject->RuntimeType() !=
                ThemeControlTemplateObject::StaticTypeId()) {
            return InvalidTheme(
                "Generic theme contains an invalid dictionary entry");
        }
        auto& source = static_cast<ThemeControlTemplateObject&>(*entryObject);
        Base::Result<TypeId> target = ResolveTargetType(
            source.TargetType(),
            runtime);
        if (!target) return target.GetStatus();
        for (const ThemeTemplateDefinition& existing : model.templates) {
            if (existing.targetType == target.Value()) {
                return InvalidTheme(
                    "Theme contains duplicate ControlTemplate TargetType");
            }
        }

        Base::Result<ThemeTemplateBlueprint> blueprint = CompileBlueprint(
            source,
            loader.Edges(),
            runtime,
            properties);
        if (!blueprint) return blueprint.GetStatus();
        Base::Result<Base::Vector<VisualStateGroup>> groups =
            CompileVisualStates(
                source,
                blueprint.Value(),
                runtime,
                properties);
        if (!groups) return groups.GetStatus();

        ThemeTemplateDefinition definition;
        definition.targetType = target.Value();
        definition.blueprint = std::move(blueprint).Value();
        definition.visualStateGroups = std::move(groups).Value();
        Base::Result<void> appended = model.templates.TryPushBack(
            std::move(definition));
        if (!appended) return appended.GetStatus();
    }

    if (model.templates.Empty()) {
        return InvalidTheme("Generic theme contains no ControlTemplate entries");
    }
    return model;
}

Base::Result<void> BuildThemeTemplate(
    TemplateBuildContext& context,
    void* factoryContext) noexcept {
    auto* blueprint = static_cast<ThemeTemplateBlueprint*>(factoryContext);
    if (blueprint == nullptr || blueprint->runtime == nullptr ||
        blueprint->nodes.Empty()) {
        return InvalidTheme("Theme template blueprint is invalid");
    }

    Base::Vector<Visual*> visuals;
    for (std::uint32_t index = 0U;
         index < blueprint->nodes.Size();
         ++index) {
        const ThemePrototypeNode& node = blueprint->nodes[index];
        Base::Result<Base::Ref<Base::Object>> created =
            blueprint->runtime->CreateObject(node.type);
        if (!created) return created.GetStatus();
        Base::Ref<Base::Object> owner = std::move(created).Value();
        if (!owner || owner->RuntimeType() != node.type ||
            !blueprint->runtime->Descriptors().IsDerivedFrom(
                node.type, Visual::StaticTypeId()) ||
            !blueprint->runtime->Descriptors().IsDerivedFrom(
                node.type, DependencyObject::StaticTypeId())) {
            return InvalidTheme(
                "Theme template factory created an incompatible object");
        }
        auto* visual = static_cast<Visual*>(owner.Get());
        auto* dependencyObject = static_cast<DependencyObject*>(owner.Get());
        for (const ThemePrototypeProperty& property : node.properties) {
            Base::Result<void> applied = dependencyObject->SetValue(
                property.property,
                property.value);
            if (!applied) return applied.GetStatus();
        }

        Base::Result<void> added;
        if (node.parent == UINT32_MAX) {
            added = context.SetRoot(
                node.name.View(),
                std::move(owner),
                *visual);
        } else {
            if (node.parent >= visuals.Size()) {
                return InvalidTheme(
                    "Theme template node parent is invalid");
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
        if (blueprint->contentPresenter >= visuals.Size() ||
            !blueprint->runtime->Descriptors().IsDerivedFrom(
                context.TemplatedParent().RuntimeType(),
                ContentControl::StaticTypeId())) {
            return InvalidTheme(
                "Theme ContentPresenter requires a ContentControl target");
        }
        Base::Result<bool> projected = context.ProjectContent(
            static_cast<ContentControl&>(context.TemplatedParent()),
            static_cast<ContentPresenter&>(
                *visuals[blueprint->contentPresenter]));
        if (!projected) return projected.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup::Detail
