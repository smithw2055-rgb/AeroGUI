#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Presentation;

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed %d: %s\n", \
            __LINE__, #expression); \
        return false; \
    } \
} while (false)

bool Near(double left, double right) noexcept {
    return std::fabs(left - right) < 0.000001;
}

class FixedElement final : public FrameworkElement {
public:
    explicit FixedElement(Size desired) noexcept
        : FrameworkElement(BuiltinTypes::FrameworkElement),
          desired_(desired) {}

    void SetDesired(Size value) noexcept {
        desired_ = value;
    }

protected:
    Result<Size> MeasureOverride(
        Size available) noexcept override {
        return Size{
            std::min(desired_.width, available.width),
            std::min(desired_.height, available.height)};
    }

private:
    Size desired_;
};

class LogicalScrollPanel final
    : public FrameworkElement,
      public IScrollInfo {
public:
    LogicalScrollPanel() noexcept
        : FrameworkElement(BuiltinTypes::FrameworkElement) {
        data_.extentWidth = 20.0;
        data_.extentHeight = 100.0;
    }

    ScrollData Data() const noexcept override {
        return data_;
    }

    Result<bool> SetViewport(
        Size value) noexcept override {
        const bool changed =
            !Near(data_.viewportWidth, value.width) ||
            !Near(data_.viewportHeight, value.height);
        data_.viewportWidth = value.width;
        data_.viewportHeight = value.height;
        Clamp();
        return changed;
    }

    Result<bool> SetHorizontalOffset(
        double value) noexcept override {
        const double next = std::clamp(
            value, 0.0, std::max(
                0.0,
                data_.extentWidth - data_.viewportWidth));
        const bool changed =
            !Near(next, data_.horizontalOffset);
        data_.horizontalOffset = next;
        return changed;
    }

    Result<bool> SetVerticalOffset(
        double value) noexcept override {
        const double next = std::clamp(
            value, 0.0, std::max(
                0.0,
                data_.extentHeight - data_.viewportHeight));
        const bool changed =
            !Near(next, data_.verticalOffset);
        data_.verticalOffset = next;
        return changed;
    }

    Result<bool> LineHorizontal(
        double direction) noexcept override {
        return SetHorizontalOffset(
            data_.horizontalOffset + direction);
    }

    Result<bool> LineVertical(
        double direction) noexcept override {
        ++lineCalls;
        return SetVerticalOffset(
            data_.verticalOffset + direction);
    }

    Result<bool> PageHorizontal(
        double direction) noexcept override {
        return SetHorizontalOffset(
            data_.horizontalOffset +
                direction * data_.viewportWidth);
    }

    Result<bool> PageVertical(
        double direction) noexcept override {
        return SetVerticalOffset(
            data_.verticalOffset +
                direction * data_.viewportHeight);
    }

    std::uint32_t lineCalls = 0U;

protected:
    Result<Size> MeasureOverride(
        Size available) noexcept override {
        return available;
    }

private:
    ScrollData data_;

    void Clamp() noexcept {
        data_.horizontalOffset = std::clamp(
            data_.horizontalOffset,
            0.0,
            std::max(
                0.0,
                data_.extentWidth - data_.viewportWidth));
        data_.verticalOffset = std::clamp(
            data_.verticalOffset,
            0.0,
            std::max(
                0.0,
                data_.extentHeight - data_.viewportHeight));
    }
};

struct ScrollLog final {
    std::uint32_t count = 0U;
    ScrollChangedEventArgs last;

    void OnChanged(
        Aero::Base::Object*,
        const ScrollChangedEventArgs& args) noexcept {
        ++count;
        last = args;
    }
};

struct Fixture final {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore
        typeBehaviors{types};
    MetadataRegistrationTypes
        typeRegistration{types, typeBehaviors};
    MetadataValueRegistrationStore
        valueRegistrations{types};
    DependencyPropertyRegistry
        properties{types, typeBehaviors};
    Dispatcher dispatcher;
    ObjectServicesScope services{
        dispatcher, properties, valueRegistrations};
    RoutedEventCatalog eventCatalog{
        types, typeBehaviors};
    RoutedEventManager events{eventCatalog};
    EffectiveValueEngine values{
        dispatcher, properties};
    ObjectTree tree{dispatcher, values};
    LayoutManager layout{dispatcher};

