#pragma once

#include <Aero/RoutedCommand.hpp>

namespace Aero::Input {

// WPF ApplicationCommands surface. Gallery and tutorial Command/KeyBinding
// strings such as "Copy" and "ApplicationCommands.Copy", plus
// `{x:Static ApplicationCommands.Copy}`, resolve to these interned instances.
class AERO_GUI_API ApplicationCommands : public Base::Object {
    AERO_DECLARE_TYPE(ApplicationCommands, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    static Result<void> RegisterDefaults() noexcept;
    static Result<Ref<RoutedCommand>> Cut() noexcept;
    static Result<Ref<RoutedCommand>> Copy() noexcept;
    static Result<Ref<RoutedCommand>> Paste() noexcept;
    static Result<Ref<RoutedCommand>> Undo() noexcept;
    static Result<Ref<RoutedCommand>> Redo() noexcept;
    static Result<Ref<RoutedCommand>> Delete() noexcept;
    static Result<Ref<RoutedCommand>> Find() noexcept;
    static Result<Ref<RoutedCommand>> Replace() noexcept;
    static Result<Ref<RoutedCommand>> SelectAll() noexcept;
    static Result<Ref<RoutedCommand>> Help() noexcept;
    static Result<Ref<RoutedCommand>> New() noexcept;
    static Result<Ref<RoutedCommand>> Open() noexcept;
    static Result<Ref<RoutedCommand>> Save() noexcept;
    static Result<Ref<RoutedCommand>> SaveAs() noexcept;
    static Result<Ref<RoutedCommand>> Print() noexcept;
    static Result<Ref<RoutedCommand>> PrintPreview() noexcept;
    static Result<Ref<RoutedCommand>> Properties() noexcept;
    static Result<Ref<RoutedCommand>> Close() noexcept;
    static Result<Ref<RoutedCommand>> Stop() noexcept;
    static Result<Ref<RoutedCommand>> ContextMenu() noexcept;
    static Result<Ref<RoutedCommand>> CorrectionList() noexcept;
    static Result<Ref<RoutedCommand>> NotACommand() noexcept;

private:
    ApplicationCommands() noexcept = default;
};

} // namespace Aero::Input
