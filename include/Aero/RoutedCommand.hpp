#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Events/CommandEventArgs.hpp>

namespace Aero::Input {
class KeyBinding;

class AERO_GUI_API RoutedCommand : public ICommand {
    AERO_DECLARE_TYPE(RoutedCommand, ICommand)
public:
    RoutedCommand() noexcept;
    explicit RoutedCommand(StringView name) noexcept;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    StringView GetName() const noexcept { return name_.View(); }
    void SetName(StringView name) noexcept;
    void AddInputGesture(Ref<InputGesture> gesture) noexcept;
    Span<const Ref<InputGesture>> GetInputGestures() const noexcept {
        return {gestures_.Data(), gestures_.Size()};
    }
    bool MatchesInput(const KeyboardInput& input) const noexcept;
    bool MatchesPointer(const PointerInput& input) const noexcept;

    Result<bool> CanExecute(
        const Value& parameter,
        UIElement* target = nullptr) noexcept override;
    void Execute(
        const Value& parameter,
        UIElement* target = nullptr) noexcept override;

    void InvalidateCanExecute() const noexcept {
        RaiseCanExecuteChanged();
    }

    // Registers and resolves process-stable WPF-style static command members.
    // Module metadata registers the supported names once; XAML and control
    // bindings then resolve the same command object identity.
    static Result<void> RegisterStatic(
        Meta::TypeId ownerType,
        StringView memberName) noexcept;
    static Result<Ref<RoutedCommand>> ResolveStatic(
        Meta::TypeId ownerType,
        StringView memberName) noexcept;
    // Interns `Copy`, `ApplicationCommands.Copy`, and other authored names
    // onto one RoutedCommand instance so CommandBinding and KeyBinding match.
    static Result<Ref<RoutedCommand>> ResolveAuthored(
        StringView name) noexcept;

private:
    friend class KeyBinding;

    String name_;
    Base::Vector<Ref<InputGesture>> gestures_;
};
} // namespace Aero::Input
