#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cstdio>
#include <memory>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class TestControl final : public Control {
public:
    TestControl() noexcept
        : Control(BuiltinTypes::Control) {}
    ~TestControl() override = default;
};

Result<void> BuildTemplate(
    TemplateBuildContext& context,
    void*) noexcept {
    Result<Ref<FrameworkElement>> root =
        MakeRef<FrameworkElement>(
            BuiltinTypes::FrameworkElement);
    if (!root) return root.GetStatus();
    Ref<FrameworkElement> rootTyped =
        std::move(root).Value();
    Ref<Object> rootOwner(rootTyped);
    Result<void> registered =
        context.SetRoot(rootOwner, *rootTyped);
    if (!registered) return registered.GetStatus();

    Result<Ref<FrameworkElement>> content =
        MakeRef<FrameworkElement>(
            BuiltinTypes::FrameworkElement);
    if (!content) return content.GetStatus();
    Ref<FrameworkElement> contentTyped =
        std::move(content).Value();
    Ref<Object> contentOwner(contentTyped);
    return context.AddPart(
        "PART_Content",
        *rootTyped,
        contentOwner,
        *contentTyped);
}

bool RunPropertyChanges(Dispatcher& dispatcher) {
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    return true;
}

bool TestControlTemplateBindingTriggerAndNameScope() {
    MetadataDomain metadata;
    CHECK(TryRegisterCoreMetadata(metadata));
    CHECK(TryRegisterPresentationMetadata(metadata));
    CHECK(TryRegisterControlsMetadata(metadata));
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(TryRegisterDependencyPropertyRuntimeProvider(
        runtime,
        metadata.DependencyProperties(),
        BuiltinTypes::DependencyObject));
    CHECK(runtime.Freeze());

    Dispatcher dispatcher;
    ObjectServicesScope services(
        dispatcher,
        metadata.DependencyProperties(),
        runtime);
    EffectiveValueEngine values(
        dispatcher, metadata.DependencyProperties());
    CHECK(values.Initialize());
    ObjectTree tree(dispatcher, values);
    CHECK(tree.Initialize());
    TestControl control;
    CHECK(tree.SetRoot(&control));
    CHECK(control.SetWidth(42.0));

    ControlTemplate plan(
        BuiltinTypes::Control, &BuildTemplate);
    CHECK(plan.TryAddTemplateBinding(
        "PART_Content",
        FrameworkElement::WidthProperty,
        FrameworkElement::WidthProperty));
    TemplatePropertyTrigger trigger;
    trigger.property = FrameworkElement::HeightProperty;
    Length heightFive = Length::Pixels(5.0);
    Result<Value> condition = runtime.TryCreateValue(
        TypeOf<Length>(), &heightFive);
    CHECK(condition);
    trigger.value = condition.Value();
    TemplateTriggerSetter setter;
    CHECK(setter.targetName.TryAssign("PART_Content"));
    setter.property = FrameworkElement::HeightProperty;
    Length heightSeventySeven = Length::Pixels(77.0);
    Result<Value> triggerValue = runtime.TryCreateValue(
        TypeOf<Length>(), &heightSeventySeven);
    CHECK(triggerValue);
    setter.value = triggerValue.Value();
    CHECK(trigger.setters.TryPushBack(std::move(setter)));
    CHECK(plan.TryAddPropertyTrigger(std::move(trigger)));

    VisualStateGroup commonStates;
    CHECK(commonStates.name.TryAssign("CommonStates"));
    VisualState normal;
    CHECK(normal.name.TryAssign("Normal"));
    CHECK(commonStates.states.TryPushBack(std::move(normal)));
    VisualState pressed;
    CHECK(pressed.name.TryAssign("Pressed"));
    VisualStateSetter pressedWidth;
    CHECK(pressedWidth.targetName.TryAssign("PART_Content"));
    pressedWidth.property = FrameworkElement::WidthProperty;
    Length widthEighty = Length::Pixels(80.0);
    Result<Value> pressedValue = runtime.TryCreateValue(
        TypeOf<Length>(), &widthEighty);
    CHECK(pressedValue);
    pressedWidth.value = pressedValue.Value();
    CHECK(pressed.setters.TryPushBack(std::move(pressedWidth)));
    CHECK(commonStates.states.TryPushBack(std::move(pressed)));
    VisualState broken;
    CHECK(broken.name.TryAssign("Broken"));
    VisualStateSetter missingTarget;
    CHECK(missingTarget.targetName.TryAssign("PART_Missing"));
    missingTarget.property = FrameworkElement::WidthProperty;
    missingTarget.value = pressedValue.Value();
    CHECK(broken.setters.TryPushBack(std::move(missingTarget)));
    CHECK(commonStates.states.TryPushBack(std::move(broken)));
    CHECK(plan.TryAddVisualStateGroup(std::move(commonStates)));

    VisualStateGroup focusStates;
    CHECK(focusStates.name.TryAssign("FocusStates"));
    VisualState focused;
    CHECK(focused.name.TryAssign("Focused"));
    VisualStateSetter focusedMinimum;
    CHECK(focusedMinimum.targetName.TryAssign("PART_Content"));
    focusedMinimum.property = FrameworkElement::MinWidthProperty;
    focusedMinimum.value =
        Value::FromDouble(BuiltinTypes::Double, 10.0);
    CHECK(focused.setters.TryPushBack(std::move(focusedMinimum)));
    CHECK(focusStates.states.TryPushBack(std::move(focused)));
    VisualState unfocused;
    CHECK(unfocused.name.TryAssign("Unfocused"));
    CHECK(focusStates.states.TryPushBack(std::move(unfocused)));
    CHECK(plan.TryAddVisualStateGroup(std::move(focusStates)));
    CHECK(plan.Seal(metadata.DependencyProperties()));

    TemplateManager templates(
        tree, values, metadata.DependencyProperties());
    Result<TemplateHandle> applied =
        templates.Apply(control, plan);
    CHECK(applied);
    auto* content = static_cast<FrameworkElement*>(
        templates.FindName(
            applied.Value(), "PART_Content"));
    CHECK(content != nullptr);
    CHECK(content->TemplatedParent() == &control);
    CHECK(control.TemplateChild() != nullptr);
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->Width() == 42.0);
    CHECK(values.Diagnostics(
        *content,
        FrameworkElement::WidthProperty).Value().provider ==
        EffectiveValueProvider::Template);

    CHECK(control.SetWidth(64.0));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->Width() == 64.0);
    CHECK(content->SetWidth(99.0));
    CHECK(values.Invalidate(
        *content, FrameworkElement::WidthProperty));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->Width() == 99.0);

    CHECK(control.SetHeight(5.0));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->Height() == 77.0);
    CHECK(values.Diagnostics(
        *content,
        FrameworkElement::HeightProperty).Value().provider ==
        EffectiveValueProvider::Trigger);
    CHECK(control.SetHeight(6.0));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(!content->HasHeight());

    VisualStateManager states(values, templates);
    CHECK(states.CurrentState(control, "CommonStates").Empty());
    Result<bool> normalState = states.GoToState(
        control, "CommonStates", "Normal");
    CHECK(normalState && normalState.Value());
    CHECK(states.CurrentState(
        control, "CommonStates") == "Normal");
    Result<bool> pressedState = states.GoToState(
        control, "CommonStates", "Pressed");
    CHECK(pressedState && pressedState.Value());
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->Width() == 80.0);
    CHECK(values.Diagnostics(
        *content,
        FrameworkElement::WidthProperty).Value().provider ==
        EffectiveValueProvider::Animation);
    pressedState = states.GoToState(
        control, "CommonStates", "Pressed");
    CHECK(pressedState && !pressedState.Value());
    Result<bool> brokenState = states.GoToState(
        control, "CommonStates", "Broken");
    CHECK(!brokenState);
    CHECK(brokenState.GetStatus().code == ErrorCode::NotFound);
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->Width() == 80.0);
    CHECK(states.CurrentState(
        control, "CommonStates") == "Pressed");
    normalState = states.GoToState(
        control, "CommonStates", "Normal");
    CHECK(normalState && normalState.Value());
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->Width() == 99.0);

    Result<bool> focusedState = states.GoToState(
        control, "FocusStates", "Focused");
    CHECK(focusedState && focusedState.Value());
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->MinSize().width == 10.0);
    Result<bool> clearedFocus = states.ClearState(
        control, "FocusStates");
    CHECK(clearedFocus && clearedFocus.Value());
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(content->MinSize().width == 0.0);
    Result<bool> missingState = states.GoToState(
        control, "CommonStates", "Missing");
    CHECK(!missingState);
    CHECK(missingState.GetStatus().code == ErrorCode::NotFound);
    Result<std::uint32_t> clearedStates = states.Clear(control);
    CHECK(clearedStates && clearedStates.Value() == 1U);

    CHECK(templates.Clear(applied.Value()).Value());
    CHECK(control.TemplateChild() == nullptr);
    CHECK(control.VisualChildren().Empty());
    CHECK(control.LogicalChildren().Empty());
    CHECK(templates.FindName(
        applied.Value(), "PART_Content") == nullptr);
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(control));
    return true;
}

} // namespace

int main() {
    if (!TestControlTemplateBindingTriggerAndNameScope()) return 1;
    std::puts("Aero control template tests passed");
    return 0;
}
