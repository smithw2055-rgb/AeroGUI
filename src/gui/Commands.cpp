#include "gui/MetadataInternal.hpp"
#include <Aero/Input.hpp>
#include "InputInternal.hpp"


#include <cctype>
#include "gui/InputInternal.hpp"

namespace Aero::Input {

Base::Result<void> ICommand::TryAddCanExecuteChanged(
    const CanExecuteChangedHandler& handler) noexcept {
    return canExecuteChanged_.TryAdd(handler);
}

bool ICommand::RemoveCanExecuteChanged(
    const CanExecuteChangedHandler& handler) noexcept {
    return canExecuteChanged_.Remove(handler);
}

void ICommand::RaiseCanExecuteChanged() const noexcept {
    if (!canExecuteChanged_.Empty()) canExecuteChanged_.Invoke();
}

bool KeyGesture::Matches(const KeyboardInput& input) const noexcept {
    return IsValid() &&
        input.action == KeyboardAction::Down &&
        input.key == key_ &&
        input.modifiers == modifiers_;
}

RoutedCommand::RoutedCommand() noexcept
    : name_(&Base::GetDefaultAllocator()),
      gestures_(&Base::GetDefaultAllocator()) {}

RoutedCommand::RoutedCommand(Base::StringView name) noexcept
    : RoutedCommand() {
    Base::Result<void> assigned = name_.TryAssign(name);
    if (!assigned) {
        Base::ReportOutOfMemory(
            name.SizeBytes() + 1U, alignof(char), Base::MemoryTag::String);
    }
}

Base::Result<void> RoutedCommand::TrySetName(
    Base::StringView name) noexcept {
    return name_.TryAssign(name);
}

Base::Result<void> RoutedCommand::TryAddInputGesture(
    Base::Ref<InputGesture> gesture) noexcept {
    if (!gesture) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Input gesture must not be null");
    }
    return gestures_.TryPushBack(std::move(gesture));
}

bool RoutedCommand::MatchesInput(
    const KeyboardInput& input) const noexcept {
    for (const Base::Ref<InputGesture>& gesture : gestures_) {
        if (gesture && gesture->Matches(input)) return true;
    }
    return false;
}

namespace {

Base::StringView TrimAscii(Base::StringView value) noexcept {
    std::uint32_t first = 0U;
    std::uint32_t last = value.SizeBytes();
    while (first < last && (value[first] == ' ' || value[first] == '\t' ||
        value[first] == '\r' || value[first] == '\n')) ++first;
    while (last > first && (value[last - 1U] == ' ' ||
        value[last - 1U] == '\t' || value[last - 1U] == '\r' ||
        value[last - 1U] == '\n')) --last;
    return value.Substr(first, last - first);
}

Base::Result<std::uint32_t> ParseKeyName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed = TrimAscii(value);
    if (trimmed.SizeBytes() == 1U) {
        const unsigned char character =
            static_cast<unsigned char>(trimmed[0]);
        if (std::isalnum(character)) {
            return static_cast<std::uint32_t>(std::toupper(character));
        }
    }
    if (trimmed == Base::StringView("Enter")) return KeyboardKeyEnter;
    if (trimmed == Base::StringView("Escape")) return KeyboardKeyEscape;
    if (trimmed == Base::StringView("Space")) return KeyboardKeySpace;
    return Base::Status::Failure(Base::ErrorCode::Unsupported,
        "KeyBinding Key is not supported");
}

Base::Result<std::uint32_t> ParseModifiersName(
    Base::StringView value) noexcept {
    const Base::StringView trimmed = TrimAscii(value);
    if (trimmed.Empty() || trimmed == Base::StringView("None")) return 0U;
    std::uint32_t modifiers = 0U;
    std::uint32_t begin = 0U;
    while (begin < trimmed.SizeBytes()) {
        std::uint32_t end = begin;
        while (end < trimmed.SizeBytes() && trimmed[end] != '+' &&
               trimmed[end] != ',') ++end;
        const Base::StringView item = TrimAscii(
            trimmed.Substr(begin, end - begin));
        if (item == Base::StringView("Ctrl") ||
            item == Base::StringView("Control")) {
            modifiers |= static_cast<std::uint32_t>(KeyboardModifiers::Control);
        } else if (item == Base::StringView("Shift")) {
            modifiers |= static_cast<std::uint32_t>(KeyboardModifiers::Shift);
        } else if (item == Base::StringView("Alt")) {
            modifiers |= static_cast<std::uint32_t>(KeyboardModifiers::Alt);
        } else {
            return Base::Status::Failure(Base::ErrorCode::Unsupported,
                "KeyBinding modifier is not supported");
        }
        begin = end + 1U;
    }
    return modifiers;
}

} // namespace

