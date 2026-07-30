#pragma once

#include <Aero/Controls/Selection.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Animation.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#include "ViewRuntime.hpp"

#include <memory>

namespace Aero::Controls {
class TemplateManager;
}

namespace Aero::Presentation {
class AnimationManager;
}

namespace Aero::Detail {

// Source-side bridge for repository-owned diagnostics and samples. Product
// consumers operate on View and cannot observe the runtime service graph.
class ViewAccess final {
public:
    static Base::Result<Presentation::BindingHandle> AttachBinding(
        View& view,
        const Presentation::BindingDescriptor& descriptor) noexcept {
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

    static Presentation::Visual* RootVisual(
        View& view) noexcept {
        Presentation::ObjectTree* tree = Runtime(view).Tree();
        return tree != nullptr ? tree->Root() : nullptr;
    }

    static Presentation::AnimationManager* Animations(
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
