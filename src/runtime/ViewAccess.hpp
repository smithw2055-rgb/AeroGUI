#pragma once

#include <Aero/Base/Result.hpp>
#include "gui/ElementInternal.hpp"
#include "gui/BindingInternal.hpp"
#include <Aero/Data.hpp>
#include <Aero/View.hpp>
#include <Aero/Meta/MetadataId.hpp>

#include <memory>

namespace Aero {
class GuiContext;
class Visual;
namespace Controls {
class ItemContainerGenerator;
class VisualStateManager;
}
namespace Markup {
class DocumentCache;
class SourceProviderRegistry;
}
namespace Detail {
class ViewState;
class AnimationManager;
}
}

namespace Aero::Detail {

// Narrow source-side bridge for repository-owned diagnostics, reload support
// and tooling. It is implemented beside ViewState so the View does not
// expose its service graph through public or private accessor methods.
class ViewAccess final {
public:
    static Base::Result<Data::BindingHandle> AttachBinding(
        View& view,
        const Data::BindingDescriptor& descriptor) noexcept;
    static Base::Result<std::uint32_t> FlushBindings(View& view) noexcept;
    static std::unique_ptr<Controls::ItemContainerGenerator>
    CreateItemContainerGenerator(View& view);
    static Visual* RootVisual(View& view) noexcept;
    static AnimationManager* Animations(View& view) noexcept;
    static Controls::VisualStateManager* VisualStates(View& view) noexcept;
    static Controls::TemplateManager* Templates(View& view) noexcept;
    static bool IsInstanceOf(
        View& view,
        const Base::Object& object,
        Core::TypeId baseType) noexcept;
    static Markup::SourceProviderRegistry* Sources(View& view) noexcept;
    static Markup::DocumentCache* DocumentCache(View& view) noexcept;

private:
    static ViewState& State(View& view) noexcept;
};

} // namespace Aero::Detail
