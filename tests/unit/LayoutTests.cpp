#include <Aero/Core/Layout.hpp>
#include <Aero/Core/Controls.hpp>

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

    StackPanel root(fixture.dispatcher, fixture.properties, fixture.panelType);
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

    CHECK(root.SetOrientation(Orientation::Horizontal));
    phase = fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout);
    CHECK(phase);
    CHECK(first.LayoutSlot().x == 0.0);
    CHECK(second.LayoutSlot().x == 50.0);
    CHECK(first.LayoutSlot().height == 80.0);
    CHECK(second.LayoutSlot().height == 80.0);

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
    CHECK(element.SetClipToBounds(true));
    CHECK(element.SetLayoutRounding(true, 2.0));
    CHECK(layout.SetRoot(&element, {10.26, 9.74}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(element.DesiredSize().width == 10.5);
    CHECK(element.DesiredSize().height == 9.5);
    CHECK(element.LayoutSlot().width == 10.5);
    CHECK(element.LayoutClip().width == 10.5);
    CHECK(element.SetClipToBounds(false));
    CHECK(!element.IsArrangeValid());
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(element.LayoutClip().width == element.RenderSize().width);

    Result<void> invalidRoot = layout.SetRoot(&element, {-1.0, 2.0});
    CHECK(!invalidRoot);
    CHECK(invalidRoot.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidDpi = element.SetLayoutRounding(true, 0.0);
    CHECK(!invalidDpi);
    CHECK(invalidDpi.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    return true;
}

bool TestCanvasChildPosition() {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());
    Canvas root(fixture.dispatcher, fixture.properties, fixture.panelType);
    FixedElement child(fixture.dispatcher, fixture.properties,
        fixture.elementType, {12.0, 7.0});
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, child));
    CHECK(tree.AttachVisual(root, child));
    CHECK(layout.Attach(root, child));
    CHECK(root.SetChildPosition(child, {8.0, 9.0}));
    CHECK(layout.SetRoot(&root, {100.0, 80.0}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root.DesiredSize().width == 20.0 && root.DesiredSize().height == 16.0);
    CHECK(child.LayoutSlot().x == 8.0 && child.LayoutSlot().y == 9.0);
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(layout.Detach(root, child));
    CHECK(tree.DetachVisual(root, child));
    CHECK(tree.DetachLogical(root, child));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(child));
    CHECK(values.DetachObject(root));
    return true;
}

bool TestBorderPaddingDecoratorLayout() {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());
    Border root(fixture.dispatcher, fixture.properties, fixture.panelType);
    FixedElement child(fixture.dispatcher, fixture.properties,
        fixture.elementType, {30.0, 12.0});
    CHECK(root.SetPadding({4.0, 3.0, 6.0, 5.0}));
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, child));
    CHECK(tree.AttachVisual(root, child));
    CHECK(layout.Attach(root, child));
    CHECK(layout.SetRoot(&root, {100.0, 80.0}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root.DesiredSize().width == 40.0 && root.DesiredSize().height == 20.0);
    CHECK(child.LayoutSlot().x == 4.0 && child.LayoutSlot().y == 3.0);
    CHECK(child.LayoutSlot().width == 90.0 && child.LayoutSlot().height == 72.0);
    Result<void> invalid = root.SetPadding({0.0, -1.0, 0.0, 0.0});
    CHECK(!invalid && invalid.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(layout.Detach(root, child));
    CHECK(tree.DetachVisual(root, child));
    CHECK(tree.DetachLogical(root, child));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(child));
    CHECK(values.DetachObject(root));
    return true;
}

