#ifndef __AERO_DRAGDROP_HPP__
#define __AERO_DRAGDROP_HPP__

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Input.hpp>
#include <Aero/RoutedEvent.hpp>

namespace Aero {

class DependencyObject;

/// Provides helper methods and fields for initiating drag-and-drop
/// operations. Exposes the attached routed events raised during a drag and a
/// DoDragDrop entry point that integrates with the active input router.
///
/// Reference: System.Windows.DragDrop
class AERO_GUI_API DragDrop : public Base::Object {
    AERO_DECLARE_TYPE(DragDrop, Base::Object)
public:
    static const RoutedEventRef<DragDrop, DragEventArgs> PreviewDragEnterEvent;
    static const RoutedEventRef<DragDrop, DragEventArgs> DragEnterEvent;
    static const RoutedEventRef<DragDrop, DragEventArgs> PreviewDragOverEvent;
    static const RoutedEventRef<DragDrop, DragEventArgs> DragOverEvent;
    static const RoutedEventRef<DragDrop, DragEventArgs> PreviewDragLeaveEvent;
    static const RoutedEventRef<DragDrop, DragEventArgs> DragLeaveEvent;
    static const RoutedEventRef<DragDrop, DragEventArgs> PreviewDropEvent;
    static const RoutedEventRef<DragDrop, DragEventArgs> DropEvent;

    /// Initiates a drag-and-drop operation with the supplied payload. Returns
    /// the effects that were allowed. The actual drop effect is applied by the
    /// input engine while the drag is in progress.
    static Input::DragDropEffects DoDragDrop(
        ::Aero::DependencyObject* source,
        const Base::Ref<Base::Object>& data,
        Input::DragDropEffects allowedEffects) noexcept;

protected:
    DragDrop() = default;
};

} // namespace Aero

#endif