Base::Result<void> KeyBinding::SetCommandName(
    Base::StringView value) noexcept {
    command_.Reset();
    return commandName_.TryAssign(TrimAscii(value));
}

Base::Result<void> KeyBinding::SetKeyName(
    Base::StringView value) noexcept {
    command_.Reset();
    return keyName_.TryAssign(TrimAscii(value));
}

Base::Result<void> KeyBinding::SetModifiersName(
    Base::StringView value) noexcept {
    command_.Reset();
    return modifiersName_.TryAssign(TrimAscii(value));
}

Base::Result<void> KeyBinding::Finalize() noexcept {
    if (command_) return {};
    if (commandName_.Empty() || keyName_.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "KeyBinding requires Command and Key");
    }
    Base::Result<std::uint32_t> key = ParseKeyName(keyName_.View());
    if (!key) return key.GetStatus();
    Base::Result<std::uint32_t> modifiers = ParseModifiersName(
        modifiersName_.View());
    if (!modifiers) return modifiers.GetStatus();
    Base::Result<Base::Ref<RoutedCommand>> command =
        Base::MakeRef<RoutedCommand>(commandName_.View());
    if (!command) return command.GetStatus();
    Base::Result<Base::Ref<KeyGesture>> gesture =
        Base::MakeRef<KeyGesture>(key.Value(), modifiers.Value());
    if (!gesture) return gesture.GetStatus();
    Base::Result<void> added = command.Value()->TryAddInputGesture(
        Base::Ref<InputGesture>(std::move(gesture).Value()));
    if (!added) return added.GetStatus();
    command_ = std::move(command).Value();
    return {};
}

Base::Result<bool> RoutedCommand::CanExecute(
    const Core::Value& parameter,
    UIElement* target) noexcept {
    Aero::Detail::InputRouter* input = target != nullptr
        ? Aero::Detail::ElementPrivate::InputRouterFor(*target)
        : nullptr;
    if (input == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "RoutedCommand requires a loaded command target");
    }
    return input->CanExecute(*this, parameter, *target);
}

Base::Result<void> RoutedCommand::Execute(
    const Core::Value& parameter,
    UIElement* target) noexcept {
    Aero::Detail::InputRouter* input = target != nullptr
        ? Aero::Detail::ElementPrivate::InputRouterFor(*target)
        : nullptr;
    if (input == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "RoutedCommand requires a loaded command target");
    }
    Base::Result<bool> executed =
        input->Execute(*this, parameter, *target);
    if (!executed) return executed.GetStatus();
    return {};
}

} // namespace Aero::Input

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Input;

CommandState::CommandState(ElementTree& tree, EventRouter& events) noexcept
    : tree_(&tree),
      events_(&events),
      bindings_(&Base::GetDefaultAllocator()) {}

Base::Result<void> CommandState::VerifyTarget(
    UIElement& target) const noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Command routing requires an ElementTree root");
    }
    Base::Result<void> access = root->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!target.GetIsLoaded() || Aero::Detail::ElementPrivate::Tree(target) != tree_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Command target must be loaded in the command tree");
    }
    return {};
}

void CommandState::PruneStaleBindings() noexcept {
    std::uint32_t destination = 0U;
    for (std::uint32_t index = 0U; index < bindings_.Size(); ++index) {
        if (tree_->ResolveHandle(bindings_[index].owner) == nullptr) continue;
        if (destination != index) {
            bindings_[destination] = std::move(bindings_[index]);
        }
        ++destination;
    }
    while (bindings_.Size() > destination) bindings_.PopBack();
}

void CommandState::PruneStaleInputBindings() noexcept {
    std::uint32_t destination = 0U;
    for (std::uint32_t index = 0U;
         index < inputBindings_.Size(); ++index) {
        if (tree_->ResolveHandle(inputBindings_[index].owner) == nullptr) {
            continue;
        }
        if (destination != index) {
            inputBindings_[destination] = std::move(inputBindings_[index]);
        }
        ++destination;
    }
    while (inputBindings_.Size() > destination) inputBindings_.PopBack();
}

