#include "ObjectWriterState.hpp"

#include <utility>
#include "../ui/RuntimeManagers.hpp"

namespace Aero::Markup {
namespace {

Base::Status InvalidContent(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidContentState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

} // namespace

Base::Result<void> DeferredContentPlan::Stage(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    Core::MetadataRuntime& runtime,
    Core::MemberId member) noexcept {
    if (!child || member == Core::InvalidMemberId ||
        !runtime.IsReady()) {
        return InvalidContentState(
            "Deferred XAML content edge is invalid");
    }
    Base::Result<void> retained =
        edges_.TryPushBack({
            &owner,
            &parent,
            child,
            &runtime,
            member,
            false});
    if (!retained) return retained.GetStatus();
    Base::Result<void> written =
        runtime.WriteContent(parent, member, child);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageProperty(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    Core::MetadataRuntime& runtime,
    Core::MemberId member) noexcept {
    if (!child ||
        member == Core::InvalidMemberId) {
        return InvalidContentState(
            "Deferred XAML structural property edge is invalid");
    }
    const Core::PropertyInfo* property =
        runtime.Types().FindProperty(member);
    if (property == nullptr) {
        return InvalidContent(
            "Deferred XAML structural property was not found");
    }
    Base::Result<void> retained =
        edges_.TryPushBack({
            &owner,
            &parent,
            child,
            &runtime,
            member,
            true});
    if (!retained) return retained.GetStatus();
    const Core::Value value =
        Core::Value::FromObject(
            property->ValueType(), child);
    Base::Result<void> written =
        runtime.SetProperty(
            parent, member, value);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::CopyForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredContentEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredContentEdge& edge : edges_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.TryPushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageBinding(
    Base::Object& owner,
    Base::Object* source,
    Core::DependencyObject& target,
    Aero::Detail::BindingManager& manager,
    Core::MetadataRuntime& metadata,
    Core::DependencyPropertyHandle targetProperty,
    Core::DependencyPropertyHandle dataContextProperty,
    Base::StringView path,
    Base::StringView stringFormat,
    Data::BindingMode mode,
    Core::UpdateSourceTrigger updateSourceTrigger,
    bool bindsToSource) noexcept {
    if (!targetProperty.IsValid() ||
        (path.Empty() && !bindsToSource) ||
        !metadata.IsReady()) {
        return InvalidContentState(
            "Deferred XAML Binding declaration is invalid");
    }
    DeferredBindingEdge edge;
    edge.owner = &owner;
    edge.source = source;
    edge.target = &target;
    edge.manager = &manager;
    edge.metadata = &metadata;
    edge.targetProperty = targetProperty;
    edge.dataContextProperty = dataContextProperty;
    edge.mode = mode;
    edge.bindsToSource = bindsToSource;
    edge.updateSourceTrigger = updateSourceTrigger;
    Base::Result<void> assigned =
        edge.path.TryAssign(path);
    if (!assigned) return assigned.GetStatus();
    assigned = edge.stringFormat.TryAssign(
        stringFormat);
    if (!assigned) return assigned.GetStatus();
    return bindings_.TryPushBack(std::move(edge));
}

Base::Result<void>
DeferredContentPlan::CopyBindingsForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredBindingEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredBindingEdge& edge : bindings_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.TryPushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

void DeferredContentPlan::ReleaseOwner(
    Base::Object& owner) noexcept {
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner != &owner) continue;
        bool firstForParent = true;
        for (std::uint32_t earlier = 0U;
             earlier < index;
             ++earlier) {
            firstForParent =
                firstForParent &&
                (edges_[earlier].owner != &owner ||
                 edges_[earlier].parent !=
                     edge.parent ||
                 (edge.property &&
                  edges_[earlier].member !=
                      edge.member));
        }
        if (firstForParent &&
            edge.parent != nullptr &&
            edge.runtime != nullptr) {
            if (edge.property) {
                const Core::PropertyInfo* property =
                    edge.runtime->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.runtime->SetProperty(
                        *edge.parent,
                        edge.member,
                        Core::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.runtime->ClearContent(
                    *edge.parent,
                    edge.member);
            }
        }
    }

    std::uint32_t output = 0U;
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            edges_[output] = std::move(edge);
        }
        ++output;
    }
    (void)edges_.TryResize(output);

    output = 0U;
    for (std::uint32_t index = 0U;
         index < bindings_.Size();
         ++index) {
        DeferredBindingEdge& edge = bindings_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            bindings_[output] = std::move(edge);
        }
        ++output;
    }
    (void)bindings_.TryResize(output);
}

void DeferredContentPlan::ReleaseAll() noexcept {
    while (!edges_.Empty() || !bindings_.Empty()) {
        Base::Object* owner = !edges_.Empty()
            ? edges_.Front().owner
            : bindings_.Front().owner;
        if (owner == nullptr) {
            edges_.Clear();
            return;
        }
        ReleaseOwner(*owner);
    }
}

ObjectWriter::ObjectWriter(
    ::Aero::Markup::Schema& schema,
    Core::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics) {}

Base::Result<LoaderResult> ObjectWriter::LoadDocument(
    NodeReader& reader) noexcept {
    ObjectWriterState state(*this);
    return state.Load(reader);
}