    bool Build() {
        MetaRegistrationContext registration(
            types,
            typeBehaviors,
            valueRegistrations,
            properties,
            &eventCatalog);
        CHECK(Aero::Core::Detail::
            PopulateCoreMetadata(registration));
        CHECK(Aero::Presentation::Detail::
            PopulatePresentationMetadata(registration));
        CHECK(Aero::Controls::Detail::
            PopulateControlsMetadata(registration));
        CHECK(types.Freeze());
        CHECK(typeBehaviors.Freeze());
        CHECK(valueRegistrations.Freeze());
        CHECK(properties.Freeze());
        CHECK(eventCatalog.Freeze());
        CHECK(values.Initialize());
        CHECK(tree.Initialize());
        CHECK(layout.Initialize());
        return true;
    }
};

bool AttachChild(
    Fixture& fixture,
    ScrollContentPresenter& parent,
    UIElement& child) {
    CHECK(fixture.tree.AttachLogical(parent, child));
    CHECK(fixture.tree.AttachVisual(parent, child));
    CHECK(fixture.layout.Attach(parent, child));
    CHECK(parent.SetChild(&child));
    return true;
}

bool DetachChild(
    Fixture& fixture,
    ScrollContentPresenter& parent,
    UIElement& child) {
    CHECK(fixture.layout.Detach(parent, child));
    CHECK(fixture.tree.DetachVisual(parent, child));
    CHECK(fixture.tree.DetachLogical(parent, child));
    CHECK(parent.SetChild(nullptr));
    CHECK(fixture.values.DetachObject(child));
    return true;
}

bool TestPhysicalLayoutAndClamping() {
    Fixture fixture;
    CHECK(fixture.Build());
    ScrollViewer viewer;
    FixedElement content({300.0, 500.0});
    Result<void> readOnly = viewer.SetValue(
        ScrollViewer::HorizontalOffsetProperty,
        Value::FromDouble(
            BuiltinTypes::Double, 10.0));
    CHECK(!readOnly);
    CHECK(fixture.tree.SetRoot(&viewer));
    CHECK(AttachChild(fixture, viewer, content));
    CHECK(fixture.layout.SetRoot(
        &viewer, {100.0, 120.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(viewer.ClipToBounds());
    CHECK(Near(viewer.ExtentWidth(), 300.0));
    CHECK(Near(viewer.ExtentHeight(), 500.0));
    CHECK(Near(viewer.ViewportWidth(), 100.0));
    CHECK(Near(viewer.ViewportHeight(), 120.0));

    CHECK(viewer.SetHorizontalOffset(250.0));
    CHECK(viewer.SetVerticalOffset(450.0));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(Near(viewer.HorizontalOffset(), 200.0));
    CHECK(Near(viewer.VerticalOffset(), 380.0));
    CHECK(Near(content.LayoutSlot().x, -200.0));
    CHECK(Near(content.LayoutSlot().y, -380.0));

    content.SetDesired({40.0, 60.0});
    CHECK(content.InvalidateMeasure());
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(Near(viewer.HorizontalOffset(), 0.0));
    CHECK(Near(viewer.VerticalOffset(), 0.0));
    CHECK(Near(viewer.ExtentWidth(), 40.0));
    CHECK(Near(viewer.ExtentHeight(), 60.0));

    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(DetachChild(fixture, viewer, content));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(viewer));
    return true;
}

bool TestWheelAndScrollChanged() {
    Fixture fixture;
    CHECK(fixture.Build());
    ScrollViewer viewer;
    FixedElement content({100.0, 500.0});
    CHECK(fixture.tree.SetRoot(&viewer));
    CHECK(AttachChild(fixture, viewer, content));
    CHECK(fixture.layout.SetRoot(
        &viewer, {100.0, 100.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));

    ScrollInteractionManager interactions(
        fixture.tree, fixture.events);
    CHECK(interactions.Attach(viewer));
    ScrollLog log;
    ScrollChangedEventHandler handler(
        &log, &ScrollLog::OnChanged);
    CHECK(viewer.ScrollChanged().TryAdd(handler));

    HitTestManager hitTests;
    PointerInputManager pointer(
        hitTests, fixture.events, viewer);
    PointerInput wheel;
    wheel.action = PointerAction::Wheel;
    wheel.position = {10.0, 10.0};
    wheel.wheelDeltaY = -2.0;
    Result<PointerDispatchResult> dispatched =
        pointer.Dispatch(wheel);
    CHECK(dispatched && dispatched.Value().routed);
    CHECK(Near(viewer.VerticalOffset(), 32.0));
    CHECK(log.count == 1U);
    CHECK(log.last.inputKind == ScrollInputKind::Wheel);
    CHECK(Near(log.last.oldData.verticalOffset, 0.0));
    CHECK(Near(log.last.newData.verticalOffset, 32.0));

    CHECK(viewer.LineVertical(1.0));
    CHECK(Near(viewer.VerticalOffset(), 48.0));
    CHECK(log.count == 2U);
    CHECK(log.last.inputKind == ScrollInputKind::Line);
    CHECK(viewer.PageVertical(1.0));
    CHECK(Near(viewer.VerticalOffset(), 148.0));
    CHECK(log.count == 3U);
    CHECK(log.last.inputKind == ScrollInputKind::Page);
    CHECK(viewer.ApplyScrollDelta(
        0.0, 1000.0, ScrollInputKind::Touch));
    CHECK(Near(viewer.VerticalOffset(), 400.0));
    CHECK(log.last.inputKind == ScrollInputKind::Touch);

    CHECK(viewer.ScrollChanged().Remove(handler));
    CHECK(interactions.Detach(viewer).Value());
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(DetachChild(fixture, viewer, content));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(viewer));
    return true;
}

bool TestLogicalScrolling() {
    Fixture fixture;
    CHECK(fixture.Build());
    ScrollViewer viewer;
    LogicalScrollPanel content;
    CHECK(fixture.tree.SetRoot(&viewer));
    CHECK(AttachChild(fixture, viewer, content));
    CHECK(viewer.SetContentScrollInfo(&content));
    CHECK(viewer.SetCanContentScroll(true));
    CHECK(fixture.layout.SetRoot(
        &viewer, {20.0, 10.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(Near(viewer.ExtentHeight(), 100.0));
    CHECK(Near(viewer.ViewportHeight(), 10.0));
    CHECK(viewer.LineVertical(3.0));
    CHECK(content.lineCalls == 1U);
    CHECK(Near(viewer.VerticalOffset(), 3.0));
    CHECK(viewer.PageVertical(1.0));
    CHECK(Near(viewer.VerticalOffset(), 13.0));
    CHECK(Near(content.LayoutSlot().y, 0.0));

    CHECK(viewer.SetCanContentScroll(false));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(viewer.Data().extentHeight > 100.0);

    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(viewer.SetContentScrollInfo(nullptr));
    CHECK(DetachChild(fixture, viewer, content));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(viewer));
    return true;
}

bool TestTrackScrollBarAndThumb() {
    Fixture fixture;
    CHECK(fixture.Build());
    Track track;
    CHECK(track.SetRange(0.0, 80.0));
    CHECK(track.SetViewportSize(20.0));
    CHECK(track.SetValue(40.0));
    CHECK(Near(track.ThumbLength(100.0), 20.0));
    CHECK(Near(track.ThumbOffset(100.0), 40.0));
    Result<double> mapped =
        track.ValueFromThumbOffset(60.0, 100.0);
    CHECK(mapped && Near(mapped.Value(), 60.0));

    ScrollBar bar;
    CHECK(bar.SetRange(0.0, 80.0));
    CHECK(bar.SetViewportSize(20.0));
    CHECK(bar.SetSmallChange(5.0));
    CHECK(bar.LineIncrement().Value());
    CHECK(Near(bar.Value(), 5.0));
    CHECK(bar.PageIncrement().Value());
    CHECK(Near(bar.Value(), 25.0));
    CHECK(bar.DragThumb(80.0, 100.0).Value());
    CHECK(Near(bar.Value(), 80.0));
    CHECK(!bar.LineIncrement().Value());

    Thumb thumb;
    CHECK(thumb.BeginDrag(7U, {2.0, 3.0}));
    Result<ThumbDragDelta> delta =
        thumb.DragTo(7U, {8.0, 13.0});
    CHECK(delta);
    CHECK(Near(delta.Value().horizontalChange, 6.0));
    CHECK(Near(delta.Value().verticalChange, 10.0));
    CHECK(thumb.EndDrag(7U).Value());
    CHECK(!thumb.EndDrag(7U).Value());

    CHECK(fixture.values.DetachObject(thumb));
    CHECK(fixture.values.DetachObject(bar));
    CHECK(fixture.values.DetachObject(track));
    return true;
}

} // namespace

int main() {
    if (!TestPhysicalLayoutAndClamping()) return 1;
    if (!TestWheelAndScrollChanged()) return 1;
    if (!TestLogicalScrolling()) return 1;
    if (!TestTrackScrollBarAndThumb()) return 1;
    std::puts("Aero scroll tests passed");
    return 0;
}
