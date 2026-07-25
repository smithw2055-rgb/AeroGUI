from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_section(text: str, start: str, end: str, replacement: str, label: str) -> str:
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"{label}: start marker not found")
    finish = text.find(end, begin)
    if finish < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:begin] + replacement.rstrip() + "\n\n" + text[finish:]


# Base: acquire a strong reference only when the object is reference-managed.
path = "include/Aero/Base/Ref.hpp"
text = read(path)
text = replace_once(
    text,
    """    static Ref FromBorrowed(T& value) noexcept {\n        value.AddRef();\n        return Ref(&value, Detail::AdoptRef);\n    }\n""",
    """    static Ref FromBorrowed(T& value) noexcept {\n        value.AddRef();\n        return Ref(&value, Detail::AdoptRef);\n    }\n\n    // Returns an empty reference for stack/embedded Objects that do not have an\n    // intrusive control block. This lets snapshot code strongly retain managed\n    // objects while remaining source-compatible with stack-based test hosts.\n    static Ref TryFromBorrowed(T& value) noexcept {\n        Detail::ObjectControlBlock* control =\n            static_cast<Object&>(value).ControlBlock();\n        if (control == nullptr || !Detail::TryAddStrong(control)) {\n            return {};\n        }\n        return Ref(&value, Detail::AdoptRef);\n    }\n""",
    "Ref::TryFromBorrowed",
)
write(path, text)


# ObjectTree public/private representation.
path = "include/Aero/Presentation/ObjectTree.hpp"
text = read(path)
text = replace_once(
    text,
    "#include <Aero/Base/Result.hpp>\n",
    "#include <Aero/Base/Result.hpp>\n#include <Aero/Base/Ref.hpp>\n",
    "ObjectTree Ref include",
)
text = replace_once(
    text,
    """namespace Detail {\n\nclass RoutedHandlerStorage final {\n""",
    """namespace Detail {\n\n// A separately allocated lifetime cell is used for stack/embedded Visuals that\n// cannot participate in intrusive ownership. Managed Visuals are retained by a\n// strong Ref; unmanaged Visuals invalidate this cell from their destructor.\nclass VisualLifetime final : public Base::Object {\npublic:\n    explicit VisualLifetime(Visual& node) noexcept : node_(&node) {}\n    ~VisualLifetime() override = default;\n\n    Visual* Node() const noexcept { return node_; }\n    void Invalidate() noexcept { node_ = nullptr; }\n\nprivate:\n    Visual* node_ = nullptr;\n};\n\nstruct VisualLease;\n\nclass RoutedHandlerStorage final {\n""",
    "Visual lifetime cell",
)
text = replace_once(
    text,
    """private:\n    friend class ObjectTree;\n    ObjectTree* tree_ = nullptr;\n""",
    """private:\n    friend class ObjectTree;\n    friend class RoutedEventManager;\n    friend struct Detail::VisualLease;\n\n    Base::Result<Base::Ref<Detail::VisualLifetime>>\n    AcquireLifetime() noexcept;\n\n    ObjectTree* tree_ = nullptr;\n""",
    "Visual lifetime friends",
)
text = replace_once(
    text,
    """    VisualHandle handle_;\n\n};\n\nclass AERO_API ObjectTree final {\n""",
    """    VisualHandle handle_;\n    Base::Ref<Detail::VisualLifetime> lifetime_;\n\n};\n\nnamespace Detail {\n\nstruct VisualLease final {\n    Base::Ref<Visual> strong;\n    Base::Ref<VisualLifetime> lifetime;\n\n    static Base::Result<VisualLease> Acquire(Visual& node) noexcept;\n    Visual* Resolve() const noexcept {\n        return strong ? strong.Get()\n                      : (lifetime ? lifetime->Node() : nullptr);\n    }\n};\n\n} // namespace Detail\n\nclass AERO_API ObjectTree final {\n""",
    "Visual lease",
)
text = replace_once(
    text,
    """    struct LifecycleRecord final {\n        Visual* node = nullptr;\n        bool loaded = false;\n""",
    """    struct LifecycleRecord final {\n        Detail::VisualLease node;\n        bool loaded = false;\n""",
    "Lifecycle lease record",
)
text = replace_once(
    text,
    """    Base::Result<void> QueueLifecycleSubtree(\n        Visual& node,\n        bool loaded) noexcept;\n    Base::Result<void> SetLoadedSubtree(\n        Visual& node,\n        bool loaded) noexcept;\n""",
    """    Base::Result<void> CollectLogicalSubtree(\n        Visual& node,\n        Base::Vector<Visual*>& nodes) noexcept;\n    Base::Result<void> StageLifecycleSubtree(\n        Visual& node,\n        bool loaded,\n        Base::Vector<LifecycleRecord>& staged) noexcept;\n    void PublishLifecycle(\n        Base::Vector<LifecycleRecord>& staged) noexcept;\n    void ApplyLoadedSubtree(Visual& node, bool loaded) noexcept;\n    void SetTreeSubtree(Visual& node, ObjectTree* tree) noexcept;\n""",
    "ObjectTree staged lifecycle declarations",
)
text = replace_once(
    text,
    """    Base::Result<void> BuildRoute(\n        Visual& source,\n        RoutingStrategy strategy,\n        Base::Vector<Visual*>& route) noexcept;\n""",
    """    Base::Result<void> BuildRoute(\n        Visual& source,\n        RoutingStrategy strategy,\n        Base::Vector<Detail::VisualLease>& route) noexcept;\n""",
    "RoutedEvent lease route",
)
write(path, text)


