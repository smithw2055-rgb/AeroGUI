#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Controls/Selection.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Detail/AnimationRuntime.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#include "ViewRuntime.hpp"

#include <memory>

namespace Aero::Controls {
}


namespace Aero::Detail {

// Source-side bridge for repository-owned diagnostics and samples. Product
// consumers operate on View and cannot observe the runtime service graph.
class ViewAccess final {
public:
    static Base::Result<Data::BindingHandle> AttachBinding(
        View& view,
        const Data::BindingDescriptor& descriptor) noexcept {
        return Runtime(view).Bindings()->Attach(descriptor);
    }

    static Base::Result<std::uint32_t> FlushBindings(
        View& view) noexcept {
        return Runtime(view).Bindings()->Flush();
    }

    static std::unique_ptr<Controls::ItemContainerGenerator>
    CreateItemContainerGenerator(View& view) {
        ViewRuntime& runtime = Runtime(view);
        return std::make_unique<Controls::ItemContainerGenerator>(
            *runtime.Tree(),
            *runtime.Layout(),
            *runtime.EffectiveValues(),
            nullptr,
            runtime.Renderer());
    }

    static Aero::Visual* RootVisual(
        View& view) noexcept {
        Aero::ObjectTree* tree = Runtime(view).Tree();
        return tree != nullptr ? tree->Root() : nullptr;
    }

    static Aero::Detail::AnimationManager* Animations(
        View& view) noexcept {
        return Runtime(view).Animations();
    }

    static Controls::VisualStateManager* VisualStates(
        View& view) noexcept {
        return Runtime(view).VisualStates();
    }

    static Controls::TemplateManager* Templates(
        View& view) noexcept {
        return Runtime(view).Templates();
    }

private:
    static ViewRuntime& Runtime(View& view) noexcept {
        return *static_cast<ViewRuntime*>(
            view.IntegrationRuntime());
    }
};

} // namespace Aero::Detail
