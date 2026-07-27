#include <Aero/Markup/Runtime/XamlContentWriter.hpp>

namespace Aero::Markup {
namespace {

Base::Status InvalidContent(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidContentState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

Base::Result<void> XamlDeferredContentPlan::Stage(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    Core::ContentWriteCallback write,
    Core::ContentClearCallback clear,
    void* contentContext) noexcept {
    if (!child || write == nullptr || clear == nullptr) {
        return InvalidContentState(
            "Deferred XAML content edge is invalid");
    }
    Base::Result<void> retained =
        edges_.TryPushBack({
            &owner,
            &parent,
            child,
            write,
            clear,
            contentContext});
    if (!retained) return retained.GetStatus();
    Base::Result<void> written =
        write(parent, child, contentContext);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> XamlDeferredContentPlan::CopyForOwner(
    const Base::Object& owner,
    Base::Vector<XamlDeferredContentEdge>& output) const noexcept {
    output.Clear();
    for (const XamlDeferredContentEdge& edge : edges_) {
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

void XamlDeferredContentPlan::ReleaseOwner(
    Base::Object& owner) noexcept {
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        XamlDeferredContentEdge& edge = edges_[index];
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
            edge.clear != nullptr) {
            (void)edge.clear(
                *edge.parent,
                edge.contentContext);
        }
    }

    std::uint32_t output = 0U;
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        XamlDeferredContentEdge& edge = edges_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            edges_[output] = std::move(edge);
        }
        ++output;
    }
    (void)edges_.TryResize(output);
}

void XamlDeferredContentPlan::ReleaseAll() noexcept {
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

Base::Result<void> XamlContentWriter::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema_ != nullptr || schema.IsFrozen() ||
        !schema.Descriptors().IsSealed() || !schema.Facets().IsSealed()) {
        return InvalidContentState(
            "XAML content writer metadata is not ready");
    }

    schema_ = &schema;
    Base::Result<void> registered = schema.TryRegisterMemberProvider({
        &HandlesContentMember,
        &SetContentMember,
        this,
        XamlMemberWriteMode::SetOnce,
        false});
    if (!registered) {
        schema_ = nullptr;
        return registered.GetStatus();
    }
    return {};
}

Base::Result<Presentation::Visual*> XamlContentWriter::ResolveVisual(
    Base::Object& object, Core::TypeId type) const noexcept {
    if (schema_ == nullptr || object.RuntimeType() != type ||
        !schema_->Descriptors().IsDerivedFrom(
            type, Presentation::Visual::StaticTypeId())) {
        return InvalidContent(
            "XAML object metadata is not compatible with Visual");
    }
    return static_cast<Presentation::Visual*>(&object);
}

Base::Result<Presentation::UIElement*> XamlContentWriter::ResolveUIElement(
    Base::Object& object, Core::TypeId type) const noexcept {
    Base::Result<Presentation::Visual*> visual = ResolveVisual(object, type);
    if (!visual) return visual.GetStatus();
    Presentation::UIElement* element = visual.Value()->AsUIElement();
    if (element == nullptr) {
        return InvalidContent("XAML object is not a UIElement");
    }
    return element;
}

Base::Result<void> XamlContentWriter::StageContent(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services) noexcept {
    XamlVisualContentPlan* plan = services.visualContent;
    if (plan == nullptr || schema_ == nullptr ||
        services.targetObject != &object ||
        value.Kind() != XamlValueKind::Object || value.IsNullObject() ||
        !value.AsObject()) {
        return InvalidContentState(
            "XAML visual content requires a non-null object");
    }

    const Core::ContentFacet* content =
        schema_->Facets().FindContentByMember(services.targetMember);
    if (content == nullptr || content->write == nullptr ||
        content->clear == nullptr ||
        !Core::HasContentFlag(content->flags, Core::ContentFlags::Visual) ||
        !schema_->Descriptors().IsDerivedFrom(
            services.targetObjectType, content->type)) {
        return InvalidContent(
            "XAML content target has no visual content facet");
    }

    Base::Result<Presentation::UIElement*> parentResult = ResolveUIElement(
        object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();

    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Presentation::UIElement*> childResult = ResolveUIElement(
        *childObject, value.Type());
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
            content->write,
            content->clear,
            content->context);
    }

    Base::Result<void> reserved = plan->TryReserve(
        plan->contentEdges.Size() + 1U,
        plan->mountEdges.Size() + 1U,
        plan->nodes.Size() + 2U);
    if (!reserved) return reserved.GetStatus();

    Base::Result<Presentation::Visual*> parentNode = ResolveVisual(
        object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<Presentation::Visual*> childNode = ResolveVisual(
        *childObject, value.Type());
    if (!childNode) return childNode.GetStatus();

    Base::Result<void> parentAdded =
        plan->TryAddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<void> childAdded =
        plan->TryAddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();

    Base::Ref<Base::Object> parentOwner =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Base::Result<void> tracked = plan->contentEdges.TryPushBack({
        std::move(parentOwner), value.AsObject(),
        content->clear, content->context});
    if (!tracked) return tracked.GetStatus();

    tracked = plan->mountEdges.TryPushBack({
        parentResult.Value(), childResult.Value(), {}});
    if (!tracked) {
        plan->contentEdges.PopBack();
        return tracked.GetStatus();
    }

    Base::Result<void> written = content->write(
        object, value.AsObject(), content->context);
    if (!written) {
        plan->mountEdges.PopBack();
        plan->contentEdges.PopBack();
        return written.GetStatus();
    }
    return {};
}

bool XamlContentWriter::HandlesContentMember(
    const XamlResolvedMember& member,
    void* context) noexcept {
    auto* writer = static_cast<XamlContentWriter*>(context);
    if (writer == nullptr || writer->schema_ == nullptr ||
        member.kind != Core::MemberKind::Property) {
        return false;
    }
    const Core::ContentFacet* content =
        writer->schema_->Facets().FindContentByMember(member.id);
    return content != nullptr && content->write != nullptr &&
        Core::HasContentFlag(content->flags, Core::ContentFlags::Visual);
}

Base::Result<void> XamlContentWriter::SetContentMember(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    auto* writer = static_cast<XamlContentWriter*>(context);
    return writer != nullptr ? writer->StageContent(object, value, services)
        : Base::Result<void>(InvalidContentState(
            "XAML content writer is unavailable"));
}

} // namespace Aero::Markup