# ObjectTree transactional implementation.
path = "src/presentation/ObjectTree.cpp"
text = read(path)
text = replace_once(
    text,
    """Visual::~Visual() {\n    AERO_ASSERT(tree_ == nullptr);\n    AERO_ASSERT(logicalParent_ == nullptr);\n    AERO_ASSERT(visualParent_ == nullptr);\n    AERO_ASSERT(logicalChildren_.Empty());\n    AERO_ASSERT(visualChildren_.Empty());\n}\n""",
    """Visual::~Visual() {\n    AERO_ASSERT(tree_ == nullptr);\n    AERO_ASSERT(logicalParent_ == nullptr);\n    AERO_ASSERT(visualParent_ == nullptr);\n    AERO_ASSERT(logicalChildren_.Empty());\n    AERO_ASSERT(visualChildren_.Empty());\n    if (lifetime_) lifetime_->Invalidate();\n}\n\nBase::Result<Base::Ref<Detail::VisualLifetime>>\nVisual::AcquireLifetime() noexcept {\n    if (!lifetime_) {\n        Base::Result<Base::Ref<Detail::VisualLifetime>> created =\n            Base::MakeRef<Detail::VisualLifetime>(*this);\n        if (!created) return created.GetStatus();\n        lifetime_ = std::move(created).Value();\n    }\n    return lifetime_;\n}\n\nBase::Result<Detail::VisualLease> Detail::VisualLease::Acquire(\n    Visual& node) noexcept {\n    VisualLease lease;\n    lease.strong = Base::Ref<Visual>::TryFromBorrowed(node);\n    if (lease.strong) return lease;\n\n    Base::Result<Base::Ref<VisualLifetime>> lifetime =\n        node.AcquireLifetime();\n    if (!lifetime) return lifetime.GetStatus();\n    lease.lifetime = std::move(lifetime).Value();\n    return lease;\n}\n""",
    "Visual lease implementation",
)
text = replace_section(
    text,
    "Base::Result<void> ObjectTree::RegisterHandleSubtree",
    "void ObjectTree::InvalidateHandleSubtree",
    """Base::Result<void> ObjectTree::CollectLogicalSubtree(\n    Visual& node,\n    Base::Vector<Visual*>& nodes) noexcept {\n    Base::Result<void> appended = nodes.TryPushBack(&node);\n    if (!appended) return appended.GetStatus();\n    for (Visual* child : node.logicalChildren_) {\n        if (child == nullptr) continue;\n        Base::Result<void> collected =\n            CollectLogicalSubtree(*child, nodes);\n        if (!collected) return collected.GetStatus();\n    }\n    return {};\n}\n\nBase::Result<void> ObjectTree::RegisterHandleSubtree(Visual& node) noexcept {\n    Base::Vector<Visual*> nodes;\n    Base::Result<void> collected = CollectLogicalSubtree(node, nodes);\n    if (!collected) return collected.GetStatus();\n\n    std::uint32_t required = 0U;\n    for (Visual* current : nodes) {\n        if (current->handle_.IsValid()) {\n            if (current->handle_.index >= handles_.Size()) {\n                return InvalidState("ObjectTree node has an invalid pre-existing handle");\n            }\n            const HandleEntry& entry = handles_[current->handle_.index];\n            if (entry.node != current ||\n                entry.generation != current->handle_.generation) {\n                return InvalidState("ObjectTree node has a stale pre-existing handle");\n            }\n        } else {\n            ++required;\n        }\n    }\n\n    Base::Result<void> reserved =\n        handles_.TryReserve(handles_.Size() + required);\n    if (!reserved) return reserved.GetStatus();\n\n    Base::Vector<Visual*> added;\n    reserved = added.TryReserve(required);\n    if (!reserved) return reserved.GetStatus();\n\n    for (Visual* current : nodes) {\n        if (current->handle_.IsValid()) continue;\n\n        HandleEntry entry;\n        entry.node = current;\n        Base::Result<void> appended = handles_.TryPushBack(entry);\n        AERO_ASSERT(appended);\n        current->handle_ = {handles_.Size() - 1U, entry.generation};\n\n        Base::Result<void> tracked = TrackInheritedValues(*current);\n        if (!tracked) {\n            current->handle_ = {};\n            handles_.PopBack();\n            while (!added.Empty()) {\n                Visual* rollback = added.Back();\n                UntrackInheritedValues(*rollback);\n                rollback->handle_ = {};\n                handles_.PopBack();\n                added.PopBack();\n            }\n            return tracked.GetStatus();\n        }\n        Base::Result<void> remembered = added.TryPushBack(current);\n        AERO_ASSERT(remembered);\n    }\n    return {};\n}\n""",
    "atomic handle registration",
)
text = replace_section(
    text,
    "Base::Result<void> ObjectTree::QueueLifecycleSubtree",
    "Base::Result<void> ObjectTree::SetRoot",
    """Base::Result<void> ObjectTree::StageLifecycleSubtree(\n    Visual& node,\n    bool loaded,\n    Base::Vector<LifecycleRecord>& staged) noexcept {\n    if (node.loaded_ != loaded) {\n        Base::Result<Detail::VisualLease> lease =\n            Detail::VisualLease::Acquire(node);\n        if (!lease) return lease.GetStatus();\n\n        LifecycleRecord record;\n        record.node = std::move(lease).Value();\n        record.loaded = loaded;\n        Base::Result<void> appended =\n            staged.TryPushBack(std::move(record));\n        if (!appended) return appended.GetStatus();\n    }\n    for (Visual* child : node.logicalChildren_) {\n        if (child == nullptr) continue;\n        Base::Result<void> childResult =\n            StageLifecycleSubtree(*child, loaded, staged);\n        if (!childResult) return childResult.GetStatus();\n    }\n    return {};\n}\n\nvoid ObjectTree::PublishLifecycle(\n    Base::Vector<LifecycleRecord>& staged) noexcept {\n    for (LifecycleRecord& record : staged) {\n        record.sequence = nextLifecycleSequence_++;\n        record.treeVersion = version_;\n        Base::Result<void> appended =\n            lifecycleQueue_.TryPushBack(std::move(record));\n        AERO_ASSERT(appended);\n    }\n    staged.Clear();\n}\n\nvoid ObjectTree::ApplyLoadedSubtree(Visual& node, bool loaded) noexcept {\n    node.loaded_ = loaded;\n    for (Visual* child : node.logicalChildren_) {\n        if (child != nullptr) ApplyLoadedSubtree(*child, loaded);\n    }\n}\n\nvoid ObjectTree::SetTreeSubtree(\n    Visual& node, ObjectTree* tree) noexcept {\n    node.tree_ = tree;\n    for (Visual* child : node.logicalChildren_) {\n        if (child != nullptr) SetTreeSubtree(*child, tree);\n    }\n}\n""",
    "staged lifecycle implementation",
)
text = replace_section(
    text,
    "Base::Result<void> ObjectTree::SetRoot",
    "Base::Result<void> ObjectTree::AttachLogical",
    """Base::Result<void> ObjectTree::SetRoot(Visual* root) noexcept {\n    if (root == root_) return {};\n    if (root == nullptr && root_ == nullptr) return {};\n\n    Visual& verificationNode = root != nullptr ? *root : *root_;\n    Base::Result<void> verified = VerifyMutation(verificationNode, root_);\n    if (!verified) return verified.GetStatus();\n\n    if (root != nullptr &&\n        (root->logicalParent_ != nullptr ||\n         root->visualParent_ != nullptr ||\n         root->tree_ != nullptr)) {\n        return InvalidState("ObjectTree root must be fully detached");\n    }\n\n    Base::Vector<LifecycleRecord> staged;\n    if (root_ != nullptr) {\n        Base::Result<void> prepared =\n            StageLifecycleSubtree(*root_, false, staged);\n        if (!prepared) return prepared.GetStatus();\n    }\n    if (root != nullptr) {\n        Base::Result<void> prepared =\n            StageLifecycleSubtree(*root, true, staged);\n        if (!prepared) return prepared.GetStatus();\n    }\n    Base::Result<void> queueReserved = lifecycleQueue_.TryReserve(\n        lifecycleQueue_.Size() + staged.Size());\n    if (!queueReserved) return queueReserved.GetStatus();\n\n    if (root != nullptr) {\n        Base::Result<void> registered = RegisterHandleSubtree(*root);\n        if (!registered) return registered.GetStatus();\n        Base::Result<void> inherited =\n            values_->SetInheritanceParent(*root, nullptr);\n        if (!inherited) {\n            InvalidateHandleSubtree(*root);\n            return inherited.GetStatus();\n        }\n    }\n\n    mutating_ = true;\n    Visual* oldRoot = root_;\n    ++version_;\n    if (oldRoot != nullptr) {\n        ApplyLoadedSubtree(*oldRoot, false);\n        InvalidateHandleSubtree(*oldRoot);\n        SetTreeSubtree(*oldRoot, nullptr);\n    }\n\n    root_ = root;\n    if (root_ != nullptr) {\n        SetTreeSubtree(*root_, this);\n        ApplyLoadedSubtree(*root_, true);\n    }\n    PublishLifecycle(staged);\n    mutating_ = false;\n    return {};\n}\n""",
    "atomic SetRoot",
)
text = replace_section(
    text,
    "Base::Result<void> ObjectTree::AttachLogical",
    "void ObjectTree::RemoveChild",
    """Base::Result<void> ObjectTree::AttachLogical(\n    Visual& parent,\n    Visual& child) noexcept {\n    Base::Result<void> verified = VerifyMutation(parent, &child);\n    if (!verified) return verified.GetStatus();\n    if (&parent == &child || IsLogicalAncestor(child, parent)) {\n        return Base::Status::Failure(\n            Base::ErrorCode::CycleDetected,\n            "Logical tree attachment would create a cycle");\n    }\n    if (child.logicalParent_ != nullptr || child.tree_ != nullptr ||\n        parent.tree_ != this) {\n        return InvalidState(\n            "Logical child must be detached and parent must belong to this tree");\n    }\n\n    Base::Result<void> childReserved = parent.logicalChildren_.TryReserve(\n        parent.logicalChildren_.Size() + 1U);\n    if (!childReserved) return childReserved.GetStatus();\n\n    Base::Vector<LifecycleRecord> staged;\n    if (parent.loaded_) {\n        Base::Result<void> prepared =\n            StageLifecycleSubtree(child, true, staged);\n        if (!prepared) return prepared.GetStatus();\n    }\n    Base::Result<void> queueReserved = lifecycleQueue_.TryReserve(\n        lifecycleQueue_.Size() + staged.Size());\n    if (!queueReserved) return queueReserved.GetStatus();\n\n    Base::Result<void> registered = RegisterHandleSubtree(child);\n    if (!registered) return registered.GetStatus();\n    Base::Result<void> inherited =\n        values_->SetInheritanceParent(child, &parent);\n    if (!inherited) {\n        InvalidateHandleSubtree(child);\n        return inherited.GetStatus();\n    }\n\n    mutating_ = true;\n    Base::Result<void> appended =\n        parent.logicalChildren_.TryPushBack(&child);\n    AERO_ASSERT(appended);\n    child.logicalParent_ = &parent;\n    SetTreeSubtree(child, this);\n    ++version_;\n    if (parent.loaded_) ApplyLoadedSubtree(child, true);\n    PublishLifecycle(staged);\n    mutating_ = false;\n    return {};\n}\n""",
    "atomic AttachLogical",
)
text = replace_section(
    text,
    "Base::Result<void> ObjectTree::DetachLogical",
    "Base::Result<void> ObjectTree::AttachVisual",
    """Base::Result<void> ObjectTree::DetachLogical(\n    Visual& parent,\n    Visual& child) noexcept {\n    Base::Result<void> verified = VerifyMutation(parent, &child);\n    if (!verified) return verified.GetStatus();\n    if (child.logicalParent_ != &parent || child.tree_ != this) {\n        return NotFound("Logical parent-child relationship was not found");\n    }\n\n    Base::Vector<LifecycleRecord> staged;\n    if (child.loaded_) {\n        Base::Result<void> prepared =\n            StageLifecycleSubtree(child, false, staged);\n        if (!prepared) return prepared.GetStatus();\n    }\n    Base::Result<void> queueReserved = lifecycleQueue_.TryReserve(\n        lifecycleQueue_.Size() + staged.Size());\n    if (!queueReserved) return queueReserved.GetStatus();\n\n    Base::Result<void> inherited =\n        values_->SetInheritanceParent(child, nullptr);\n    if (!inherited) return inherited.GetStatus();\n\n    mutating_ = true;\n    if (child.loaded_) ApplyLoadedSubtree(child, false);\n    RemoveChild(parent.logicalChildren_, child);\n    child.logicalParent_ = nullptr;\n    SetTreeSubtree(child, nullptr);\n    InvalidateHandleSubtree(child);\n    ++version_;\n    PublishLifecycle(staged);\n    mutating_ = false;\n    return {};\n}\n""",
    "atomic DetachLogical",
)
text = replace_section(
    text,
    "Base::Result<std::uint32_t> ObjectTree::FlushLifecycle",
    "void ObjectTree::LifecycleHook",
    """Base::Result<std::uint32_t> ObjectTree::FlushLifecycle() noexcept {\n    Base::Result<void> access = dispatcher_->VerifyAccess();\n    if (!access) return access.GetStatus();\n\n    Base::Vector<LifecycleRecord> snapshot;\n    Base::Result<void> assigned = snapshot.TryAssign(\n        Base::Span<const LifecycleRecord>(\n            lifecycleQueue_.Data(), lifecycleQueue_.Size()));\n    if (!assigned) return assigned.GetStatus();\n    lifecycleQueue_.Clear();\n\n    std::uint32_t count = 0U;\n    for (const LifecycleRecord& record : snapshot) {\n        Visual* node = record.node.Resolve();\n        if (node == nullptr) continue;\n        if (lifecycleHandler_ != nullptr) {\n            const ObjectTreeLifecycleEvent event{\n                node, record.loaded, record.treeVersion};\n            lifecycleHandler_(event, lifecycleContext_);\n        }\n        ++count;\n    }\n    return count;\n}\n""",
    "safe lifecycle flush",
)
text = replace_section(
    text,
    "Base::Result<void> RoutedEventManager::BuildRoute",
    "void RoutedEventManager::InvokeNode",
    """Base::Result<void> RoutedEventManager::BuildRoute(\n    Visual& source,\n    RoutingStrategy strategy,\n    Base::Vector<Detail::VisualLease>& route) noexcept {\n    if (strategy == RoutingStrategy::Direct) {\n        Base::Result<Detail::VisualLease> lease =\n            Detail::VisualLease::Acquire(source);\n        if (!lease) return lease.GetStatus();\n        return route.TryPushBack(std::move(lease).Value());\n    }\n\n    Visual* current = &source;\n    while (current != nullptr) {\n        Base::Result<Detail::VisualLease> lease =\n            Detail::VisualLease::Acquire(*current);\n        if (!lease) return lease.GetStatus();\n        Base::Result<void> appended =\n            route.TryPushBack(std::move(lease).Value());\n        if (!appended) return appended.GetStatus();\n        current = current->LogicalParent() != nullptr\n            ? current->LogicalParent()\n            : current->VisualParent();\n    }\n    if (strategy == RoutingStrategy::Tunnel) {\n        for (std::uint32_t left = 0U, right = route.Size() - 1U;\n             left < right; ++left, --right) {\n            Detail::VisualLease temporary = std::move(route[left]);\n            route[left] = std::move(route[right]);\n            route[right] = std::move(temporary);\n        }\n    }\n    return {};\n}\n""",
    "safe routed-event route",
)
text = replace_once(
    text,
    """    Base::Vector<Visual*> route;\n""",
    """    Base::Vector<Detail::VisualLease> route;\n""",
    "RaiseEvent lease vector",
)
text = replace_once(
    text,
    """    for (Visual* node : route) {\n        InvokeNode(*node, args);\n    }\n""",
    """    for (const Detail::VisualLease& lease : route) {\n        Visual* node = lease.Resolve();\n        if (node != nullptr) InvokeNode(*node, args);\n    }\n""",
    "RaiseEvent safe loop",
)
write(path, text)


