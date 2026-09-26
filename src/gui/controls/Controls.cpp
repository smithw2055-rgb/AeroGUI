#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include "gui/media/BrushRendering.hpp"

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;

void Control::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    // A templated Control delegates its chrome to the template. Painting the
    // base Background as well produces an extra full-control rectangle behind
    // custom ComboBox, TreeView, Button and similar templates.
    if (GetTemplateRoot() != nullptr) return;
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    static_cast<void>(PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height}));
}

} // namespace Aero::Controls
