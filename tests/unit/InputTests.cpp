#include <Aero/Core/Controls.hpp>
#include <Aero/Core/Input.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

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
    EffectiveValueEngine values{dispatcher, properties}; ObjectTree tree{dispatcher, values};
    LayoutManager layout{dispatcher}; TypeId objectType; TypeId rootType; TypeId boxType;
    bool Build() {
        const StringView ns("urn:input"); objectType=MakeTypeId(ns,StringView("Object"));
        rootType=MakeTypeId(ns,StringView("StackPanel")); boxType=MakeTypeId(ns,StringView("Box"));
        CHECK(types.TryRegisterType({ns,StringView("Object"),InvalidTypeId,TypeFlags::None,nullptr}));
        CHECK(types.TryRegisterType({ns,StringView("StackPanel"),objectType,TypeFlags::None,nullptr}));
        CHECK(types.TryRegisterType({ns,StringView("Box"),objectType,TypeFlags::None,nullptr}));
        CHECK(types.Freeze()); CHECK(properties.Freeze()); CHECK(values.Initialize()); CHECK(tree.Initialize()); CHECK(layout.Initialize()); return true;
    }
};

LayoutElement* CastStack(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<StackPanel&>(node)); }
LayoutElement* CastBox(TreeNode& node, void*) noexcept { return static_cast<LayoutElement*>(&static_cast<Box&>(node)); }

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
    CHECK(f.layout.SetRoot(nullptr,{0,0})); for (LayoutElement* child : {static_cast<LayoutElement*>(&second),static_cast<LayoutElement*>(&first)}) { CHECK(f.layout.Detach(root,*child)); CHECK(f.tree.DetachVisual(root,*child)); CHECK(f.tree.DetachLogical(root,*child)); CHECK(f.values.DetachObject(*child)); } CHECK(f.tree.SetRoot(nullptr)); CHECK(f.values.DetachObject(root)); return true;
}
}
int main(){ if(!TestVisualHitTesting()) return 1; std::puts("Aero input tests passed"); return 0; }
