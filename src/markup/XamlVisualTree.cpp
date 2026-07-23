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
    Core::ObjectTree& tree, Core::LayoutManager& layout,
    Core::EffectiveValueEngine& values, Core::RenderManager* renderer) noexcept
    : tree_(&tree), layout_(&layout), values_(&values), renderer_(renderer),
      types_(), singles_(), collections_(), edges_(), nodes_() {}

XamlVisualTreeHost::~XamlVisualTreeHost() noexcept { AERO_ASSERT(!mounted_); }

Base::Result<void> XamlVisualTreeHost::TryRegisterType(
    const XamlVisualTreeTypeRegistration& registration) noexcept {
    if (schema_ != nullptr || registration.type == Core::InvalidTypeId ||
        registration.asVisual == nullptr) {
        return InvalidVisualTree("XAML visual-tree type registration is invalid");
    }
    if (FindType(registration.type) != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "XAML visual-tree type is already registered");
    }
    return types_.TryPushBack(registration);
}

Base::Result<void> XamlVisualTreeHost::TryRegisterSingleContent(
    const XamlSingleContentRegistration& registration) noexcept {
    if (schema_ != nullptr || registration.type == Core::InvalidTypeId ||
        registration.setContent == nullptr || registration.clearContent == nullptr) {
        return InvalidVisualTree("XAML single-content registration is invalid");
    }
    if (FindSingle(registration.type) != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "XAML single-content type is already registered");
    }
    return singles_.TryPushBack(registration);
}

Base::Result<void> XamlVisualTreeHost::TryRegisterCollectionContent(
    const XamlCollectionContentRegistration& registration) noexcept {
    if (schema_ != nullptr || registration.type == Core::InvalidTypeId ||
        registration.member == Core::InvalidMemberId ||
        registration.addChild == nullptr || registration.clearChildren == nullptr) {
        return InvalidVisualTree("XAML collection-content registration is invalid");
    }
    if (FindCollection(registration.type, registration.member) != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "XAML collection-content member is already registered");
    }
    return collections_.TryPushBack(registration);
}

Base::Result<void> XamlVisualTreeHost::Register(XamlSchemaContext& schema) noexcept {
    if (schema_ != nullptr || schema.IsFrozen() || !schema.Types().IsFrozen() ||
        (singles_.Empty() && collections_.Empty())) {
        return InvalidVisualTreeState("XAML visual-tree registries are not ready");
    }

    for (const XamlSingleContentRegistration& registration : singles_) {
        for (const Core::TypeInfo& type : schema.Types().Types()) {
            if (!schema.Types().IsDerivedFrom(type.Id(), registration.type)) continue;
            const Core::MemberId member = schema.Types().FindContentMember(type.Id());
            const Core::PropertyInfo* content = schema.Types().FindProperty(member);
            if (content == nullptr) continue;
            if (schema.FindTypeAdapter(type.Id()) == nullptr) {
                Base::Result<void> typeAdapter = schema.TryRegisterTypeAdapter({
                    type.Id(), nullptr, nullptr, nullptr, this, false, false,
                    nullptr, nullptr});
                if (!typeAdapter) return typeAdapter.GetStatus();
            }
            if (schema.FindMemberAdapter(content->Id()) == nullptr) {
                Base::Result<void> memberAdapter = schema.TryRegisterMemberAdapter({
                    content->Id(), XamlMemberWriteMode::SetOnce, nullptr, this,
                    &SetContentMember, false});
                if (!memberAdapter) return memberAdapter.GetStatus();
            }
        }
    }

    for (const XamlCollectionContentRegistration& registration : collections_) {
        const Core::PropertyInfo* baseMember =
            schema.Types().FindProperty(registration.member);
        if (baseMember == nullptr) {
            return InvalidVisualTree("Collection-content metadata is invalid");
        }
        for (const Core::TypeInfo& type : schema.Types().Types()) {
            if (!schema.Types().IsDerivedFrom(type.Id(), registration.type)) continue;
            const Core::MemberId member = schema.Types().FindContentMember(type.Id());
            const Core::PropertyInfo* children = schema.Types().FindProperty(member);
            if (children == nullptr || children->Name() != baseMember->Name()) continue;
            if (schema.FindTypeAdapter(type.Id()) == nullptr) {
                Base::Result<void> typeAdapter = schema.TryRegisterTypeAdapter({
                    type.Id(), nullptr, nullptr, nullptr, this, false, false,
                    nullptr, nullptr});
                if (!typeAdapter) return typeAdapter.GetStatus();
            }
            if (schema.FindMemberAdapter(children->Id()) == nullptr) {
                Base::Result<void> memberAdapter = schema.TryRegisterMemberAdapter({
                    children->Id(), XamlMemberWriteMode::Collection, nullptr, this,
                    &SetContentMember, false});
                if (!memberAdapter) return memberAdapter.GetStatus();
            }
        }
    }
    schema_ = &schema;
    return {};
}

