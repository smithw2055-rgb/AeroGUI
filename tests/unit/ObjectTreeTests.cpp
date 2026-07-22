#include <Aero/Core/ObjectTree.hpp>

#include <cstdint>
#include <cstdio>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

struct RouteLog final {
    TreeNode* nodes[32]{};
    std::uint32_t count = 0U;
    bool markHandled = false;
};

void RecordRoute(
    TreeNode& sender,
    RoutedEventArgs& args,
    void* context) noexcept {
    RouteLog* log = static_cast<RouteLog*>(context);
    if (log->count < 32U) {
        log->nodes[log->count++] = &sender;
    }
    if (log->markHandled) {
        args.handled = true;
    }
}

struct LifecycleLog final {
    TreeNode* nodes[64]{};
    bool loaded[64]{};
    std::uint32_t count = 0U;
};

void RecordLifecycle(
    const TreeLifecycleEvent& event,
    void* context) noexcept {
    LifecycleLog* log = static_cast<LifecycleLog*>(context);
    if (log->count < 64U) {
        log->nodes[log->count] = event.node;
        log->loaded[log->count] = event.loaded;
        ++log->count;
    }
}

struct Fixture final {
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    RoutedEventRegistry events{types};
    Dispatcher dispatcher;

    TypeId objectType = InvalidTypeId;
    TypeId eventArgsType = InvalidTypeId;
    TypeId nodeType = InvalidTypeId;
    TypeId controlType = InvalidTypeId;
    RoutedEventHandle bubble;
    RoutedEventHandle tunnel;
    RoutedEventHandle direct;

    bool Build() {
        const StringView ns("urn:aero");
        objectType = MakeTypeId(ns, StringView("Object"));
        eventArgsType = MakeTypeId(ns, StringView("RoutedEventArgs"));
        nodeType = MakeTypeId(ns, StringView("TreeNode"));
        controlType = MakeTypeId(ns, StringView("Control"));

        CHECK(types.TryRegisterType({
            ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("RoutedEventArgs"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("TreeNode"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Control"), nodeType,
            TypeFlags::None, nullptr}));

        Result<RoutedEventHandle> bubbleResult = events.TryRegister({
            StringView("Click"), nodeType, eventArgsType,
            RoutingStrategy::Bubble});
        CHECK(bubbleResult);
        bubble = bubbleResult.Value();

        Result<RoutedEventHandle> tunnelResult = events.TryRegister({
            StringView("PreviewClick"), nodeType, eventArgsType,
            RoutingStrategy::Tunnel});
        CHECK(tunnelResult);
        tunnel = tunnelResult.Value();

        Result<RoutedEventHandle> directResult = events.TryRegister({
            StringView("Activated"), nodeType, eventArgsType,
            RoutingStrategy::Direct});
        CHECK(directResult);
        direct = directResult.Value();

        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(events.Freeze());
        return true;
    }
};

bool TestLogicalVisualTreeAndLifecycle() {
    Fixture fixture;
    CHECK(fixture.Build());

    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());

    TreeNode root(fixture.dispatcher, fixture.properties, fixture.nodeType);
    TreeNode child(fixture.dispatcher, fixture.properties, fixture.controlType);
    TreeNode leaf(fixture.dispatcher, fixture.properties, fixture.controlType);

    LifecycleLog lifecycle;
    tree.SetLifecycleHandler(&RecordLifecycle, &lifecycle);

    CHECK(tree.SetRoot(&root));
    Result<TreeNodeHandle> rootHandle = tree.GetHandle(root);
    CHECK(rootHandle && tree.ResolveHandle(rootHandle.Value()) == &root);
    CHECK(root.IsLoaded());
    CHECK(tree.AttachLogical(root, child));
    Result<TreeNodeHandle> childHandle = tree.GetHandle(child);
    CHECK(childHandle && tree.ResolveHandle(childHandle.Value()) == &child);
    CHECK(tree.AttachLogical(child, leaf));
    CHECK(child.LogicalParent() == &root);
    CHECK(leaf.LogicalParent() == &child);
    CHECK(tree.AttachVisual(root, child));
    CHECK(tree.AttachVisual(child, leaf));
    CHECK(child.VisualParent() == &root);
    CHECK(leaf.VisualParent() == &child);
    CHECK(tree.Version() == 5U);

    Result<std::uint32_t> lifecyclePhase = fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle);
    CHECK(lifecyclePhase);
    CHECK(lifecycle.count >= 3U);

    Result<void> logicalCycle = tree.AttachLogical(leaf, root);
    CHECK(!logicalCycle);
    CHECK(logicalCycle.GetStatus().code == ErrorCode::CycleDetected);
    Result<void> visualCycle = tree.AttachVisual(leaf, root);
    CHECK(!visualCycle);
    CHECK(visualCycle.GetStatus().code == ErrorCode::CycleDetected);