Base::Result<CommandBindingHandle> CommandState::TryAddBinding(
    UIElement& owner,
    const CommandBinding& binding) noexcept {
    Base::Result<void> verified = VerifyTarget(owner);
    if (!verified) return verified.GetStatus();
    if (!binding.IsValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Command binding requires a command and at least one handler");
    }
    Base::Result<VisualHandle> ownerHandle = tree_->GetHandle(owner);
    if (!ownerHandle) return ownerHandle.GetStatus();
    if (nextBinding_ == 0U) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "Command binding handle space exhausted");
    }
    BindingRecord record;
    record.handle.value = nextBinding_++;
    record.owner = ownerHandle.Value();
    record.binding = binding;
    Base::Result<void> appended =
        bindings_.TryPushBack(std::move(record));
    if (!appended) return appended.GetStatus();
    return bindings_[bindings_.Size() - 1U].handle;
}

Base::Result<bool> CommandState::RemoveBinding(
    CommandBindingHandle handle) noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Command binding removal requires an ElementTree root");
    }
    Base::Result<void> access = root->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!handle.IsValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Command binding handle is invalid");
    }
    for (std::uint32_t index = 0U; index < bindings_.Size(); ++index) {
        if (bindings_[index].handle.value != handle.value) continue;
        for (std::uint32_t next = index + 1U;
             next < bindings_.Size(); ++next) {
            bindings_[next - 1U] = std::move(bindings_[next]);
        }
        bindings_.PopBack();
        return true;
    }
    return false;
}

Base::Result<InputBindingHandle> CommandState::TryAddInputBinding(
    UIElement& owner,
    Base::Ref<KeyBinding> binding) noexcept {
    Base::Result<void> verified = VerifyTarget(owner);
    if (!verified) return verified.GetStatus();
    if (!binding) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "InputBinding cannot be null");
    }
    Base::Result<void> finalized = binding->Finalize();
    if (!finalized) return finalized.GetStatus();
    if (!binding->GetCommand()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "InputBinding command was not created");
    }
    Base::Result<VisualHandle> ownerHandle = tree_->GetHandle(owner);
    if (!ownerHandle) return ownerHandle.GetStatus();
    if (nextInputBinding_ == 0U) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "InputBinding handle space exhausted");
    }
    InputBindingRecord record;
    record.handle.value = nextInputBinding_++;
    record.owner = ownerHandle.Value();
    record.binding = std::move(binding);
    Base::Result<void> appended = inputBindings_.TryPushBack(std::move(record));
    if (!appended) return appended.GetStatus();
    return inputBindings_.Back().handle;
}

Base::Result<bool> CommandState::CanExecute(
    ICommand& command,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    if (command.RuntimeType() == RoutedCommand::StaticTypeId()) {
        return CanExecute(
            static_cast<RoutedCommand&>(command), parameter, target);
    }
    return command.CanExecute(parameter, &target);
}

Base::Result<bool> CommandState::Execute(
    ICommand& command,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    if (command.RuntimeType() == RoutedCommand::StaticTypeId()) {
        return Execute(
            static_cast<RoutedCommand&>(command), parameter, target);
    }
    Base::Result<bool> allowed = command.CanExecute(parameter, &target);
    if (!allowed || !allowed.Value()) {
        return allowed ? Base::Result<bool>(false)
                       : Base::Result<bool>(allowed.GetStatus());
    }
    Base::Result<void> executed = command.Execute(parameter, &target);
    return executed
        ? Base::Result<bool>(true)
        : Base::Result<bool>(executed.GetStatus());
}

Base::Result<bool> CommandState::CanExecute(
    RoutedCommand& command,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    Base::Result<void> verified = VerifyTarget(target);
    if (!verified) return verified.GetStatus();
    PruneStaleBindings();

    CanExecuteRoutedEventArgs args;
    args.SetCommand(&command);
    args.SetParameter(parameter);
    args.SetTarget(&target);
    Base::Result<void> routed = events_->VisitRoute(
        target, RoutingStrategy::Bubble,
        [&](DependencyObject& owner) noexcept {
            if (!owner.PropertyRegistry().Types().IsDerivedFrom(
                    owner.RuntimeType(), UIElement::StaticTypeId())) {
                return true;
            }
            auto& element = static_cast<UIElement&>(owner);
            const VisualHandle ownerHandle = Aero::Detail::ElementPrivate::Handle(element);
            for (const BindingRecord& record : bindings_) {
                if (record.owner.index != ownerHandle.index ||
                    record.owner.generation != ownerHandle.generation ||
                    record.binding.GetCommand() != &command) {
                    continue;
                }
                const CanExecuteRoutedEventHandler& handler = record.binding.GetCanExecute();
                if (!handler.Empty()) {
                    handler.Invoke(&element, args);
                } else if (!record.binding.GetExecuted().Empty()) {
                    args.SetCanExecute(true);
                }
                if (args.GetHandled() || !args.GetContinueRouting()) return false;
            }
            return true;
        });
    if (!routed) return routed.GetStatus();
    return args.GetCanExecute();
}

