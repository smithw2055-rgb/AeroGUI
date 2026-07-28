#pragma once

#include <Aero/Controls/Selection.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/RuntimeEnvironment.hpp>

#include "ViewRuntime.hpp"

#include <memory>

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

private:
    static ViewRuntime& Runtime(View& view) noexcept {
        return *static_cast<ViewRuntime*>(
            view.IntegrationRuntime());
    }
};

} // namespace Aero::Detail
