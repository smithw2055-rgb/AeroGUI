#include "ObjectWriterState.hpp"

#include <utility>

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
            member});
    if (!retained) return retained.GetStatus();
    Base::Result<void> written =
        runtime.WriteContent(parent, member, child);
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
                 edges_[earlier].parent != edge.parent);
        }
        if (firstForParent &&
            edge.parent != nullptr &&
            edge.runtime != nullptr) {
            (void)edge.runtime->ClearContent(
                *edge.parent,
                edge.member);
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
}

void DeferredContentPlan::ReleaseAll() noexcept {
    while (!edges_.Empty()) {
        Base::Object* owner =
            edges_.Front().owner;
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

Base::Result<Presentation::Visual*> ObjectWriter::ResolveVisual(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Core::TypeId type) noexcept {
    if (object.RuntimeType() != type ||
        !schema.Types().IsDerivedFrom(
            type, Presentation::Visual::StaticTypeId())) {
        return InvalidContent(
            "XAML object metadata is not compatible with Visual");
    }
    return static_cast<Presentation::Visual*>(&object);
}

Base::Result<Presentation::UIElement*> ObjectWriter::ResolveUIElement(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Core::TypeId type) noexcept {
    Base::Result<Presentation::Visual*> visual =
        ResolveVisual(schema, object, type);
    if (!visual) return visual.GetStatus();
    Presentation::UIElement* element =
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
    if (!contentResult) {
        return InvalidContent(
            "XAML content target has no content metadata");
    }
    const Core::ContentInfo& content = contentResult.Value();
    if (!content.writable || !content.clearable ||
        !content.IsVisual() ||
        !schema.Types().IsDerivedFrom(
            services.targetObjectType, content.ownerType)) {
        return InvalidContent(
            "XAML content target has no visual content facet");
    }

    Base::Result<Presentation::UIElement*> parentResult =
        ResolveUIElement(
            schema, object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();

    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Presentation::UIElement*> childResult =
        ResolveUIElement(schema, *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();

    if (services.deferredContentOwner != nullptr) {
        if (services.deferredContent == nullptr) {
            return InvalidContentState(
                "Deferred XAML content plan is unavailable");
        }
        return services.deferredContent->Stage(
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

    Base::Result<Presentation::Visual*> parentNode =
        ResolveVisual(
            schema, object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<Presentation::Visual*> childNode =
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
            runtime, services.targetMember});
    if (!tracked) return tracked.GetStatus();

    tracked = plan->mountEdges.TryPushBack({
        parentResult.Value(), childResult.Value(), {}});
    if (!tracked) {
        plan->contentEdges.PopBack();
        return tracked.GetStatus();
    }

    Base::Result<void> written = runtime->WriteContent(
        object, services.targetMember, value.AsObject());
    if (!written) {
        plan->mountEdges.PopBack();
        plan->contentEdges.PopBack();
        return written.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup
