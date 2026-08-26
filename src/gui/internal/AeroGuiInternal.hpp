#pragma once

// Kernel-private operations. Not installed. Implementation TUs include this
// header. Public WPF types grant friendship only to AeroGuiInternal.
// Member clusters live in included section headers; there is still one friend type.

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Image.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/Menu.hpp>
#include <Aero/Controls/MenuItem.hpp>
#include <Aero/Controls/ContextMenu.hpp>
#include <Aero/Controls/ContextMenuService.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Visual.hpp>

#include "gui/core/VisualHandle.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/internal/PropertyStore.hpp"

namespace Aero {
class AnimationEngine;
class BindingEngine;
class EventRouter;
class InputRouter;
class LayoutEngine;
class StyleEngine;
class VisualStateManager;
class DataTemplate;
class Style;
namespace Controls {
class TemplateEngine;
class ControlBehavior;
class TextBlockLayout;
class ItemsPanelTemplate;
enum class ItemSubtreeChange : std::uint8_t { Mounted = 0U, Unmounting };
using ItemSubtreeCallback = Base::Result<void> (*)(
    ::Aero::Media::Visual& root,
    ItemSubtreeChange change,
    void* context) noexcept;
namespace Primitives { class Selector; }
} // namespace Controls
namespace Collections { class IItemsSource; }
namespace Render { struct MeshResources; class RenderTree; }
namespace Media { class DrawingContext; }
} // namespace Aero

namespace Aero {

class AeroGuiInternal {
public:
    AeroGuiInternal() = delete;

#include "gui/internal/AeroGuiInternal.Layout.hpp"
#include "gui/internal/AeroGuiInternal.Visual.hpp"
#include "gui/internal/AeroGuiInternal.Control.hpp"
#include "gui/internal/AeroGuiInternal.Property.hpp"
};

} // namespace Aero
