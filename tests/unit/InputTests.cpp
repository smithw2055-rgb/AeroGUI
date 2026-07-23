#include <Aero/Core/Controls.hpp>
#include <Aero/Core/Input.hpp>
#include <Aero/Core/Presentation.hpp>

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
    bool Build() {
        Result<CorePresentationMetadata> registered =
            TryRegisterCorePresentationMetadata(types, properties, &events); CHECK(registered);
        const StringView ns("urn:input");
        objectType=registered.Value().objectType;
        rootType=registered.Value().stackPanelType;
        boxType=MakeTypeId(ns,StringView("Box"));
        CHECK(types.TryRegisterType({ns,StringView("Box"),
            registered.Value().layoutElementType,TypeFlags::None,nullptr}));
        CHECK(types.Freeze()); CHECK(properties.Freeze()); CHECK(events.Freeze()); CHECK(values.Initialize()); CHECK(tree.Initialize()); CHECK(layout.Initialize()); return true;
    }
};

LayoutElement* CastStack(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<StackPanel&>(node)); }
LayoutElement* CastBox(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<Box&>(node)); }
struct PointerLog final { std::uint32_t count=0; std::uint32_t id=0; double x=0; double y=0; };
struct PointerRecorder final {
    PointerLog* log = nullptr;
    void Record(const MouseEventArgs& args) const noexcept {
        ++log->count; log->id=args.pointerId;
        log->x=args.position.x; log->y=args.position.y;
    }
    void operator()(Aero::Base::Object*, const MouseEventArgs& args) const noexcept { Record(args); }
    void operator()(Aero::Base::Object*, const MouseButtonEventArgs& args) const noexcept { Record(args); }
};
struct FocusRecorder final {
    std::uint32_t* count = nullptr;
    void operator()(Aero::Base::Object*,
        const KeyboardFocusChangedEventArgs&) const noexcept { ++*count; }
};
struct KeyboardLog final { std::uint32_t count=0U; std::uint32_t key=0U; std::uint32_t modifiers=0U; bool repeat=false; KeyboardAction action=KeyboardAction::Down; };
struct KeyboardRecorder final {
    KeyboardLog* log = nullptr;
    void operator()(Aero::Base::Object*, const KeyEventArgs& args) const noexcept {
        ++log->count; log->key=args.key; log->modifiers=args.modifiers;
        log->repeat=args.isRepeat; log->action=args.action;
    }
};
struct TextLog final { std::uint32_t count=0U; String text; TextLog():text(&GetDefaultAllocator()){} };
struct TextRecorder final {
    TextLog* log = nullptr;
    void operator()(Aero::Base::Object*,
        const TextCompositionEventArgs& args) const noexcept {
        ++log->count; (void)log->text.TryAssign(args.text);
    }
};

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
    PointerLog log; PointerRecorder pointerRecorder{&log};
    CHECK(root.MouseMove().TryAdd(MouseEventHandler(&pointerRecorder)));
    CHECK(root.MouseDown().TryAdd(MouseButtonEventHandler(&pointerRecorder)));
    CHECK(root.MouseUp().TryAdd(MouseButtonEventHandler(&pointerRecorder)));
    PointerInputManager pointer(hit,f.events,root);
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
    FocusRecorder gotRecorder{&gotCount}; FocusRecorder lostRecorder{&lostCount};
    CHECK(first.GotKeyboardFocus().TryAdd(KeyboardFocusChangedEventHandler(&gotRecorder)));
    CHECK(first.LostKeyboardFocus().TryAdd(KeyboardFocusChangedEventHandler(&lostRecorder)));
    FocusManager focus(f.tree,f.events);
    Result<bool> focused=focus.SetFocus(&first); CHECK(focused && focused.Value() && focus.FocusedNode()==&first && gotCount==1U);
    KeyboardLog keyboardLog; KeyboardRecorder keyboardRecorder{&keyboardLog};
    CHECK(first.KeyDown().TryAdd(KeyEventHandler(&keyboardRecorder)));
    CHECK(first.KeyUp().TryAdd(KeyEventHandler(&keyboardRecorder)));
    KeyboardInputManager keyboard(focus,f.events,f.tree);
    Result<KeyboardDispatchResult> keyDown=keyboard.Dispatch({KeyboardAction::Down,65U,3U,true});
    CHECK(keyDown && keyDown.Value().routed && keyDown.Value().target==&first);
    CHECK(keyboardLog.count==1U && keyboardLog.key==65U && keyboardLog.modifiers==3U && keyboardLog.repeat && keyboardLog.action==KeyboardAction::Down);
    Result<KeyboardDispatchResult> keyUp=keyboard.Dispatch({KeyboardAction::Up,65U,0U,false});
    CHECK(keyUp && keyUp.Value().routed && keyboardLog.count==2U && keyboardLog.action==KeyboardAction::Up);
    Result<KeyboardDispatchResult> invalidKey=keyboard.Dispatch({KeyboardAction::Down,0U,0U,false}); CHECK(!invalidKey && invalidKey.GetStatus().code==ErrorCode::InvalidArgument);
    TextLog textLog; TextRecorder textRecorder{&textLog};
    CHECK(first.TextInput().TryAdd(TextCompositionEventHandler(&textRecorder)));
    TextInputManager textInput(focus,f.events,f.tree);
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
