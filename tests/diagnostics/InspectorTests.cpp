#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Diagnostics/Inspector.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cstdio>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Controls;
using namespace Aero::Core;
using namespace Aero::Diagnostics;
using namespace Aero::Presentation;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf( \
                stderr, \
                "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class ContextObject final
    : public Object {
public:
    ContextObject() noexcept = default;

    TypeId RuntimeType()
        const noexcept override {
        return BuiltinTypes::Object;
    }
};

Result<void> BuildButtonTemplate(
    TemplateBuildContext& context,
    void*) noexcept {
    Result<Ref<Border>> made =
        MakeRef<Border>();
    if (!made) {
        return made.GetStatus();
    }
    Ref<Border> root =
        std::move(made).Value();
    Result<void> background =
        root->SetBackground(
            {0.1F, 0.2F, 0.3F, 1.0F});
    if (!background) {
        return background.GetStatus();
    }
    Ref<Object> owner(root);
    return context.SetRoot(
        std::move(owner), *root);
}

bool TestInspectorEndpoints() {
    MetadataDomain metadata;
    CHECK(TryRegisterCoreMetadata(metadata));
    CHECK(TryRegisterPresentationMetadata(
        metadata));
    CHECK(TryRegisterControlsMetadata(
        metadata));
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(
        TryRegisterDependencyPropertyRuntimeProvider(
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
        dispatcher,
        metadata.DependencyProperties());
    CHECK(values.Initialize());
    ObjectTree tree(dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(dispatcher);
    CHECK(layout.Initialize());
    NullRenderBackend nullBackend;
    RenderManager renderer(
        dispatcher, nullBackend);
    CHECK(renderer.Initialize());
    BindingManager bindings(dispatcher);
    CHECK(bindings.Initialize());
    StyleManager styles(
        values,
        metadata.DependencyProperties());
    TemplateManager templates(
        tree,
        values,
        metadata.DependencyProperties(),
        &layout,
        &renderer);

    StackPanel root;
    Button button;
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(
        root, button));
    CHECK(tree.AttachVisual(
        root, button));
    CHECK(layout.SetRoot(
        &root, {320.0, 200.0}));
    CHECK(layout.Attach(root, button));
    CHECK(renderer.SetRoot(&root));
    CHECK(renderer.Attach(root, button));

    Result<Ref<ContextObject>> context =
        MakeRef<ContextObject>();
    CHECK(context);
    Ref<Object> contextObject(
        context.Value());
    CHECK(root.SetDataContext(
        contextObject));
    CHECK(root.SetWidth(240.0));

    BindingDescriptor descriptor;
    descriptor.source = &root;
    descriptor.sourceProperty =
        FrameworkElement::WidthProperty;
    descriptor.target = &button;
    descriptor.targetProperty =
        FrameworkElement::WidthProperty;
    descriptor.mode = BindingMode::OneWay;
    CHECK(bindings.Attach(descriptor));

    Style style(
        Button::StaticTypeId());
    Length height =
        Length::Pixels(36.0);
    Result<Value> heightValue =
        runtime.TryCreateValue(
            TypeOf<Length>(), &height);
    CHECK(heightValue);
    CHECK(style.TryAddSetter(
        FrameworkElement::HeightProperty,
        heightValue.Value()));
    CHECK(style.Seal(
        metadata.DependencyProperties()));
    CHECK(styles.Apply(button, style));

    ControlTemplate controlTemplate(
        Button::StaticTypeId(),
        &BuildButtonTemplate);
    CHECK(controlTemplate.Seal(
        metadata.DependencyProperties()));
    Result<TemplateHandle> applied =
        templates.Apply(
            button, controlTemplate);
    CHECK(applied);

    const DispatcherFramePhase phases[] = {
        DispatcherFramePhase::BeginFrame,
        DispatcherFramePhase::
            PropertyChanges,
        DispatcherFramePhase::DataBind,
        DispatcherFramePhase::Layout,
        DispatcherFramePhase::
            Lifecycle,
        DispatcherFramePhase::
            RenderCommit,
        DispatcherFramePhase::EndFrame};
    for (DispatcherFramePhase phase :
        phases) {
        CHECK(dispatcher.RunFramePhase(
            phase));
    }

    InspectorEndpoint inspector(
        tree,
        values,
        bindings,
        renderer,
        &styles,
        &templates);
    InspectorSnapshot snapshot;
    CHECK(inspector.Capture(
        button, snapshot));
    CHECK(snapshot.target == &button);
    CHECK(snapshot.logicalTree.Size() == 3U);
    CHECK(snapshot.visualTree.Size() == 3U);
    CHECK(!snapshot.
        effectiveProperties.Empty());
    CHECK(snapshot.activeBindings.Size() ==
        1U);
    CHECK(snapshot.activeBindings[0U].
        source == &root);
    CHECK(snapshot.activeBindings[0U].
        target == &button);
    CHECK(snapshot.dataContext.Get() ==
        contextObject.Get());
    CHECK(snapshot.appliedStyle == &style);
    CHECK(snapshot.appliedTemplate.value ==
        applied.Value().value);
    CHECK(snapshot.layoutRect.width ==
        240.0);
    CHECK(snapshot.layoutRect.height ==
        36.0);
    CHECK(snapshot.render.nodeCount == 3U);
    CHECK(snapshot.render.commandCount >
        0U);
    CHECK(snapshot.frameTimings.
        frameSequence == 1U);
    CHECK(inspector.RenderPlan().
        StableHash() ==
        snapshot.render.planHash);

    CHECK(templates.Clear(button));
    CHECK(styles.Clear(button, style));
    CHECK(bindings.DetachObject(button));
    bindings.Shutdown();
    CHECK(renderer.SetRoot(nullptr));
    CHECK(layout.SetRoot(nullptr, {}));
    CHECK(layout.Detach(root, button));
    CHECK(tree.DetachVisual(root, button));
    CHECK(tree.DetachLogical(root, button));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(button));
    CHECK(values.DetachObject(root));
    return true;
}

} // namespace

int main() {
    if (!TestInspectorEndpoints()) {
        return 1;
    }
    std::puts(
        "Aero inspector endpoint tests passed");
    return 0;
}
