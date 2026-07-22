#include <Aero/Core/Controls.hpp>
#include <Aero/Core/Input.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

namespace {
using namespace Aero::Base;
using namespace Aero::Core;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK failed %d: %s\n", __LINE__, #x); return false; } } while (false)

class Box final : public LayoutElement {
public:
    Box(Dispatcher& d, DependencyPropertyRegistry& p, TypeId t, Size desired) noexcept
        : LayoutElement(d, p, t), desired_(desired) {}
protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        return Size{std::min(desired_.width, available.width),
            std::min(desired_.height, available.height)};
    }
    Result<Size> ArrangeOverride(Size size) noexcept override { return size; }
private: Size desired_;
};

struct Fixture final {
    Dispatcher dispatcher; TypeRegistry types; DependencyPropertyRegistry properties{types};
    RoutedEventRegistry events{types};
    EffectiveValueEngine values{dispatcher, properties}; ObjectTree tree{dispatcher, values};
    LayoutManager layout{dispatcher}; TypeId objectType; TypeId rootType; TypeId boxType;
    RoutedEventHandle moved; RoutedEventHandle pressed; RoutedEventHandle released;
    bool Build() {
        const StringView ns("urn:input"); objectType=MakeTypeId(ns,StringView("Object"));
        rootType=MakeTypeId(ns,StringView("StackPanel")); boxType=MakeTypeId(ns,StringView("Box"));
        CHECK(types.TryRegisterType({ns,StringView("Object"),InvalidTypeId,TypeFlags::None,nullptr}));
        CHECK(types.TryRegisterType({ns,StringView("StackPanel"),objectType,TypeFlags::None,nullptr}));
        CHECK(types.TryRegisterType({ns,StringView("Box"),objectType,TypeFlags::None,nullptr}));
        Result<RoutedEventHandle> move = events.TryRegister({StringView("PointerMove"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(move); moved=move.Value();
        Result<RoutedEventHandle> down = events.TryRegister({StringView("PointerDown"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(down); pressed=down.Value();
        Result<RoutedEventHandle> up = events.TryRegister({StringView("PointerUp"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(up); released=up.Value();
        CHECK(types.Freeze()); CHECK(properties.Freeze()); CHECK(events.Freeze()); CHECK(values.Initialize()); CHECK(tree.Initialize()); CHECK(layout.Initialize()); return true;
    }
};

LayoutElement* CastStack(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<StackPanel&>(node)); }
LayoutElement* CastBox(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<Box&>(node)); }
struct PointerLog final { std::uint32_t count=0; std::uint32_t id=0; double x=0; double y=0; };
void OnPointer(TreeNode&, RoutedEventArgs& args, void* context) noexcept { auto* log=static_cast<PointerLog*>(context); if (log != nullptr && args.hasPointer) { ++log->count; log->id=args.pointerId; log->x=args.pointerX; log->y=args.pointerY; } }

bool TestVisualHitTesting() {
    Fixture f; CHECK(f.Build());
    StackPanel root(f.dispatcher,f.properties,f.rootType,Orientation::Vertical);
    Box first(f.dispatcher,f.properties,f.boxType,{100,30});
    Box second(f.dispatcher,f.properties,f.boxType,{100,20});
    CHECK(f.tree.SetRoot(&root));
    for (LayoutElement* child : {static_cast<LayoutElement*>(&first),static_cast<LayoutElement*>(&second)}) {
        CHECK(f.tree.AttachLogical(root,*child)); CHECK(f.tree.AttachVisual(root,*child)); CHECK(f.layout.Attach(root,*child)); }
    CHECK(f.layout.SetRoot(&root,{100,80})); CHECK(f.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    HitTestManager hit(f.types); CHECK(hit.TryRegisterType({f.rootType,&CastStack,nullptr})); CHECK(hit.TryRegisterType({f.boxType,&CastBox,nullptr}));
    Result<HitTestResult> firstHit=hit.HitTest(root,{10,10}); CHECK(firstHit && firstHit.Value().target==&first);
    Result<HitTestResult> secondHit=hit.HitTest(root,{10,35}); CHECK(secondHit && secondHit.Value().target==&second);
    Result<HitTestResult> miss=hit.HitTest(root,{110,10}); CHECK(miss && !miss.Value().HasTarget());
    Result<HitTestResult> invalid=hit.HitTest(root,{INFINITY,0}); CHECK(!invalid && invalid.GetStatus().code==ErrorCode::InvalidArgument);
    PointerLog log; CHECK(root.AddHandler(f.pressed,&OnPointer,&log));
    PointerInputManager pointer(hit,f.events,root,{f.moved,f.pressed,f.released});
    Result<PointerDispatchResult> dispatched=pointer.Dispatch({7U,PointerAction::Down,{10,35}});
    CHECK(dispatched && dispatched.Value().routed && dispatched.Value().hit.target==&second);
    CHECK(log.count==1U && log.id==7U && log.x==10.0 && log.y==5.0);
    ErrorCode workerCode = ErrorCode::Ok;
    std::thread worker([&]() {
        Result<PointerDispatchResult> wrongThread = pointer.Dispatch(
            {8U, PointerAction::Move, {10.0, 10.0}});
        workerCode = wrongThread ? ErrorCode::Ok : wrongThread.GetStatus().code;
    });
    worker.join();
    CHECK(workerCode == ErrorCode::WrongThread);
    CHECK(f.layout.SetRoot(nullptr,{0,0})); for (LayoutElement* child : {static_cast<LayoutElement*>(&second),static_cast<LayoutElement*>(&first)}) { CHECK(f.layout.Detach(root,*child)); CHECK(f.tree.DetachVisual(root,*child)); CHECK(f.tree.DetachLogical(root,*child)); CHECK(f.values.DetachObject(*child)); } CHECK(f.tree.SetRoot(nullptr)); CHECK(f.values.DetachObject(root)); return true;
}
}
int main(){ if(!TestVisualHitTesting()) return 1; std::puts("Aero input tests passed"); return 0; }
