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
    Core::ObjectTree& tree,
    Core::LayoutManager& layout,
    Core::EffectiveValueEngine& values,
    Core::RenderManager* renderer,
    Base::IAllocator* allocator) noexcept
    : tree_(&tree),
      layout_(&layout),
      values_(&values),
      renderer_(renderer),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      types_(allocator_),
      presenters_(allocator_),
      collections_(allocator_),
      edges_(allocator_),
      nodes_(allocator_) {}

XamlVisualTreeHost::~XamlVisualTreeHost() noexcept {
    AERO_ASSERT(!mounted_);
}

Base::Result<void> XamlVisualTreeHost::TryRegisterType(
    const XamlVisualTreeTypeRegistration& registration) noexcept {
    if (schema_ != nullptr || registration.type == Core::InvalidTypeId ||
        registration.asTreeNode == nullptr || registration.asLayoutElement == nullptr) {
        return InvalidVisualTree("XAML visual-tree type registration is invalid");
    }
    if (FindType(registration.type) != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "XAML visual-tree type is already registered");
    }
    return types_.TryPushBack(registration);
}

Base::Result<void> XamlVisualTreeHost::TryRegisterContentPresenter(
    const XamlContentPresenterRegistration& registration) noexcept {
    if (schema_ != nullptr || registration.type == Core::InvalidTypeId ||
        registration.asPresenter == nullptr) {
        return InvalidVisualTree("XAML ContentPresenter registration is invalid");
    }
    if (FindPresenter(registration.type) != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "XAML ContentPresenter type is already registered");
    }
    return presenters_.TryPushBack(registration);
}

Base::Result<void> XamlVisualTreeHost::TryRegisterCollectionContent(
    const XamlCollectionContentRegistration& registration) noexcept {
    if (schema_ != nullptr || registration.type == Core::InvalidTypeId ||
        registration.member == Core::InvalidMemberId ||
        registration.asStackPanel == nullptr) {
        return InvalidVisualTree("XAML collection-content registration is invalid");
    }
    if (FindCollection(registration.type, registration.member) != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
            "XAML collection-content member is already registered");
    }
    return collections_.TryPushBack(registration);
}

Base::Result<void> XamlVisualTreeHost::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema_ != nullptr || schema.IsFrozen() || !schema.Types().IsFrozen() ||
        (presenters_.Empty() && collections_.Empty())) {
        return InvalidVisualTreeState("XAML visual-tree registries are not ready");
    }
    for (const XamlContentPresenterRegistration& registration : presenters_) {
        const Core::PropertyInfo* content = schema.Types().FindProperty(
            registration.type, Base::StringView("Content"), true);
        if (content == nullptr || schema.FindMemberAdapter(content->Id()) != nullptr) {
            return InvalidVisualTree("ContentPresenter Content metadata is invalid");
        }
        Base::Result<void> typeAdapter = schema.TryRegisterTypeAdapter({
            registration.type, content->Id(), nullptr, nullptr, nullptr, this, false, false,
            nullptr, nullptr});
        if (!typeAdapter) return typeAdapter.GetStatus();
        Base::Result<void> memberAdapter = schema.TryRegisterMemberAdapter({
            content->Id(), XamlMemberWriteMode::SetOnce, nullptr, this,
            &SetContentMember, false});
        if (!memberAdapter) return memberAdapter.GetStatus();
    }
    for (const XamlCollectionContentRegistration& registration : collections_) {
        const Core::PropertyInfo* children = schema.Types().FindProperty(registration.member);
        if (children == nullptr || schema.FindTypeAdapter(registration.type) != nullptr ||
            schema.FindMemberAdapter(registration.member) != nullptr) {
            return InvalidVisualTree("StackPanel collection metadata is invalid");
        }
        Base::Result<void> typeAdapter = schema.TryRegisterTypeAdapter({
            registration.type, registration.member, nullptr, nullptr, nullptr, this, false,
            false, nullptr, nullptr});
        if (!typeAdapter) return typeAdapter.GetStatus();
        Base::Result<void> memberAdapter = schema.TryRegisterMemberAdapter({
            registration.member, XamlMemberWriteMode::Collection, nullptr, this,
            &SetContentMember, false});
        if (!memberAdapter) return memberAdapter.GetStatus();
    }
    schema_ = &schema;
    return {};
}

