#pragma once

#include <Aero/RoutedCommand.hpp>

namespace Aero::Input {

// WPF ApplicationCommands surface. Gallery and tutorial Command/KeyBinding
// strings such as "Copy" and "ApplicationCommands.Copy", plus
// `{x:Static ApplicationCommands.Copy}`, resolve to these interned instances.
class AERO_GUI_API ApplicationCommands : public Base::Object {
    AERO_DECLARE_TYPE(ApplicationCommands, Base::Object)

public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    static void RegisterDefaults() noexcept;
    static Ref<RoutedCommand> Cut() noexcept;
    static Ref<RoutedCommand> Copy() noexcept;
    static Ref<RoutedCommand> Paste() noexcept;
    static Ref<RoutedCommand> Undo() noexcept;
    static Ref<RoutedCommand> Redo() noexcept;
    static Ref<RoutedCommand> Delete() noexcept;
    static Ref<RoutedCommand> Find() noexcept;
    static Ref<RoutedCommand> Replace() noexcept;
    static Ref<RoutedCommand> SelectAll() noexcept;
    static Ref<RoutedCommand> Help() noexcept;
    static Ref<RoutedCommand> New() noexcept;
    static Ref<RoutedCommand> Open() noexcept;
    static Ref<RoutedCommand> Save() noexcept;
    static Ref<RoutedCommand> SaveAs() noexcept;
    static Ref<RoutedCommand> Print() noexcept;
    static Ref<RoutedCommand> PrintPreview() noexcept;
    static Ref<RoutedCommand> Properties() noexcept;
    static Ref<RoutedCommand> Close() noexcept;
    static Ref<RoutedCommand> Stop() noexcept;
    static Ref<RoutedCommand> ContextMenu() noexcept;
    static Ref<RoutedCommand> CorrectionList() noexcept;
    static Ref<RoutedCommand> NotACommand() noexcept;

private:
    ApplicationCommands() noexcept = default;
};

} // namespace Aero::Input