const XamlVisualTreeTypeRegistration* XamlVisualTreeHost::FindType(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        for (const XamlVisualTreeTypeRegistration& registration : types_) {
            if (registration.type == current) return &registration;
        }
        if (schema_ == nullptr) break;
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

const XamlSingleContentRegistration* XamlVisualTreeHost::FindSingle(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        for (const XamlSingleContentRegistration& registration : singles_) {
            if (registration.type == current) return &registration;
        }
        if (schema_ == nullptr) break;
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

const XamlCollectionContentRegistration* XamlVisualTreeHost::FindCollection(
    Core::TypeId type, Core::MemberId member) const noexcept {
    const Core::PropertyInfo* actual = schema_ != nullptr
        ? schema_->Types().FindProperty(member) : nullptr;
    for (const XamlCollectionContentRegistration& registration : collections_) {
        if (schema_ == nullptr) {
            if (registration.type == type && registration.member == member)
                return &registration;
            continue;
        }
        if (!schema_->Types().IsDerivedFrom(type, registration.type)) continue;
        const Core::PropertyInfo* base =
            schema_->Types().FindProperty(registration.member);
        if (actual != nullptr && base != nullptr && actual->Name() == base->Name())
            return &registration;
    }
    return nullptr;
}

Base::Result<Core::Visual*> XamlVisualTreeHost::ResolveVisual(
    Base::Object& object, Core::TypeId type) const noexcept {
    const XamlVisualTreeTypeRegistration* registration = FindType(type);
    if (registration == nullptr)
        return InvalidVisualTree("XAML object is not a registered Visual");
    Core::Visual* node = registration->asVisual(object, registration->context);
    if (node == nullptr || node->RuntimeType() != type)
        return InvalidVisualTree("XAML Visual runtime type does not match metadata");
    return node;
}

Base::Result<Core::UIElement*> XamlVisualTreeHost::ResolveUIElement(
    Base::Object& object, Core::TypeId type) const noexcept {
    Base::Result<Core::Visual*> visual = ResolveVisual(object, type);
    if (!visual) return visual.GetStatus();
    Core::UIElement* element = visual.Value()->AsUIElement();
    return element != nullptr ? Base::Result<Core::UIElement*>(element)
        : Base::Result<Core::UIElement*>(
            InvalidVisualTree("XAML object is not a UIElement"));
}

Core::FrameworkElement* XamlVisualTreeHost::ResolveFrameworkElement(
    Base::Object& object, Core::TypeId type) const noexcept {
    Base::Result<Core::Visual*> visual = ResolveVisual(object, type);
    return visual ? visual.Value()->AsFrameworkElement() : nullptr;
}

Base::Result<void> XamlVisualTreeHost::AddNode(Core::Visual& node) noexcept {
    for (Core::Visual* existing : nodes_) if (existing == &node) return {};
    return nodes_.TryPushBack(&node);
}

Base::Result<void> XamlVisualTreeHost::StageContent(
    Base::Object& object, const XamlValue& value,
    const XamlServiceProvider& services) noexcept {
    if (mounted_ || services.targetObject != &object ||
        value.Kind() != XamlValueKind::Object || value.IsNullObject() ||
        !value.AsObject()) {
        return InvalidVisualTreeState(
            "XAML Content requires a non-null object before mount");
    }
    Base::Result<Core::UIElement*> parentResult = ResolveUIElement(
        object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();
    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Core::UIElement*> childResult = ResolveUIElement(
        *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();

    XamlClearContentCallback clearSingle = nullptr;
    XamlClearCollectionCallback clearCollection = nullptr;
    XamlConfigureCollectionChildCallback configure = nullptr;
    void* contentContext = nullptr;
    const XamlSingleContentRegistration* single =
        FindSingle(services.targetObjectType);
    if (single != nullptr) {
        Base::Result<void> set = single->setContent(
            object, value.AsObject(), *childResult.Value(), single->context);
        if (!set) return set.GetStatus();
        clearSingle = single->clearContent;
        contentContext = single->context;
    } else {
        const XamlCollectionContentRegistration* collection = FindCollection(
            services.targetObjectType, services.targetMember);
        if (collection == nullptr)
            return InvalidVisualTree(
                "XAML content target is not a registered content host");
        Base::Result<void> added = collection->addChild(
            object, value.AsObject(), *childResult.Value(), collection->context);
        if (!added) return added.GetStatus();
        clearCollection = collection->clearChildren;
        configure = collection->configureChild;
        contentContext = collection->context;
    }

    Base::Result<void> reserveEdges = edges_.TryReserve(edges_.Size() + 1U);
    if (!reserveEdges) return reserveEdges.GetStatus();
    Base::Ref<Base::Object> parentOwner =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Base::Result<Core::Visual*> parentNode = ResolveVisual(
        object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<void> parentAdded = AddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<Core::Visual*> childNode = ResolveVisual(*childObject, value.Type());
    if (!childNode) return childNode.GetStatus();
    Base::Result<void> childAdded = AddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();
    return edges_.TryPushBack({std::move(parentOwner), value.AsObject(),
        parentResult.Value(), childResult.Value(), clearSingle, clearCollection,
        configure, contentContext});
}

Base::Result<void> XamlVisualTreeHost::AttachEdge(Edge& edge) noexcept {
    Base::Result<void> logical = tree_->AttachLogical(*edge.parent, *edge.child);
    if (!logical) return logical.GetStatus();
    edge.logicalAttached = true;
    Base::Result<void> visual = tree_->AttachVisual(*edge.parent, *edge.child);
    if (!visual) { DetachEdge(edge); return visual.GetStatus(); }
    edge.visualAttached = true;
    Base::Result<void> layout = layout_->Attach(*edge.parent, *edge.child);
    if (!layout) { DetachEdge(edge); return layout.GetStatus(); }
    edge.layoutAttached = true;
    if (edge.configureCollectionChild != nullptr) {
        Base::Result<void> configured = edge.configureCollectionChild(
            *edge.parentOwner.Get(), *edge.parent, *edge.child,
            edge.contentContext);
        if (!configured) { DetachEdge(edge); return configured.GetStatus(); }
    }
    if (renderer_ != nullptr) {
        Core::FrameworkElement* parentRender = ResolveFrameworkElement(
            *edge.parentOwner.Get(), edge.parent->RuntimeType());
        Core::FrameworkElement* childRender = ResolveFrameworkElement(
            *edge.childOwner.Get(), edge.child->RuntimeType());
        if (parentRender != nullptr && childRender != nullptr) {
            Base::Result<void> render = renderer_->Attach(*parentRender, *childRender);
            if (!render) { DetachEdge(edge); return render.GetStatus(); }
            edge.renderAttached = true;
        }
    }
    return {};
}

void XamlVisualTreeHost::DetachEdge(Edge& edge) noexcept {
    if (edge.renderAttached) {
        Core::FrameworkElement* parent = ResolveFrameworkElement(
            *edge.parentOwner.Get(), edge.parent->RuntimeType());
        Core::FrameworkElement* child = ResolveFrameworkElement(
            *edge.childOwner.Get(), edge.child->RuntimeType());
        if (parent != nullptr && child != nullptr)
            (void)renderer_->Detach(*parent, *child);
        edge.renderAttached = false;
    }
    if (edge.layoutAttached) {
        (void)layout_->Detach(*edge.parent, *edge.child);
        edge.layoutAttached = false;
    }
    if (edge.visualAttached) {
        (void)tree_->DetachVisual(*edge.parent, *edge.child);
        edge.visualAttached = false;
    }
    if (edge.logicalAttached) {
        (void)tree_->DetachLogical(*edge.parent, *edge.child);
        edge.logicalAttached = false;
    }
}

Base::Result<void> XamlVisualTreeHost::Mount(
    Base::Object& root, Core::TypeId rootType,
    Core::Size availableSize) noexcept {
    if (mounted_ || schema_ == nullptr ||
        !Core::IsValidLayoutSize(availableSize)) {
        return InvalidVisualTreeState(
            "XAML visual tree cannot mount in its current state");
    }
    Base::Result<Core::Visual*> rootNode = ResolveVisual(root, rootType);
    if (!rootNode) return rootNode.GetStatus();
    Base::Result<Core::UIElement*> rootLayout = ResolveUIElement(root, rootType);
    if (!rootLayout) return rootLayout.GetStatus();
    Base::Result<void> added = AddNode(*rootNode.Value());
    if (!added) return added.GetStatus();
    rootNode_ = rootNode.Value();
    rootLayout_ = rootLayout.Value();
    rootRender_ = ResolveFrameworkElement(root, rootType);
    Base::Result<void> treeRoot = tree_->SetRoot(rootNode_);
    if (!treeRoot) return treeRoot.GetStatus();
    Base::Result<void> layoutRoot = layout_->SetRoot(rootLayout_, availableSize);
    if (!layoutRoot) {
        (void)tree_->SetRoot(nullptr);
        return layoutRoot.GetStatus();
    }
    if (renderer_ != nullptr && rootRender_ != nullptr) {
        Base::Result<void> renderRoot = renderer_->SetRoot(rootRender_);
        if (!renderRoot) {
            (void)layout_->SetRoot(nullptr, {});
            (void)tree_->SetRoot(nullptr);
            return renderRoot.GetStatus();
        }
    }
    std::uint32_t attached = 0U;
    while (attached < edges_.Size()) {
        bool progressed = false;
        for (Edge& edge : edges_) {
            if (edge.logicalAttached || edge.parent->OwningTree() != tree_) continue;
            Base::Result<void> result = AttachEdge(edge);
            if (!result) { (void)Unmount(); return result.GetStatus(); }
            ++attached;
            progressed = true;
        }
        if (!progressed) {
            (void)Unmount();
            return InvalidVisualTreeState(
                "XAML content graph is disconnected from its root");
        }
    }
    mounted_ = true;
    return {};
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
        if (!firstForParent) continue;
        if (edge.clearSingle != nullptr)
            (void)edge.clearSingle(*edge.parentOwner.Get(), edge.contentContext);
        else if (edge.clearCollection != nullptr)
            (void)edge.clearCollection(*edge.parentOwner.Get(), edge.contentContext);
    }
}

Base::Result<void> XamlVisualTreeHost::Unmount() noexcept {
    if (!mounted_ && rootNode_ == nullptr) return {};
    for (std::uint32_t index = edges_.Size(); index > 0U; --index)
        DetachEdge(edges_[index - 1U]);
    if (renderer_ != nullptr && rootRender_ != nullptr)
        (void)renderer_->SetRoot(nullptr);
    if (rootLayout_ != nullptr) (void)layout_->SetRoot(nullptr, {});
    if (rootNode_ != nullptr) (void)tree_->SetRoot(nullptr);
    for (Core::Visual* node : nodes_) {
        if (node != nullptr) (void)values_->DetachObject(*node);
    }
    ReleaseStagedContent();
    edges_.Clear();
    nodes_.Clear();
    rootNode_ = nullptr;
    rootLayout_ = nullptr;
    rootRender_ = nullptr;
    mounted_ = false;
    return {};
}

Base::Result<void> XamlVisualTreeHost::DiscardStaged() noexcept {
    if (mounted_) {
        return InvalidVisualTreeState(
            "Mounted XAML visual tree must be unmounted before discarding it");
    }
    ReleaseStagedContent();
    edges_.Clear();
    nodes_.Clear();
    rootNode_ = nullptr;
    rootLayout_ = nullptr;
    rootRender_ = nullptr;
    return {};
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

} // namespace Aero::Markup