bool TestContentPresenterLayout() {
    // ContentPresenter is a RenderElement too; pure-layout use stays valid.
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());
    ContentPresenter root(fixture.dispatcher, fixture.properties, fixture.panelType);
    FixedElement child(fixture.dispatcher, fixture.properties,
        fixture.elementType, {30.0, 12.0});
    FixedElement extra(fixture.dispatcher, fixture.properties,
        fixture.elementType, {8.0, 8.0});
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, child));
    CHECK(tree.AttachVisual(root, child));
    CHECK(layout.Attach(root, child));
    CHECK(root.SetContent(&child));
    CHECK(layout.SetRoot(&root, {80.0, 40.0}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root.DesiredSize().width == 30.0 && root.DesiredSize().height == 12.0);
    CHECK(child.LayoutSlot().x == 0.0 && child.LayoutSlot().y == 0.0);
    CHECK(child.RenderSize().width == 80.0 && child.RenderSize().height == 40.0);

    CHECK(tree.AttachLogical(root, extra));
    CHECK(tree.AttachVisual(root, extra));
    CHECK(layout.Attach(root, extra));
    CHECK(root.InvalidateMeasure());
    Result<std::uint32_t> invalidLayout = layout.Flush();
    CHECK(!invalidLayout && invalidLayout.GetStatus().code == ErrorCode::InvalidState);
    CHECK(layout.Detach(root, extra));
    CHECK(tree.DetachVisual(root, extra));
    CHECK(tree.DetachLogical(root, extra));
    CHECK(values.DetachObject(extra));
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(layout.Detach(root, child));
    CHECK(tree.DetachVisual(root, child));
    CHECK(tree.DetachLogical(root, child));
    CHECK(root.SetContent(nullptr));
    CHECK(values.DetachObject(child));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(root));
    return true;
}

bool TestFrameworkLayoutConstraints() {
    Fixture fixture;
    CHECK(fixture.Build());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());

    FixedElement element(fixture.dispatcher, fixture.properties,
        fixture.elementType, {12.0, 8.0});
    CHECK(element.SetMinSize({30.0, 20.0}));
    CHECK(element.SetMaxSize({60.0, 30.0}));
    CHECK(element.SetWidth(40.0));
    CHECK(element.SetHeight(25.0));
    CHECK(element.SetMargin({10.0, 5.0, 20.0, 15.0}));
    CHECK(element.SetHorizontalAlignment(HorizontalAlignment::Center));
    CHECK(element.SetVerticalAlignment(VerticalAlignment::Bottom));
    CHECK(layout.SetRoot(&element, {120.0, 80.0}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));

    CHECK(element.DesiredSize().width == 70.0);
    CHECK(element.DesiredSize().height == 45.0);
    CHECK(element.LayoutSlot().x == 35.0);
    CHECK(element.LayoutSlot().y == 40.0);
    CHECK(element.LayoutSlot().width == 40.0);
    CHECK(element.LayoutSlot().height == 25.0);
    CHECK(element.RenderSize().width == 40.0);
    CHECK(element.RenderSize().height == 25.0);

    CHECK(element.SetHorizontalAlignment(HorizontalAlignment::Right));
    CHECK(!element.IsArrangeValid());
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(element.LayoutSlot().x == 60.0);

    CHECK(element.ClearWidth());
    CHECK(element.ClearHeight());
    CHECK(element.SetHorizontalAlignment(HorizontalAlignment::Stretch));
    CHECK(element.SetVerticalAlignment(VerticalAlignment::Stretch));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(element.LayoutSlot().x == 10.0);
    CHECK(element.LayoutSlot().y == 5.0);
    CHECK(element.LayoutSlot().width == 60.0);
    CHECK(element.LayoutSlot().height == 30.0);

    Result<void> invalidMargin = element.SetMargin({-1.0, 0.0, 0.0, 0.0});
    CHECK(!invalidMargin);
    CHECK(invalidMargin.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidMax = element.SetMaxSize({20.0, 30.0});
    CHECK(!invalidMax);
    CHECK(invalidMax.GetStatus().code == ErrorCode::InvalidArgument);
    Result<void> invalidAlignment = element.SetVerticalAlignment(
        static_cast<VerticalAlignment>(255U));
    CHECK(!invalidAlignment);
    CHECK(invalidAlignment.GetStatus().code == ErrorCode::InvalidArgument);

    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    return true;
}