# K1 regression coverage.
path = "tests/presentation/ObjectTreeTests.cpp"
text = read(path)
insert = r'''

class ManagedElement final : public UIElement {
public:
    ManagedElement(TypeId type, std::uint32_t* destroyed) noexcept
        : UIElement(type), destroyed_(destroyed) {}
    ~ManagedElement() override {
        if (destroyed_ != nullptr) ++*destroyed_;
    }
private:
    std::uint32_t* destroyed_ = nullptr;
};

struct RouteTeardownContext final {
    ObjectTree* tree = nullptr;
    EffectiveValueEngine* values = nullptr;
    Ref<ManagedElement>* root = nullptr;
    Ref<ManagedElement>* child = nullptr;
    Ref<ManagedElement>* leaf = nullptr;
    RouteLog* log = nullptr;
    bool tornDown = false;
};

struct TearDownDuringRoute final {
    RouteTeardownContext* context = nullptr;
    void operator()(Object* sender, const RoutedEventArgs&) const noexcept {
        if (context->log->count < 32U) {
            context->log->nodes[context->log->count++] =
                static_cast<Visual*>(sender);
        }
        if (context->tornDown) return;
        context->tornDown = true;
        (void)context->tree->DetachLogical(
            **context->child, **context->leaf);
        (void)context->tree->DetachLogical(
            **context->root, **context->child);
        (void)context->tree->SetRoot(nullptr);
        (void)context->values->DetachObject(**context->root);
        (void)context->values->DetachObject(**context->child);
        (void)context->values->DetachObject(**context->leaf);
        context->root->Reset();
        context->child->Reset();
        context->leaf->Reset();
    }
};

bool TestLifecycleLeaseSkipsDestroyedStackVisual() {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LifecycleLog lifecycle;
    tree.SetLifecycleHandler(&RecordLifecycle, &lifecycle);

    {
        Visual transient(fixture.visualType);
        CHECK(tree.SetRoot(&transient));
        CHECK(tree.SetRoot(nullptr));
        CHECK(values.DetachObject(transient));
    }

    Result<std::uint32_t> phase = fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle);
    CHECK(phase);
    CHECK(lifecycle.count == 0U);
    return true;
}

bool TestRouteSnapshotRetainsManagedNodesDuringTeardown() {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());

    std::uint32_t destroyed = 0U;
    Result<Ref<ManagedElement>> rootMade =
        MakeRef<ManagedElement>(fixture.elementType, &destroyed);
    Result<Ref<ManagedElement>> childMade =
        MakeRef<ManagedElement>(fixture.controlType, &destroyed);
    Result<Ref<ManagedElement>> leafMade =
        MakeRef<ManagedElement>(fixture.controlType, &destroyed);
    CHECK(rootMade && childMade && leafMade);
    Ref<ManagedElement> root = std::move(rootMade).Value();
    Ref<ManagedElement> child = std::move(childMade).Value();
    Ref<ManagedElement> leaf = std::move(leafMade).Value();

    CHECK(tree.SetRoot(root.Get()));
    CHECK(tree.AttachLogical(*root, *child));
    CHECK(tree.AttachLogical(*child, *leaf));

    RouteLog log;
    RouteTeardownContext context{
        &tree, &values, &root, &child, &leaf, &log, false};
    CHECK(leaf->TryAddHandler(
        fixture.bubble,
        RoutedEventHandler(TearDownDuringRoute{&context})));
    CHECK(child->TryAddHandler(
        fixture.bubble,
        RoutedEventHandler(RouteRecorder{&log})));
    CHECK(root->TryAddHandler(
        fixture.bubble,
        RoutedEventHandler(RouteRecorder{&log})));

    CHECK(fixture.events.RaiseEvent(*leaf, fixture.bubble));
    CHECK(log.count == 3U);
    CHECK(!root && !child && !leaf);
    CHECK(destroyed == 0U);

    Result<std::uint32_t> phase = fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle);
    CHECK(phase);
    CHECK(destroyed == 3U);
    return true;
}
'''
text = replace_once(
    text,
    "\n} // namespace\n\nint main() {\n",
    insert + "\n} // namespace\n\nint main() {\n",
    "ObjectTree K1 tests insertion",
)
text = replace_once(
    text,
    """        {"class-handlers-registration", &TestClassHandlersAndRegistrationRules}\n""",
    """        {"class-handlers-registration", &TestClassHandlersAndRegistrationRules},\n        {"lifecycle-lease-destroyed-stack", &TestLifecycleLeaseSkipsDestroyedStackVisual},\n        {"route-snapshot-managed-teardown", &TestRouteSnapshotRetainsManagedNodesDuringTeardown}\n""",
    "ObjectTree K1 test list",
)
write(path, text)
