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
    RoutedEventHandle moved; RoutedEventHandle pressed; RoutedEventHandle released; RoutedEventHandle gotFocus; RoutedEventHandle lostFocus; RoutedEventHandle keyDown; RoutedEventHandle keyUp; RoutedEventHandle textInput;
    bool Build() {
        const StringView ns("urn:input"); objectType=MakeTypeId(ns,StringView("Object"));
        rootType=MakeTypeId(ns,StringView("StackPanel")); boxType=MakeTypeId(ns,StringView("Box"));
        CHECK(types.TryRegisterType({ns,StringView("Object"),InvalidTypeId,TypeFlags::None,nullptr}));
        CHECK(types.TryRegisterType({ns,StringView("StackPanel"),objectType,TypeFlags::None,nullptr}));
        CHECK(types.TryRegisterType({ns,StringView("Box"),objectType,TypeFlags::None,nullptr}));
        Result<RoutedEventHandle> move = events.TryRegister({StringView("PointerMove"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(move); moved=move.Value();
        Result<RoutedEventHandle> down = events.TryRegister({StringView("PointerDown"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(down); pressed=down.Value();
        Result<RoutedEventHandle> up = events.TryRegister({StringView("PointerUp"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(up); released=up.Value();
        Result<RoutedEventHandle> got = events.TryRegister({StringView("GotFocus"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(got); gotFocus=got.Value();
        Result<RoutedEventHandle> lost = events.TryRegister({StringView("LostFocus"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(lost); lostFocus=lost.Value();
        Result<RoutedEventHandle> downKey = events.TryRegister({StringView("KeyDown"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(downKey); keyDown=downKey.Value();
        Result<RoutedEventHandle> upKey = events.TryRegister({StringView("KeyUp"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(upKey); keyUp=upKey.Value();
        Result<RoutedEventHandle> text = events.TryRegister({StringView("TextInput"),rootType,objectType,RoutingStrategy::Bubble}); CHECK(text); textInput=text.Value();
        CHECK(types.Freeze()); CHECK(properties.Freeze()); CHECK(events.Freeze()); CHECK(values.Initialize()); CHECK(tree.Initialize()); CHECK(layout.Initialize()); return true;
    }
};

LayoutElement* CastStack(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<StackPanel&>(node)); }
LayoutElement* CastBox(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<Box&>(node)); }
struct PointerLog final { std::uint32_t count=0; std::uint32_t id=0; double x=0; double y=0; };
void OnPointer(TreeNode&, RoutedEventArgs& args, void* context) noexcept { auto* log=static_cast<PointerLog*>(context); if (log != nullptr && args.hasPointer) { ++log->count; log->id=args.pointerId; log->x=args.pointerX; log->y=args.pointerY; } }
void OnFocus(TreeNode&, RoutedEventArgs&, void* context) noexcept { ++*static_cast<std::uint32_t*>(context); }
struct KeyboardLog final { std::uint32_t count=0U; std::uint32_t key=0U; std::uint32_t modifiers=0U; bool repeat=false; KeyboardAction action=KeyboardAction::Down; };
void OnKeyboard(TreeNode&, RoutedEventArgs& args, void* context) noexcept { auto* log=static_cast<KeyboardLog*>(context); if (log != nullptr && args.hasKeyboard) { ++log->count; log->key=args.key; log->modifiers=args.modifiers; log->repeat=args.isRepeat; log->action=args.keyboardAction; } }
struct TextLog final { std::uint32_t count=0U; String text; TextLog():text(&GetDefaultAllocator()){} };
void OnText(TreeNode&, RoutedEventArgs& args, void* context) noexcept { auto* log=static_cast<TextLog*>(context); if (log != nullptr && args.hasTextInput) { ++log->count; (void)log->text.TryAssign(args.text); } }

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
    CHECK(second.SetHitTestVisible(false));
    Result<HitTestResult> hiddenHit=hit.HitTest(root,{10,35}); CHECK(hiddenHit && hiddenHit.Value().target==&root);
    CHECK(second.SetHitTestVisible(true));
    Result<HitTestResult> miss=hit.HitTest(root,{110,10}); CHECK(miss && !miss.Value().HasTarget());
    Result<HitTestResult> invalid=hit.HitTest(root,{INFINITY,0}); CHECK(!invalid && invalid.GetStatus().code==ErrorCode::InvalidArgument);
    PointerLog log; CHECK(root.AddHandler(f.moved,&OnPointer,&log)); CHECK(root.AddHandler(f.pressed,&OnPointer,&log)); CHECK(root.AddHandler(f.released,&OnPointer,&log));
    PointerInputManager pointer(hit,f.events,root,{f.moved,f.pressed,f.released});
    Result<PointerDispatchResult> dispatched=pointer.Dispatch({7U,PointerAction::Down,{10,35}});
    CHECK(dispatched && dispatched.Value().routed && dispatched.Value().hit.target==&second);
    CHECK(log.count==1U && log.id==7U && log.x==10.0 && log.y==5.0);
    CHECK(pointer.CapturePointer(9U,first));
    CHECK(pointer.CapturedNode(9U)==&first);
    Result<PointerDispatchResult> captured=pointer.Dispatch({9U,PointerAction::Move,{10,35}});
    CHECK(captured && captured.Value().routed && captured.Value().hit.target==&first);
    CHECK(log.count==2U && log.id==9U && log.x==10.0 && log.y==35.0);
    CHECK(pointer.Dispatch({9U,PointerAction::Up,{10,35}}));
    CHECK(pointer.CapturedNode(9U)==nullptr);
    Result<PointerDispatchResult> released=pointer.Dispatch({9U,PointerAction::Move,{10,35}});
    CHECK(released && released.Value().routed && released.Value().hit.target==&second);
    CHECK(log.count==4U && log.id==9U && log.x==10.0 && log.y==5.0);
    Result<bool> noCapture=pointer.ReleasePointer(9U); CHECK(noCapture && !noCapture.Value());
    ErrorCode workerCode = ErrorCode::Ok;
    std::thread worker([&]() {
        Result<PointerDispatchResult> wrongThread = pointer.Dispatch(
            {8U, PointerAction::Move, {10.0, 10.0}});
        workerCode = wrongThread ? ErrorCode::Ok : wrongThread.GetStatus().code;
    });
    worker.join();
    CHECK(workerCode == ErrorCode::WrongThread);
    std::uint32_t gotCount=0U; std::uint32_t lostCount=0U;
    CHECK(first.AddHandler(f.gotFocus,&OnFocus,&gotCount)); CHECK(first.AddHandler(f.lostFocus,&OnFocus,&lostCount));
    FocusManager focus(f.tree,f.events,{f.gotFocus,f.lostFocus});
    Result<bool> focused=focus.SetFocus(&first); CHECK(focused && focused.Value() && focus.FocusedNode()==&first && gotCount==1U);
    KeyboardLog keyboardLog; CHECK(first.AddHandler(f.keyDown,&OnKeyboard,&keyboardLog)); CHECK(first.AddHandler(f.keyUp,&OnKeyboard,&keyboardLog));
    KeyboardInputManager keyboard(focus,f.events,f.tree,{f.keyDown,f.keyUp});
    Result<KeyboardDispatchResult> keyDown=keyboard.Dispatch({KeyboardAction::Down,65U,3U,true});
    CHECK(keyDown && keyDown.Value().routed && keyDown.Value().target==&first);
    CHECK(keyboardLog.count==1U && keyboardLog.key==65U && keyboardLog.modifiers==3U && keyboardLog.repeat && keyboardLog.action==KeyboardAction::Down);
    Result<KeyboardDispatchResult> keyUp=keyboard.Dispatch({KeyboardAction::Up,65U,0U,false});
    CHECK(keyUp && keyUp.Value().routed && keyboardLog.count==2U && keyboardLog.action==KeyboardAction::Up);
    Result<KeyboardDispatchResult> invalidKey=keyboard.Dispatch({KeyboardAction::Down,0U,0U,false}); CHECK(!invalidKey && invalidKey.GetStatus().code==ErrorCode::InvalidArgument);
    TextLog textLog; CHECK(first.AddHandler(f.textInput,&OnText,&textLog));
    TextInputManager textInput(focus,f.events,f.tree,{f.textInput});
    Result<TextInputDispatchResult> text= textInput.Dispatch({StringView(u8"A中")});
    CHECK(text && text.Value().routed && text.Value().target==&first && textLog.count==1U && textLog.text.View()==StringView(u8"A中"));
    const char malformedBytes[] = {static_cast<char>(0xC0), static_cast<char>(0xAF)};
    Result<TextInputDispatchResult> malformed=textInput.Dispatch({StringView(malformedBytes,2U)});
    CHECK(!malformed && malformed.GetStatus().code==ErrorCode::InvalidArgument);
    Result<bool> cleared=focus.ClearFocus(); CHECK(cleared && cleared.Value() && focus.FocusedNode()==nullptr && lostCount==1U);
    Result<KeyboardDispatchResult> unfocused=keyboard.Dispatch({KeyboardAction::Down,65U,0U,false}); CHECK(unfocused && !unfocused.Value().routed && unfocused.Value().target==nullptr);
    Result<TextInputDispatchResult> unfocusedText=textInput.Dispatch({StringView("text")}); CHECK(unfocusedText && !unfocusedText.Value().routed && unfocusedText.Value().target==nullptr);
    CHECK(f.layout.SetRoot(nullptr,{0,0})); for (LayoutElement* child : {static_cast<LayoutElement*>(&second),static_cast<LayoutElement*>(&first)}) { CHECK(f.layout.Detach(root,*child)); CHECK(f.tree.DetachVisual(root,*child)); CHECK(f.tree.DetachLogical(root,*child)); CHECK(f.values.DetachObject(*child)); } CHECK(f.tree.SetRoot(nullptr)); CHECK(f.values.DetachObject(root)); return true;
}
}
int main(){ if(!TestVisualHitTesting()) return 1; std::puts("Aero input tests passed"); return 0; }