bool TestGridFixedAutoAndStarTracks() {
    Fixture fixture;
    CHECK(fixture.Build());
    EffectiveValueEngine values(fixture.dispatcher, fixture.properties);
    CHECK(values.Initialize());
    ObjectTree tree(fixture.dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(fixture.dispatcher);
    CHECK(layout.Initialize());

    Grid root(fixture.dispatcher, fixture.properties, fixture.panelType);
    const GridLength columns[] = {
        GridLength::Pixel(20.0), GridLength::Auto(), GridLength::Star(1.0)};
    const GridLength rows[] = {
        GridLength::Pixel(10.0), GridLength::Star(1.0)};
    const GridLength invalidColumns[] = {GridLength::Star(0.0)};
    CHECK(!root.SetColumnDefinitions({invalidColumns, 1U}));
    CHECK(root.SetColumnDefinitions({columns, 3U}));
    CHECK(root.SetRowDefinitions({rows, 2U}));

    FixedElement fixed(fixture.dispatcher, fixture.properties,
        fixture.elementType, {5.0, 4.0});
    FixedElement automatic(fixture.dispatcher, fixture.properties,
        fixture.elementType, {30.0, 8.0});
    FixedElement star(fixture.dispatcher, fixture.properties,
        fixture.elementType, {16.0, 25.0});
    CHECK(tree.SetRoot(&root));
    for (LayoutElement* child : {static_cast<LayoutElement*>(&fixed),
            static_cast<LayoutElement*>(&automatic),
            static_cast<LayoutElement*>(&star)}) {
        CHECK(tree.AttachLogical(root, *child));
        CHECK(tree.AttachVisual(root, *child));
        CHECK(layout.Attach(root, *child));
    }
    CHECK(root.SetChildCell(fixed, 0U, 0U));
    CHECK(root.SetChildCell(automatic, 0U, 1U));
    CHECK(root.SetChildCell(star, 1U, 2U));
    CHECK(!root.SetChildCell(star, 2U, 0U));
    CHECK(layout.SetRoot(&root, {100.0, 80.0}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(root.DesiredSize().width == 66.0);
    CHECK(root.DesiredSize().height == 35.0);
    CHECK(fixed.LayoutSlot().x == 0.0 && fixed.LayoutSlot().width == 20.0);
    CHECK(automatic.LayoutSlot().x == 20.0 && automatic.LayoutSlot().width == 30.0);
    CHECK(star.LayoutSlot().x == 50.0 && star.LayoutSlot().width == 50.0);
    CHECK(star.LayoutSlot().y == 10.0 && star.LayoutSlot().height == 70.0);

    const GridLength weightedColumns[] = {
        GridLength::Pixel(20.0), GridLength::Auto(),
        GridLength::Star(1.0), GridLength::Star(2.0)};
    CHECK(root.SetColumnDefinitions({weightedColumns, 4U}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(std::fabs(star.LayoutSlot().x - 50.0) < 0.000001);
    CHECK(std::fabs(star.LayoutSlot().width - (50.0 / 3.0)) < 0.000001);

    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    for (LayoutElement* child : {static_cast<LayoutElement*>(&star),
            static_cast<LayoutElement*>(&automatic),
            static_cast<LayoutElement*>(&fixed)}) {
        CHECK(layout.Detach(root, *child));
        CHECK(tree.DetachVisual(root, *child));
        CHECK(tree.DetachLogical(root, *child));
        CHECK(values.DetachObject(*child));
    }
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(root));
    return true;
}

} // namespace

int main() {
    if (!TestGeometry()) return 1;
    if (!TestNestedLayoutAndInvalidation()) return 1;
    if (!TestRoundingClippingAndValidation()) return 1;
    if (!TestFrameworkLayoutConstraints()) return 1;
    if (!TestCanvasChildPosition()) return 1;
    if (!TestBorderPaddingDecoratorLayout()) return 1;
    if (!TestContentPresenterLayout()) return 1;
    if (!TestGridFixedAutoAndStarTracks()) return 1;
    std::puts("Aero layout tests passed");
    return 0;
}