const XamlVisualTreeTypeRegistration* XamlVisualTreeHost::FindType(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId && schema_ != nullptr) {
        for (const XamlVisualTreeTypeRegistration& registration : types_) {
            if (registration.type == current) return &registration;
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    for (const XamlVisualTreeTypeRegistration& registration : types_) {
        if (registration.type == type) return &registration;
    }
    return nullptr;
}

const XamlContentPresenterRegistration* XamlVisualTreeHost::FindPresenter(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId && schema_ != nullptr) {
        for (const XamlContentPresenterRegistration& registration : presenters_) {
            if (registration.type == current) return &registration;
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

const XamlCollectionContentRegistration* XamlVisualTreeHost::FindCollection(
    Core::TypeId type, Core::MemberId member) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId && schema_ != nullptr) {
        for (const XamlCollectionContentRegistration& registration : collections_) {
            if (registration.type == current && registration.member == member) {
                return &registration;
            }
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    for (const XamlCollectionContentRegistration& registration : collections_) {
        if (registration.type == type && registration.member == member) return &registration;
    }
    return nullptr;
}

Base::Result<Core::TreeNode*> XamlVisualTreeHost::ResolveTreeNode(
    Base::Object& object, Core::TypeId type) const noexcept {
    const XamlVisualTreeTypeRegistration* registration = FindType(type);
    if (registration == nullptr) return InvalidVisualTree("XAML object is not a registered tree node");
    Core::TreeNode* node = registration->asTreeNode(object, registration->context);
    if (node == nullptr || node->RuntimeType() != type) {
        return InvalidVisualTree("XAML tree-node runtime type does not match metadata");
    }
    return node;
}

Base::Result<Core::LayoutElement*> XamlVisualTreeHost::ResolveLayoutElement(
    Base::Object& object, Core::TypeId type) const noexcept {
    const XamlVisualTreeTypeRegistration* registration = FindType(type);
    if (registration == nullptr) return InvalidVisualTree("XAML object is not a registered layout element");
    Core::LayoutElement* element = registration->asLayoutElement(object, registration->context);
    if (element == nullptr || element->RuntimeType() != type) {
        return InvalidVisualTree("XAML layout-element runtime type does not match metadata");
    }
    return element;
}

Core::RenderElement* XamlVisualTreeHost::ResolveRenderElement(
    Base::Object& object, Core::TypeId type) const noexcept {
    const XamlVisualTreeTypeRegistration* registration = FindType(type);
    if (registration == nullptr || registration->asRenderElement == nullptr) return nullptr;
    Core::RenderElement* element = registration->asRenderElement(object, registration->context);
    return element != nullptr && element->RuntimeType() == type ? element : nullptr;
}

Base::Result<void> XamlVisualTreeHost::AddNode(Core::TreeNode& node) noexcept {
    for (Core::TreeNode* existing : nodes_) {
        if (existing == &node) return {};
    }
    return nodes_.TryPushBack(&node);
}

Base::Result<void> XamlVisualTreeHost::StageContent(
    Base::Object& object, const XamlValue& value,
    const XamlServiceProvider& services) noexcept {
    if (mounted_ || services.targetObject != &object || value.Kind() != XamlValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return InvalidVisualTreeState("XAML Content requires a non-null object before mount");
    }
    Base::Result<Core::LayoutElement*> parentResult = ResolveLayoutElement(
        object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();
    Core::ContentPresenter* presenter = nullptr;
    Core::StackPanel* stackPanel = nullptr;
    const XamlContentPresenterRegistration* presenterRegistration = FindPresenter(
        services.targetObjectType);
    if (presenterRegistration != nullptr) {
        presenter = presenterRegistration->asPresenter(object, presenterRegistration->context);
        if (presenter == nullptr || presenter != parentResult.Value()) {
            return InvalidVisualTree("XAML ContentPresenter runtime type does not match metadata");
        }
    } else {
        const XamlCollectionContentRegistration* collection = FindCollection(
            services.targetObjectType, services.targetMember);
        if (collection == nullptr) {
            return InvalidVisualTree("XAML content target is not a registered collection owner");
        }
        stackPanel = collection->asStackPanel(object, collection->context);
        if (stackPanel == nullptr || stackPanel != parentResult.Value()) {
            return InvalidVisualTree("XAML StackPanel runtime type does not match metadata");
        }
    }
    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Core::LayoutElement*> childResult = ResolveLayoutElement(
        *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();

    Base::Result<void> reserveEdges = edges_.TryReserve(edges_.Size() + 1U);
    if (!reserveEdges) return reserveEdges;
    Base::Result<Core::TreeNode*> parentNode = ResolveTreeNode(
        object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<void> parentAdded = AddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded;
    Base::Result<Core::TreeNode*> childNode = ResolveTreeNode(*childObject, value.Type());
    if (!childNode) return childNode.GetStatus();
    Base::Result<void> childAdded = AddNode(*childNode.Value());
    if (!childAdded) return childAdded;
    Base::Result<void> contentSet = presenter != nullptr
        ? presenter->SetOwnedContent(value.AsObject(), *childResult.Value())
        : stackPanel->AddOwnedChild(value.AsObject(), *childResult.Value());
    if (!contentSet) return contentSet;
    return edges_.TryPushBack({parentResult.Value(), childResult.Value(), presenter, stackPanel});
}

Base::Result<void> XamlVisualTreeHost::AttachEdge(Edge& edge) noexcept {
    Base::Result<void> logical = tree_->AttachLogical(*edge.parent, *edge.child);
    if (!logical) return logical;
    edge.logicalAttached = true;
    Base::Result<void> visual = tree_->AttachVisual(*edge.parent, *edge.child);
    if (!visual) { DetachEdge(edge); return visual; }
    edge.visualAttached = true;
    Base::Result<void> layout = layout_->Attach(*edge.parent, *edge.child);
    if (!layout) { DetachEdge(edge); return layout; }
    edge.layoutAttached = true;
    if (renderer_ != nullptr) {
        Core::RenderElement* parentRender = ResolveRenderElement(
            *static_cast<Base::Object*>(edge.parent), edge.parent->RuntimeType());
        Core::RenderElement* childRender = ResolveRenderElement(
            *static_cast<Base::Object*>(edge.child), edge.child->RuntimeType());
        if (parentRender != nullptr && childRender != nullptr) {
            Base::Result<void> render = renderer_->Attach(*parentRender, *childRender);
            if (!render) { DetachEdge(edge); return render; }
            edge.renderAttached = true;
        }
    }
    return {};
}

void XamlVisualTreeHost::DetachEdge(Edge& edge) noexcept {
    if (edge.renderAttached) {
        Core::RenderElement* parent = ResolveRenderElement(
            *static_cast<Base::Object*>(edge.parent), edge.parent->RuntimeType());
        Core::RenderElement* child = ResolveRenderElement(
            *static_cast<Base::Object*>(edge.child), edge.child->RuntimeType());
        if (parent != nullptr && child != nullptr) (void)renderer_->Detach(*parent, *child);
        edge.renderAttached = false;
    }
    if (edge.layoutAttached) { (void)layout_->Detach(*edge.parent, *edge.child); edge.layoutAttached = false; }
    if (edge.visualAttached) { (void)tree_->DetachVisual(*edge.parent, *edge.child); edge.visualAttached = false; }
    if (edge.logicalAttached) { (void)tree_->DetachLogical(*edge.parent, *edge.child); edge.logicalAttached = false; }
}

Base::Result<void> XamlVisualTreeHost::Mount(
    Base::Object& root, Core::TypeId rootType, Core::Size availableSize) noexcept {
    if (mounted_ || schema_ == nullptr || !Core::IsValidLayoutSize(availableSize)) {
        return InvalidVisualTreeState("XAML visual tree cannot mount in its current state");
    }
    Base::Result<Core::TreeNode*> rootNode = ResolveTreeNode(root, rootType);
    if (!rootNode) return rootNode.GetStatus();
    Base::Result<Core::LayoutElement*> rootLayout = ResolveLayoutElement(root, rootType);
    if (!rootLayout) return rootLayout.GetStatus();
    Base::Result<void> added = AddNode(*rootNode.Value());
    if (!added) return added;
    rootNode_ = rootNode.Value();
    rootLayout_ = rootLayout.Value();
    rootRender_ = ResolveRenderElement(root, rootType);
    Base::Result<void> treeRoot = tree_->SetRoot(rootNode_);
    if (!treeRoot) return treeRoot;
    Base::Result<void> layoutRoot = layout_->SetRoot(rootLayout_, availableSize);
    if (!layoutRoot) { (void)tree_->SetRoot(nullptr); return layoutRoot; }
    if (renderer_ != nullptr && rootRender_ != nullptr) {
        Base::Result<void> renderRoot = renderer_->SetRoot(rootRender_);
        if (!renderRoot) { (void)layout_->SetRoot(nullptr, {}); (void)tree_->SetRoot(nullptr); return renderRoot; }
    }
    std::uint32_t attached = 0U;
    while (attached < edges_.Size()) {
        bool progressed = false;
        for (Edge& edge : edges_) {
            if (edge.logicalAttached || edge.parent->OwningTree() != tree_) continue;
            Base::Result<void> result = AttachEdge(edge);
            if (!result) { (void)Unmount(); return result; }
            ++attached;
            progressed = true;
        }
        if (!progressed) { (void)Unmount(); return InvalidVisualTreeState("XAML content graph is disconnected from its root"); }
    }
    mounted_ = true;
    return {};
}

Base::Result<void> XamlVisualTreeHost::Unmount() noexcept {
    if (!mounted_ && rootNode_ == nullptr) return {};
    for (std::uint32_t index = edges_.Size(); index > 0U; --index) {
        Edge& edge = edges_[index - 1U];
        DetachEdge(edge);
    }
    if (renderer_ != nullptr && rootRender_ != nullptr) (void)renderer_->SetRoot(nullptr);
    if (rootLayout_ != nullptr) (void)layout_->SetRoot(nullptr, {});
    if (rootNode_ != nullptr) (void)tree_->SetRoot(nullptr);
    // XAML containers keep their child objects alive. Detach all non-owning
    // effective-value entries before releasing those strong references.
    for (Core::TreeNode* node : nodes_) {
        if (node != nullptr) (void)values_->DetachObject(*node);
    }
    for (std::uint32_t index = 0U; index < edges_.Size(); ++index) {
        Edge& edge = edges_[index];
        if (edge.presenter != nullptr) (void)edge.presenter->SetContent(nullptr);
        if (edge.stackPanel != nullptr) {
            bool seen = false;
            for (std::uint32_t prior = 0U; prior < index; ++prior) {
                seen = seen || edges_[prior].stackPanel == edge.stackPanel;
            }
            if (!seen) (void)edge.stackPanel->ClearOwnedChildren();
        }
    }
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
    for (std::uint32_t index = 0U; index < edges_.Size(); ++index) {
        Edge& edge = edges_[index];
        if (edge.presenter != nullptr) (void)edge.presenter->SetContent(nullptr);
        if (edge.stackPanel != nullptr) {
            bool seen = false;
            for (std::uint32_t prior = 0U; prior < index; ++prior) {
                seen = seen || edges_[prior].stackPanel == edge.stackPanel;
            }
            if (!seen) (void)edge.stackPanel->ClearOwnedChildren();
        }
    }
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
    if (host == nullptr) return InvalidVisualTreeState("XAML visual-tree host is unavailable");
    return host->StageContent(object, value, services);
}

Base::Result<Base::Ref<Base::Object>> LoadXamlVisualTreeWithActivation(
    XamlVisualTreeHost& host,
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
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