Base::Result<bool> CommandState::Execute(
    RoutedCommand& command,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    if (!target.GetIsEnabled()) return false;
    Base::Result<bool> allowed = CanExecute(command, parameter, target);
    if (!allowed || !allowed.Value()) {
        return allowed ? Base::Result<bool>(false)
                       : Base::Result<bool>(allowed.GetStatus());
    }

    PruneStaleBindings();
    ExecutedRoutedEventArgs args;
    args.SetCommand(&command);
    args.SetParameter(parameter);
    args.SetTarget(&target);
    bool invoked = false;
    Base::Result<void> routed = events_->VisitRoute(
        target, RoutingStrategy::Bubble,
        [&](DependencyObject& owner) noexcept {
            if (!owner.PropertyRegistry().Types().IsDerivedFrom(
                    owner.RuntimeType(), UIElement::StaticTypeId())) {
                return true;
            }
            auto& element = static_cast<UIElement&>(owner);
            const VisualHandle ownerHandle = Aero::Detail::ElementPrivate::Handle(element);
            for (const BindingRecord& record : bindings_) {
                if (record.owner.index != ownerHandle.index ||
                    record.owner.generation != ownerHandle.generation ||
                    record.binding.GetCommand() != &command) {
                    continue;
                }
                const ExecutedRoutedEventHandler& handler = record.binding.GetExecuted();
                if (!handler.Empty()) {
                    handler.Invoke(&element, args);
                    invoked = true;
                }
                if (args.GetHandled() || !args.GetContinueRouting()) return false;
            }
            return true;
        });
    if (!routed) return routed.GetStatus();
    return invoked;
}

Base::Result<bool> CommandState::ProcessInput(
    UIElement& target,
    const KeyboardInput& input) noexcept {
    if (input.action != KeyboardAction::Down || !target.GetIsEnabled()) return false;
    Base::Result<void> verified = VerifyTarget(target);
    if (!verified) return verified.GetStatus();

    PruneStaleBindings();
    PruneStaleInputBindings();
    Base::Result<bool> result(false);
    Base::Result<void> routed = events_->VisitRoute(
        target, RoutingStrategy::Bubble,
        [&](DependencyObject& current) noexcept {
            if (!current.PropertyRegistry().Types().IsDerivedFrom(
                    current.RuntimeType(), UIElement::StaticTypeId())) {
                return true;
            }
            auto& element = static_cast<UIElement&>(current);
            const VisualHandle owner = Aero::Detail::ElementPrivate::Handle(element);
            for (const InputBindingRecord& record : inputBindings_) {
                if (record.owner.index != owner.index ||
                    record.owner.generation != owner.generation ||
                    !record.binding || !record.binding->GetCommand()) {
                    continue;
                }
                RoutedCommand& command = *record.binding->GetCommand();
                if (command.MatchesInput(input)) {
                    result = Execute(command, Core::Value::Unset(), target);
                    return false;
                }
            }
            for (const BindingRecord& record : bindings_) {
                if (record.owner.index != owner.index ||
                    record.owner.generation != owner.generation) {
                    continue;
                }
                RoutedCommand* command = record.binding.GetCommand();
                if (command == nullptr || !command->MatchesInput(input)) continue;
                Base::Result<bool> allowed = CanExecute(*command, Core::Value::Unset(), target);
                if (!allowed) {
                    result = allowed.GetStatus();
                    return false;
                }
                if (allowed.Value()) {
                    result = Execute(*command, Core::Value::Unset(), target);
                    return false;
                }
            }
            return true;
        });
    if (!routed) return routed.GetStatus();
    return result;
}

Base::Result<void> CommandState::TryAddRequerySuggested(
    const RequerySuggestedHandler& handler) noexcept {
    return requerySuggested_.TryAdd(handler);
}

bool CommandState::RemoveRequerySuggested(
    const RequerySuggestedHandler& handler) noexcept {
    return requerySuggested_.Remove(handler);
}

void CommandState::InvalidateRequerySuggested() const noexcept {
    if (!requerySuggested_.Empty()) requerySuggested_.Invoke();
}

} // namespace Aero::Detail
