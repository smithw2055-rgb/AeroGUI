#include <Aero/Core/Layout.hpp>

#include <cmath>
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

class FixedElement final : public LayoutElement {
public:
    FixedElement(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId type, Size desired) noexcept
        : LayoutElement(dispatcher, registry, type), desired_(desired) {}

    void SetDesired(Size value) noexcept { desired_ = value; }
    std::uint32_t MeasureCount() const noexcept { return measureCount_; }
    std::uint32_t ArrangeCount() const noexcept { return arrangeCount_; }

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        ++measureCount_;
        return Size{std::min(desired_.width, available.width),
            std::min(desired_.height, available.height)};
    }

    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        ++arrangeCount_;
        return finalSize;
    }

private:
    Size desired_;
    std::uint32_t measureCount_ = 0U;
    std::uint32_t arrangeCount_ = 0U;
};

class VerticalPanel final : public LayoutElement {
public:
    VerticalPanel(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId type) noexcept : LayoutElement(dispatcher, registry, type) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        double width = 0.0;
        double height = 0.0;
        for (LayoutElement* child : LayoutChildren()) {
            Result<void> measured = MeasureChild(*child, available);
            if (!measured) {
                return measured.GetStatus();
            }
            const Size desired = child->DesiredSize();
            width = std::max(width, desired.width);
            height += desired.height;
        }
        return Size{std::min(width, available.width),
            std::min(height, available.height)};
    }

    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        double y = 0.0;
        for (LayoutElement* child : LayoutChildren()) {
            const double height = child->DesiredSize().height;
            Result<void> arranged = ArrangeChild(
                *child, {0.0, y, finalSize.width, height});
            if (!arranged) {
                return arranged.GetStatus();
            }
            y += height;
        }
        return finalSize;
    }
};

struct Fixture final {
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    Dispatcher dispatcher;
    TypeId objectType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId panelType = InvalidTypeId;

    bool Build() {
        const StringView ns("urn:aero");
        objectType = MakeTypeId(ns, StringView("Object"));
        elementType = MakeTypeId(ns, StringView("LayoutElement"));
        panelType = MakeTypeId(ns, StringView("VerticalPanel"));
        CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("LayoutElement"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({ns, StringView("VerticalPanel"), elementType,
            TypeFlags::None, nullptr}));
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        return true;
    }
};

bool TestGeometry() {
    CHECK(IsFinite(Point{1.0, 2.0}));
    CHECK(!IsFinite(Size{INFINITY, 2.0}));
    CHECK(IsValidLayoutRect({0.0, 0.0, 10.0, 20.0}));
    CHECK(!IsValidLayoutSize({-1.0, 1.0}));
    const Size deflated = Deflate({20.0, 15.0}, {2.0, 3.0, 4.0, 5.0});
    CHECK(deflated.width == 14.0 && deflated.height == 7.0);
    const Size inflated = Inflate(deflated, {2.0, 3.0, 4.0, 5.0});
    CHECK(inflated.width == 20.0 && inflated.height == 15.0);
    const Rect intersection = Intersect(
        {0.0, 0.0, 10.0, 10.0}, {5.0, 6.0, 10.0, 10.0});
    CHECK(intersection.x == 5.0 && intersection.y == 6.0);
    CHECK(intersection.width == 5.0 && intersection.height == 4.0);
    CHECK(RoundLayoutValue(1.24, 2.0) == 1.0);
    CHECK(RoundLayoutValue(1.26, 2.0) == 1.5);
    return true;
}

bool TestNestedLayoutAndInvalidation() {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());

    VerticalPanel root(fixture.dispatcher, fixture.properties, fixture.panelType);
    FixedElement first(fixture.dispatcher, fixture.properties,
        fixture.elementType, {30.0, 10.0});
    FixedElement second(fixture.dispatcher, fixture.properties,
        fixture.elementType, {40.0, 15.0});

    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, first));
    CHECK(tree.AttachLogical(root, second));
    CHECK(tree.AttachVisual(root, first));
    CHECK(tree.AttachVisual(root, second));
    CHECK(layout.Attach(root, first));
    CHECK(layout.Attach(root, second));
    CHECK(layout.SetRoot(&root, {100.0, 80.0}));

    Result<std::uint32_t> phase = fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout);
    CHECK(phase);
    CHECK(root.DesiredSize().width == 40.0);
    CHECK(root.DesiredSize().height == 25.0);
    CHECK(root.RenderSize().width == 100.0);
    CHECK(first.LayoutSlot().y == 0.0);
    CHECK(second.LayoutSlot().y == 10.0);
    CHECK(first.RenderSize().width == 100.0);
    CHECK(root.IsMeasureValid() && root.IsArrangeValid());
    CHECK(layout.PassVersion() == 1U);

    const std::uint32_t oldMeasureCount = first.MeasureCount();
    first.SetDesired({50.0, 20.0});
    CHECK(first.InvalidateMeasure());
    CHECK(!first.IsMeasureValid());
    CHECK(!root.IsMeasureValid());
    phase = fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout);
    CHECK(phase);
    CHECK(first.MeasureCount() > oldMeasureCount);
    CHECK(root.DesiredSize().height == 35.0);
    CHECK(second.LayoutSlot().y == 20.0);
    CHECK(layout.Diagnostics().passVersion == 2U);

    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(layout.Detach(root, second));
    CHECK(layout.Detach(root, first));
    CHECK(tree.DetachVisual(root, second));
    CHECK(tree.DetachVisual(root, first));
    CHECK(tree.DetachLogical(root, second));
    CHECK(tree.DetachLogical(root, first));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(first));
    CHECK(values.DetachObject(second));
    CHECK(values.DetachObject(root));
    return true;
}

bool TestRoundingClippingAndValidation() {
    Fixture fixture;
    CHECK(fixture.Build());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());
    FixedElement element(fixture.dispatcher, fixture.properties,
        fixture.elementType, {10.26, 9.74});
    element.SetClipToBounds(true);
    CHECK(element.SetLayoutRounding(true, 2.0));
    CHECK(layout.SetRoot(&element, {10.26, 9.74}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(element.DesiredSize().width == 10.5);
    CHECK(element.DesiredSize().height == 9.5);
    CHECK(element.LayoutSlot().width == 10.5);
    CHECK(element.LayoutClip().width == 10.5);

    Result<void> invalidRoot = layout.SetRoot(&element, {-1.0, 2.0});
    CHECK(!invalidRoot);
    CHECK(invalidRoot.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidDpi = element.SetLayoutRounding(true, 0.0);
    CHECK(!invalidDpi);
    CHECK(invalidDpi.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    return true;
}

} // namespace

int main() {
    if (!TestGeometry()) return 1;
    if (!TestNestedLayoutAndInvalidation()) return 1;
    if (!TestRoundingClippingAndValidation()) return 1;
    std::puts("Aero layout tests passed");
    return 0;
}
