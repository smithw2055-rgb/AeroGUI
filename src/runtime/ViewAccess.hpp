#pragma once

#include "RuntimeFwd.hpp"
#include "../data/BindingRuntime.hpp"
#include "../controls/ItemContainerGeneratorAccess.hpp"

#include <Aero/Controls/Items.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Data.hpp>
#include "../media/AnimationRuntimeTypes.hpp"
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
        Base::Result<Controls::ItemContainerGenerator*> created =
            Controls::Detail::ItemContainerGeneratorAccess::Create(
                *runtime.Tree(),
                *runtime.Layout(),
                *runtime.EffectiveValues(),
                nullptr,
                runtime.Renderer());
        return created
            ? std::unique_ptr<Controls::ItemContainerGenerator>(
                  created.Value())
            : nullptr;
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

    static bool IsInstanceOf(
        View& view,
        const Base::Object& object,
        Core::TypeId baseType) noexcept {
        Core::MetadataDomain* metadata = Runtime(view).Metadata();
        return metadata != nullptr &&
            metadata->Types().IsDerivedFrom(
                object.RuntimeType(), baseType);
    }

private:
    static ViewRuntime& Runtime(View& view) noexcept {
        return *static_cast<ViewRuntime*>(
            view.IntegrationRuntime());
    }
};

} // namespace Aero::Detail