Base::Result<LoaderResult> ObjectWriter::LoadDocument(
    const CompiledDocument& document) noexcept {
    ObjectWriterState state(*this);
    return state.Load(document);
}

Base::Result<Aero::Visual*> ObjectWriter::ResolveVisual(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Core::TypeId type) noexcept {
    if (object.RuntimeType() != type ||
        !schema.Types().IsDerivedFrom(
            type, Aero::Visual::StaticTypeId())) {
        return InvalidContent(
            "XAML object metadata is not compatible with Visual");
    }
    return static_cast<Aero::Visual*>(&object);
}

Base::Result<Aero::UIElement*> ObjectWriter::ResolveUIElement(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Core::TypeId type) noexcept {
    Base::Result<Aero::Visual*> visual =
        ResolveVisual(schema, object, type);
    if (!visual) return visual.GetStatus();
    Aero::UIElement* element =
        visual.Value()->AsUIElement();
    if (element == nullptr) {
        return InvalidContent("XAML object is not a UIElement");
    }
    return element;
}

Base::Result<void> ObjectWriter::StageContent(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    const Core::Value& value,
    const ExtensionContext& services) noexcept {
    VisualContentPlan* plan = services.visualContent;
    if (plan == nullptr || services.targetObject != &object ||
        value.Kind() != Core::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return InvalidContentState(
            "XAML visual content requires a non-null object");
    }

    Core::MetadataRuntime* runtime = schema.Runtime();
    if (runtime == nullptr) {
        return InvalidContentState(
            "XAML content metadata runtime is unavailable");
    }
    Base::Result<Core::ContentInfo> contentResult =
        runtime->GetContentInfo(services.targetMember);
    const Core::PropertyInfo* property =
        schema.Types().FindProperty(
            services.targetMember);
    const bool structuralProperty =
        property != nullptr &&
        (static_cast<std::uint32_t>(
             property->Flags()) &
         static_cast<std::uint32_t>(
             Core::PropertyFlags::Structural)) !=
            0U &&
        runtime->CanWriteProperty(
            services.targetMember);
    if (!contentResult && !structuralProperty) {
        return InvalidContent(
            "XAML content target has no content metadata");
    }
    if (!structuralProperty) {
        const Core::ContentInfo& content =
            contentResult.Value();
        if (!content.writable ||
            !content.clearable ||
            !content.IsVisual() ||
            !schema.Types().IsDerivedFrom(
                services.targetObjectType,
                content.ownerType)) {
            return InvalidContent(
                "XAML content target has no visual content facet");
        }
    }

    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Aero::UIElement*> childResult =
        ResolveUIElement(schema, *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();

    // A deferred template owns a visual root without itself being a Visual.
    // Commit that root through the template's content accessor; descendant
    // visual edges are staged below once their actual visual parent exists.
    if (services.deferredContentOwner == &object &&
        !schema.Types().IsDerivedFrom(
            services.targetObjectType,
            Aero::Visual::StaticTypeId())) {
        if (structuralProperty) {
            return InvalidContent(
                "A non-visual template root cannot use a visual structural property");
        }
        return runtime->WriteContent(
            object,
            services.targetMember,
            value.AsObject());
    }

    Base::Result<Aero::UIElement*> parentResult =
        ResolveUIElement(
            schema, object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();

    if (services.deferredContentOwner != nullptr) {
        if (services.deferredContent == nullptr) {
            return InvalidContentState(
                "Deferred XAML content plan is unavailable");
        }
        return structuralProperty
            ? services.deferredContent->
                  StageProperty(
                      *services.
                           deferredContentOwner,
                      object,
                      value.AsObject(),
                      *runtime,
                      services.targetMember)
            : services.deferredContent->Stage(
                  *services.deferredContentOwner,
                  object,
                  value.AsObject(),
                  *runtime,
                  services.targetMember);
    }

    Base::Result<void> reserved = plan->TryReserve(
        plan->contentEdges.Size() + 1U,
        plan->mountEdges.Size() + 1U,
        plan->nodes.Size() + 2U);
    if (!reserved) return reserved.GetStatus();

    Base::Result<Aero::Visual*> parentNode =
        ResolveVisual(
            schema, object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<Aero::Visual*> childNode =
        ResolveVisual(schema, *childObject, value.Type());
    if (!childNode) return childNode.GetStatus();

    Base::Result<void> parentAdded =
        plan->TryAddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<void> childAdded =
        plan->TryAddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();

    Base::Ref<Base::Object> parentOwner =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Base::Result<void> tracked =
        plan->contentEdges.TryPushBack({
            std::move(parentOwner), value.AsObject(),
            runtime, services.targetMember,
            structuralProperty});
    if (!tracked) return tracked.GetStatus();

    tracked = plan->mountEdges.TryPushBack({
        parentResult.Value(), childResult.Value(), {}});
    if (!tracked) {
        plan->contentEdges.PopBack();
        return tracked.GetStatus();
    }

    Base::Result<void> written =
        structuralProperty
        ? runtime->SetProperty(
              object,
              services.targetMember,
              value)
        : runtime->WriteContent(
              object,
              services.targetMember,
              value.AsObject());
    if (!written) {
        plan->mountEdges.PopBack();
        plan->contentEdges.PopBack();
        return written.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup
