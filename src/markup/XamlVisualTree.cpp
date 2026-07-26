#include <Aero/Markup/XamlVisualTree.hpp>

namespace Aero::Markup {
namespace {

Base::Status InvalidVisualTree(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidVisualTreeState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

} // namespace

XamlVisualTreeHost::XamlVisualTreeHost(
    Presentation::ObjectTree& tree, Presentation::LayoutManager& layout,
    Core::EffectiveValueEngine& values, Presentation::RenderManager* renderer) noexcept
    : values_(&values), mount_(tree, layout, renderer), edges_(),
      mountEdges_(), nodes_() {}

XamlVisualTreeHost::~XamlVisualTreeHost() noexcept { AERO_ASSERT(!mount_.IsMounted()); }

Base::Result<void> XamlVisualTreeHost::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema_ != nullptr || schema.IsFrozen() ||
        !schema.Descriptors().IsSealed() || !schema.Facets().IsSealed()) {
        return InvalidVisualTreeState(
            "XAML visual-tree metadata is not ready");
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

Base::Result<Presentation::Visual*> XamlVisualTreeHost::ResolveVisual(
    Base::Object& object, Core::TypeId type) const noexcept {
    if (schema_ == nullptr || object.RuntimeType() != type ||
        !schema_->Descriptors().IsDerivedFrom(
            type, Presentation::Visual::StaticTypeId())) {
        return InvalidVisualTree(
            "XAML object metadata is not compatible with Visual");
    }
    return static_cast<Presentation::Visual*>(&object);
}

Base::Result<Presentation::UIElement*> XamlVisualTreeHost::ResolveUIElement(
    Base::Object& object, Core::TypeId type) const noexcept {
    Base::Result<Presentation::Visual*> visual = ResolveVisual(object, type);
    if (!visual) return visual.GetStatus();
    Presentation::UIElement* element = visual.Value()->AsUIElement();
    return element != nullptr ? Base::Result<Presentation::UIElement*>(element)
        : Base::Result<Presentation::UIElement*>(
            InvalidVisualTree("XAML object is not a UIElement"));
}

Presentation::FrameworkElement* XamlVisualTreeHost::ResolveFrameworkElement(
    Base::Object& object, Core::TypeId type) const noexcept {
    Base::Result<Presentation::Visual*> visual = ResolveVisual(object, type);
    return visual ? visual.Value()->AsFrameworkElement() : nullptr;
}

Base::Result<void> XamlVisualTreeHost::AddNode(
    Presentation::Visual& node) noexcept {
    for (Presentation::Visual* existing : nodes_) {
        if (existing == &node) return {};
    }
    return nodes_.TryPushBack(&node);
}

Base::Result<void> XamlVisualTreeHost::StageContent(
    Base::Object& object, const XamlValue& value,
    const XamlServiceProvider& services) noexcept {
    if (mount_.IsMounted() || schema_ == nullptr || services.targetObject != &object ||
        value.Kind() != XamlValueKind::Object || value.IsNullObject() ||
        !value.AsObject()) {
        return InvalidVisualTreeState(
            "XAML visual content requires a non-null object before mount");
    }

    const Core::ContentFacet* content =
        schema_->Facets().FindContentByMember(services.targetMember);
    if (content == nullptr || content->write == nullptr ||
        content->clear == nullptr ||
        !Core::HasContentFlag(content->flags, Core::ContentFlags::Visual) ||
        !schema_->Descriptors().IsDerivedFrom(
            services.targetObjectType, content->type)) {
        return InvalidVisualTree(
            "XAML content target has no visual content facet");
    }

    Base::Result<Presentation::UIElement*> parentResult = ResolveUIElement(
        object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();
    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Presentation::UIElement*> childResult = ResolveUIElement(
        *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();

    // Reserve every container that will grow before mutating the control. This
    // keeps content writes failure-atomic even under allocator exhaustion.
    Base::Result<void> reserved = edges_.TryReserve(edges_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    reserved = mountEdges_.TryReserve(mountEdges_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();
    reserved = nodes_.TryReserve(nodes_.Size() + 2U);
    if (!reserved) return reserved.GetStatus();

    Base::Result<Presentation::Visual*> parentNode = ResolveVisual(
        object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<Presentation::Visual*> childNode = ResolveVisual(
        *childObject, value.Type());
    if (!childNode) return childNode.GetStatus();

    Base::Result<void> parentAdded = AddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<void> childAdded = AddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();

    Base::Ref<Base::Object> parentOwner =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Base::Result<void> tracked = edges_.TryPushBack({
        std::move(parentOwner), value.AsObject(),
        content->clear, content->context});
    if (!tracked) return tracked.GetStatus();
    tracked = mountEdges_.TryPushBack({
        parentResult.Value(), childResult.Value(), {}});
    if (!tracked) {
        edges_.PopBack();
        return tracked.GetStatus();
    }

    // All graph bookkeeping is now committed. Content callbacks are required
    // to be failure-atomic; if one rejects the child, remove the staged edge
    // without clearing content that may have existed before this write.
    Base::Result<void> written = content->write(
        object, value.AsObject(), content->context);
    if (!written) {
        mountEdges_.PopBack();
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> XamlVisualTreeHost::Mount(
    Base::Object& root,
    Core::TypeId rootType,
    Presentation::Size availableSize) noexcept {
    if (mount_.IsMounted() || schema_ == nullptr) {
        return InvalidVisualTreeState(
            "XAML visual tree cannot mount in its current state");
    }
    Base::Result<Presentation::Visual*> rootNode =
        ResolveVisual(root, rootType);
    if (!rootNode) return rootNode.GetStatus();
    Base::Result<Presentation::UIElement*> rootLayout =
        ResolveUIElement(root, rootType);
    if (!rootLayout) return rootLayout.GetStatus();
    Base::Result<void> added = AddNode(*rootNode.Value());
    if (!added) return added.GetStatus();

    return mount_.Mount(
        *rootNode.Value(),
        *rootLayout.Value(),
        ResolveFrameworkElement(root, rootType),
        {mountEdges_.Data(), mountEdges_.Size()},
        availableSize);
}
void XamlVisualTreeHost::ReleaseStagedContent() noexcept {
    for (std::uint32_t index = 0U; index < edges_.Size(); ++index) {
        Edge& edge = edges_[index];
        bool firstForParent = true;
        for (std::uint32_t prior = 0U; prior < index; ++prior) {
            if (edges_[prior].parentOwner.Get() == edge.parentOwner.Get()) {
                firstForParent = false;
                break;
            }
        }
        if (firstForParent && edge.clearContent != nullptr) {
            (void)edge.clearContent(
                *edge.parentOwner.Get(), edge.contentContext);
        }
    }
}

Base::Result<void> XamlVisualTreeHost::Unmount() noexcept {
    Base::Result<void> unmounted = mount_.Unmount(
        {mountEdges_.Data(), mountEdges_.Size()});
    if (!unmounted) return unmounted.GetStatus();

    for (Presentation::Visual* node : nodes_) {
        if (node != nullptr) (void)values_->DetachObject(*node);
    }
    ReleaseStagedContent();
    edges_.Clear();
    mountEdges_.Clear();
    nodes_.Clear();
    return {};
}
Base::Result<void> XamlVisualTreeHost::DiscardStaged() noexcept {
    if (mount_.IsMounted()) {
        return InvalidVisualTreeState(
            "Mounted XAML visual tree must be unmounted before discarding it");
    }
    ReleaseStagedContent();
    edges_.Clear();
    mountEdges_.Clear();
    nodes_.Clear();
    return {};
}

bool XamlVisualTreeHost::HandlesContentMember(
    const XamlResolvedMember& member,
    void* context) noexcept {
    auto* host = static_cast<XamlVisualTreeHost*>(context);
    if (host == nullptr || host->schema_ == nullptr ||
        member.kind != Core::MemberKind::Property) {
        return false;
    }
    const Core::ContentFacet* content =
        host->schema_->Facets().FindContentByMember(member.id);
    return content != nullptr && content->write != nullptr &&
        Core::HasContentFlag(content->flags, Core::ContentFlags::Visual);
}

Base::Result<void> XamlVisualTreeHost::SetContentMember(
    Base::Object& object, const XamlValue& value,
    const XamlServiceProvider& services, void* context) noexcept {
    auto* host = static_cast<XamlVisualTreeHost*>(context);
    return host != nullptr ? host->StageContent(object, value, services)
        : Base::Result<void>(InvalidVisualTreeState(
            "XAML visual-tree host is unavailable"));
}

Base::Result<Base::Ref<Base::Object>> LoadXamlVisualTreeWithActivation(
    XamlVisualTreeHost& host, XamlObjectWriter& writer,
    XamlNodeReader& reader, XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    Base::Result<void> discarded = host.DiscardStaged();
    if (!discarded) return discarded.GetStatus();
    Base::Result<Base::Ref<Base::Object>> loaded = LoadXamlWithActivation(
        writer, reader, providers, activation);
    if (!loaded) {
        (void)host.DiscardStaged();
        return loaded.GetStatus();
    }
    return std::move(loaded).Value();
}

Base::Result<Base::Ref<Base::Object>> LoadXamlVisualTreeWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    Base::Result<void> discarded = host.DiscardStaged();
    if (!discarded) return discarded.GetStatus();
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadXamlWithActivation(
            writer, document, providers, activation);
    if (!loaded) {
        static_cast<void>(host.DiscardStaged());
        return loaded.GetStatus();
    }
    return std::move(loaded).Value();
}

Base::Result<XamlLoadResult> LoadXamlVisualTreeDocumentWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    Base::Result<void> discarded = host.DiscardStaged();
    if (!discarded) return discarded.GetStatus();
    Base::Result<XamlLoadResult> loaded = LoadXamlDocumentWithActivation(
        writer, reader, providers, activation);
    if (!loaded) {
        (void)host.DiscardStaged();
        return loaded.GetStatus();
    }
    return std::move(loaded).Value();
}

Base::Result<XamlLoadResult> LoadXamlVisualTreeDocumentWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    Base::Result<void> discarded = host.DiscardStaged();
    if (!discarded) return discarded.GetStatus();
    Base::Result<XamlLoadResult> loaded = LoadXamlDocumentWithActivation(
        writer, document, providers, activation);
    if (!loaded) {
        (void)host.DiscardStaged();
        return loaded.GetStatus();
    }
    return std::move(loaded).Value();
}

Base::Result<void> XamlVisualTreeHost::Resize(
    Presentation::Size availableSize) noexcept {
    return mount_.Resize(availableSize);
}

} // namespace Aero::Markup

