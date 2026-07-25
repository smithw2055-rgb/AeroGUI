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
    : tree_(&tree), layout_(&layout), values_(&values), renderer_(renderer),
      mounts_(tree, &layout, renderer), rootMount_(),
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
    if (schema_ != nullptr || schema.IsFrozen() ||
        !schema.Descriptors().IsSealed() ||
        (singles_.Empty() && collections_.Empty())) {
        return InvalidVisualTreeState("XAML visual-tree registries are not ready");
    }

    for (const XamlSingleContentRegistration& registration : singles_) {
        for (const Core::MetadataTypeDescriptor& type :
             schema.Descriptors().Types()) {
            if (!schema.Descriptors().IsDerivedFrom(
                    type.Id(), registration.type)) continue;
            const Core::MemberId member =
                schema.Facets().FindContentMember(type.Id());
            const Core::MetadataPropertyDescriptor* content =
                schema.Descriptors().FindProperty(member);
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
        const Core::MetadataPropertyDescriptor* baseMember =
            schema.Descriptors().FindProperty(registration.member);
        if (baseMember == nullptr) {
            return InvalidVisualTree("Collection-content metadata is invalid");
        }
        for (const Core::MetadataTypeDescriptor& type :
             schema.Descriptors().Types()) {
            if (!schema.Descriptors().IsDerivedFrom(
                    type.Id(), registration.type)) continue;
            const Core::MemberId member =
                schema.Facets().FindContentMember(type.Id());
            const Core::MetadataPropertyDescriptor* children =
                schema.Descriptors().FindProperty(member);
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
        const Core::MetadataTypeDescriptor* info =
            schema_->Descriptors().FindType(current);
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
        const Core::MetadataTypeDescriptor* info =
            schema_->Descriptors().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

const XamlCollectionContentRegistration* XamlVisualTreeHost::FindCollection(
    Core::TypeId type, Core::MemberId member) const noexcept {
    const Core::MetadataPropertyDescriptor* actual = schema_ != nullptr
        ? schema_->Descriptors().FindProperty(member) : nullptr;
    for (const XamlCollectionContentRegistration& registration : collections_) {
        if (schema_ == nullptr) {
            if (registration.type == type && registration.member == member)
                return &registration;
            continue;
        }
        if (!schema_->Descriptors().IsDerivedFrom(
                type, registration.type)) continue;
        const Core::MetadataPropertyDescriptor* base =
            schema_->Descriptors().FindProperty(registration.member);
        if (actual != nullptr && base != nullptr && actual->Name() == base->Name())
            return &registration;
    }
    return nullptr;
}

Base::Result<Presentation::Visual*> XamlVisualTreeHost::ResolveVisual(
    Base::Object& object, Core::TypeId type) const noexcept {
    const XamlVisualTreeTypeRegistration* registration = FindType(type);
    if (registration == nullptr)
        return InvalidVisualTree("XAML object is not a registered Visual");
    Presentation::Visual* node = registration->asVisual(object, registration->context);
    if (node == nullptr || node->RuntimeType() != type)
        return InvalidVisualTree("XAML Visual runtime type does not match metadata");
    return node;
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

Base::Result<void> XamlVisualTreeHost::AddNode(Presentation::Visual& node) noexcept {
    for (Presentation::Visual* existing : nodes_) if (existing == &node) return {};
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
    Base::Result<Presentation::UIElement*> parentResult = ResolveUIElement(
        object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();
    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Presentation::UIElement*> childResult = ResolveUIElement(
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
    Base::Result<Presentation::Visual*> parentNode = ResolveVisual(
        object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<void> parentAdded = AddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<Presentation::Visual*> childNode = ResolveVisual(*childObject, value.Type());
    if (!childNode) return childNode.GetStatus();
    Base::Result<void> childAdded = AddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();
    return edges_.TryPushBack({std::move(parentOwner), value.AsObject(),
        parentResult.Value(), childResult.Value(), clearSingle, clearCollection,
        configure, contentContext});
}

Base::Result<void> XamlVisualTreeHost::AttachEdge(Edge& edge) noexcept {
    Base::Result<Presentation::MountEdgeState> mounted =
        mounts_.Attach(*edge.parent, *edge.child);
    if (!mounted) return mounted.GetStatus();
    edge.mount = std::move(mounted).Value();

    if (edge.configureCollectionChild != nullptr) {
        Base::Result<void> configured =
            edge.configureCollectionChild(
                *edge.parentOwner.Get(),
                *edge.parent,
                *edge.child,
                edge.contentContext);
        if (!configured) {
            (void)mounts_.Detach(edge.mount);
            return configured.GetStatus();
        }
    }
    return {};
}

void XamlVisualTreeHost::DetachEdge(Edge& edge) noexcept {
    (void)mounts_.Detach(edge.mount);
}

Base::Result<void> XamlVisualTreeHost::Mount(
    Base::Object& root,
    Core::TypeId rootType,
    Presentation::Size availableSize) noexcept {
    if (mounted_ || schema_ == nullptr ||
        !Presentation::IsValidLayoutSize(availableSize)) {
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

    rootNode_ = rootNode.Value();
    rootLayout_ = rootLayout.Value();
    rootRender_ = ResolveFrameworkElement(root, rootType);

    Base::Result<Presentation::MountRootState> rootMounted =
        mounts_.AttachRoot(*rootNode_, availableSize);
    if (!rootMounted) {
        rootNode_ = nullptr;
        rootLayout_ = nullptr;
        rootRender_ = nullptr;
        return rootMounted.GetStatus();
    }
    rootMount_ = std::move(rootMounted).Value();

    std::uint32_t attached = 0U;
    while (attached < edges_.Size()) {
        bool progressed = false;
        for (Edge& edge : edges_) {
            if (edge.mount.logicalAttached ||
                edge.parent->OwningTree() != tree_) {
                continue;
            }
            Base::Result<void> result = AttachEdge(edge);
            if (!result) {
                (void)Unmount();
                return result.GetStatus();
            }
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

    Base::Status firstError;
    for (std::uint32_t index = edges_.Size(); index > 0U; --index) {
        Base::Result<void> detached =
            mounts_.Detach(edges_[index - 1U].mount);
        if (!detached && firstError.IsOk()) {
            firstError = detached.GetStatus();
        }
    }
    Base::Result<void> rootDetached = mounts_.DetachRoot(rootMount_);
    if (!rootDetached && firstError.IsOk()) {
        firstError = rootDetached.GetStatus();
    }

    for (Presentation::Visual* node : nodes_) {
        if (node != nullptr) (void)values_->DetachObject(*node);
    }
    ReleaseStagedContent();
    edges_.Clear();
    nodes_.Clear();
    rootNode_ = nullptr;
    rootLayout_ = nullptr;
    rootRender_ = nullptr;
    rootMount_ = {};
    mounted_ = false;
    return firstError.IsOk()
        ? Base::Result<void>()
        : Base::Result<void>(firstError);
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

} // namespace Aero::Markup
