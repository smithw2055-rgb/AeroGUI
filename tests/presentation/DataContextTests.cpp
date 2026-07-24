#include <Aero/Base/Ref.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <cstdio>
#include <memory>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class DataItem final : public Object {
public:
    TypeId RuntimeType() const noexcept override {
        return BuiltinTypes::Object;
    }
    ~DataItem() override = default;
};

bool RunPropertyChanges(Dispatcher& dispatcher, std::uint32_t count = 1U) {
    for (std::uint32_t index = 0U; index < count; ++index) {
        if (!dispatcher.RunFramePhase(
                DispatcherFramePhase::PropertyChanges)) {
            return false;
        }
    }
    return true;
}

bool SameObject(
    FrameworkElement& element,
    const Ref<Object>& expected) {
    Result<Ref<Object>> actual = element.GetDataContext();
    return actual && actual.Value().Get() == expected.Get();
}

bool TestDataContextInheritanceAndReparenting() {
    MetadataDomain metadata;
    CHECK(TryRegisterCoreMetadata(metadata));
    CHECK(TryRegisterPresentationMetadata(metadata));
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(TryRegisterDependencyPropertyRuntimeProvider(
        runtime,
        metadata.DependencyProperties(),
        BuiltinTypes::DependencyObject));
    CHECK(runtime.Freeze());

    Dispatcher dispatcher;
    ObjectServicesScope services(
        dispatcher, metadata.DependencyProperties(), runtime);
    EffectiveValueEngine values(
        dispatcher, metadata.DependencyProperties());
    CHECK(values.Initialize());
    ObjectTree tree(dispatcher, values);
    CHECK(tree.Initialize());

    FrameworkElement root(BuiltinTypes::FrameworkElement);
    FrameworkElement branch(BuiltinTypes::FrameworkElement);
    FrameworkElement child(BuiltinTypes::FrameworkElement);
    Result<Ref<DataItem>> firstItem = MakeRef<DataItem>();
    Result<Ref<DataItem>> secondItem = MakeRef<DataItem>();
    Result<Ref<DataItem>> thirdItem = MakeRef<DataItem>();
    CHECK(firstItem && secondItem && thirdItem);
    Ref<Object> first(std::move(firstItem).Value());
    Ref<Object> second(std::move(secondItem).Value());
    Ref<Object> third(std::move(thirdItem).Value());

    CHECK(root.SetDataContext(first));
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, child));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(SameObject(child, first));
    CHECK(child.GetValueSource(
        FrameworkElement::DataContextProperty).Value() ==
        EffectiveValueSource::Current);

    CHECK(child.SetDataContext(second));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(SameObject(child, second));
    CHECK(child.ClearDataContext());
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(SameObject(child, first));

    CHECK(branch.SetDataContext(third));
    CHECK(tree.AttachLogical(root, branch));
    CHECK(tree.DetachLogical(root, child));
    CHECK(tree.AttachLogical(branch, child));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(SameObject(child, third));

    CHECK(branch.ClearDataContext());
    CHECK(root.SetDataContext(second));
    CHECK(RunPropertyChanges(dispatcher, 3U));
    CHECK(SameObject(branch, second));
    CHECK(SameObject(child, second));

    CHECK(root.ClearDataContext());
    CHECK(RunPropertyChanges(dispatcher, 3U));
    CHECK(!root.GetDataContext().Value());
    CHECK(!branch.GetDataContext().Value());
    CHECK(!child.GetDataContext().Value());

    CHECK(tree.DetachLogical(branch, child));
    CHECK(tree.DetachLogical(root, branch));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(child));
    CHECK(values.DetachObject(branch));
    CHECK(values.DetachObject(root));
    return true;
}

bool TestInheritedDataContextBindingReResolves() {
    MetadataDomain metadata;
    CHECK(TryRegisterCoreMetadata(metadata));
    CHECK(TryRegisterPresentationMetadata(metadata));
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(TryRegisterDependencyPropertyRuntimeProvider(
        runtime,
        metadata.DependencyProperties(),
        BuiltinTypes::DependencyObject));
    CHECK(runtime.Freeze());

    Dispatcher dispatcher;
    ObjectServicesScope services(
        dispatcher, metadata.DependencyProperties(), runtime);
    EffectiveValueEngine values(
        dispatcher, metadata.DependencyProperties());
    CHECK(values.Initialize());
    ObjectTree tree(dispatcher, values);
    CHECK(tree.Initialize());
    BindingManager bindings(dispatcher);
    CHECK(bindings.Initialize());

    FrameworkElement root(BuiltinTypes::FrameworkElement);
    FrameworkElement target(BuiltinTypes::FrameworkElement);
    Result<Ref<FrameworkElement>> firstModel =
        MakeRef<FrameworkElement>(BuiltinTypes::FrameworkElement);
    Result<Ref<FrameworkElement>> secondModel =
        MakeRef<FrameworkElement>(BuiltinTypes::FrameworkElement);
    CHECK(firstModel && secondModel);
    Ref<FrameworkElement> firstTyped =
        std::move(firstModel).Value();
    Ref<FrameworkElement> secondTyped =
        std::move(secondModel).Value();
    CHECK(firstTyped->SetWidth(42.0));
    CHECK(secondTyped->SetWidth(84.0));
    Ref<Object> first(firstTyped);
    Ref<Object> second(secondTyped);

    CHECK(root.SetDataContext(first));
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, target));
    MetadataBindingDescriptor descriptor;
    descriptor.metadata = &runtime;
    descriptor.target = &target;
    descriptor.targetProperty = FrameworkElement::WidthProperty;
    descriptor.dataContextProperty =
        FrameworkElement::DataContextProperty;
    descriptor.path = "Width";
    descriptor.mode = BindingMode::TwoWay;
    Result<BindingHandle> attached = bindings.Attach(descriptor);
    CHECK(attached);
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::DataBind));
    CHECK(target.Width() == 42.0);

    CHECK(root.SetDataContext(second));
    CHECK(RunPropertyChanges(dispatcher, 2U));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::DataBind));
    CHECK(target.Width() == 84.0);

    CHECK(target.SetWidth(21.0));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::DataBind));
    CHECK(secondTyped->Width() == 21.0);

    CHECK(bindings.Detach(attached.Value()).Value());
    bindings.Shutdown();
    CHECK(tree.DetachLogical(root, target));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(target));
    CHECK(values.DetachObject(root));
    return true;
}

} // namespace

int main() {
    if (!TestDataContextInheritanceAndReparenting()) return 1;
    if (!TestInheritedDataContextBindingReResolves()) return 1;
    std::puts("Aero DataContext tests passed");
    return 0;
}