    CHECK(tree.DetachVisual(child, leaf));
    CHECK(tree.DetachVisual(root, child));
    CHECK(tree.DetachLogical(child, leaf));
    CHECK(tree.DetachLogical(root, child));
    CHECK(tree.ResolveHandle(childHandle.Value()) == nullptr);
    CHECK(tree.SetRoot(nullptr));
    CHECK(tree.ResolveHandle(rootHandle.Value()) == nullptr);
    CHECK(!root.IsLoaded());
    CHECK(!child.IsLoaded());
    CHECK(!leaf.IsLoaded());
    CHECK(values.DetachObject(root));
    CHECK(values.DetachObject(child));
    CHECK(values.DetachObject(leaf));

    lifecyclePhase = fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle);
    CHECK(lifecyclePhase);
    CHECK(lifecycle.count >= 6U);
    return true;
}

bool TestBubbleTunnelDirectAndHandledEventsToo() {
    Fixture fixture;
    CHECK(fixture.Build());

    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());

    TreeNode root(fixture.dispatcher, fixture.properties, fixture.nodeType);
    TreeNode child(fixture.dispatcher, fixture.properties, fixture.controlType);
    TreeNode leaf(fixture.dispatcher, fixture.properties, fixture.controlType);
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, child));
    CHECK(tree.AttachLogical(child, leaf));

    RouteLog bubbleLog;
    CHECK(root.AddHandler(fixture.bubble, &RecordRoute, &bubbleLog));
    CHECK(child.AddHandler(fixture.bubble, &RecordRoute, &bubbleLog));
    CHECK(leaf.AddHandler(fixture.bubble, &RecordRoute, &bubbleLog));
    CHECK(fixture.events.RaiseEvent(leaf, fixture.bubble));
    CHECK(bubbleLog.count == 3U);
    CHECK(bubbleLog.nodes[0] == &leaf);
    CHECK(bubbleLog.nodes[1] == &child);
    CHECK(bubbleLog.nodes[2] == &root);

    RouteLog tunnelLog;
    CHECK(root.AddHandler(fixture.tunnel, &RecordRoute, &tunnelLog));
    CHECK(child.AddHandler(fixture.tunnel, &RecordRoute, &tunnelLog));
    CHECK(leaf.AddHandler(fixture.tunnel, &RecordRoute, &tunnelLog));
    CHECK(fixture.events.RaiseEvent(leaf, fixture.tunnel));
    CHECK(tunnelLog.count == 3U);
    CHECK(tunnelLog.nodes[0] == &root);
    CHECK(tunnelLog.nodes[1] == &child);
    CHECK(tunnelLog.nodes[2] == &leaf);

    RouteLog directLog;
    CHECK(root.AddHandler(fixture.direct, &RecordRoute, &directLog));
    CHECK(leaf.AddHandler(fixture.direct, &RecordRoute, &directLog));
    CHECK(fixture.events.RaiseEvent(leaf, fixture.direct));
    CHECK(directLog.count == 1U);
    CHECK(directLog.nodes[0] == &leaf);

    RouteLog handled;
    handled.markHandled = true;
    RouteLog observed;
    CHECK(leaf.AddHandler(fixture.bubble, &RecordRoute, &handled));
    CHECK(root.AddHandler(
        fixture.bubble, &RecordRoute, &observed, nullptr, true));
    CHECK(fixture.events.RaiseEvent(leaf, fixture.bubble));
    CHECK(handled.count == 1U);
    CHECK(observed.count == 1U);
    CHECK(observed.nodes[0] == &root);

    CHECK(tree.DetachLogical(child, leaf));
    CHECK(tree.DetachLogical(root, child));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(root));
    CHECK(values.DetachObject(child));
    CHECK(values.DetachObject(leaf));
    return true;
}

bool TestClassHandlersAndRegistrationRules() {
    Fixture fixture;
    CHECK(fixture.Build());

    RouteLog classLog;
    CHECK(fixture.events.RegisterClassHandler(
        fixture.bubble,
        fixture.nodeType,
        &RecordRoute,
        &classLog));

    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    TreeNode root(fixture.dispatcher, fixture.properties, fixture.nodeType);
    TreeNode child(fixture.dispatcher, fixture.properties, fixture.controlType);
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, child));

    CHECK(fixture.events.RaiseEvent(child, fixture.bubble));
    CHECK(classLog.count == 2U);
    CHECK(classLog.nodes[0] == &child);
    CHECK(classLog.nodes[1] == &root);

    Result<RoutedEventHandle> late = fixture.events.TryRegister({
        StringView("Late"), fixture.nodeType, fixture.eventArgsType,
        RoutingStrategy::Bubble});
    CHECK(!late);
    CHECK(late.GetStatus().code == ErrorCode::InvalidState);

    CHECK(tree.DetachLogical(root, child));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(root));
    CHECK(values.DetachObject(child));
    return true;
}

} // namespace

int main() {
    const struct TestCase {
        const char* name;
        bool (*run)();
    } tests[] = {
        {"logical-visual-tree-lifecycle", &TestLogicalVisualTreeAndLifecycle},
        {"bubble-tunnel-direct-handled", &TestBubbleTunnelDirectAndHandledEventsToo},
        {"class-handlers-registration", &TestClassHandlersAndRegistrationRules}
    };

    for (const TestCase& test : tests) {
        if (!test.run()) {
            std::fprintf(stderr, "FAILED: %s\n", test.name);
            return 1;
        }
    }
    return 0;
}
