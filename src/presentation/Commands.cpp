#include <Aero/Presentation/Commands.hpp>

#include <Aero/Presentation/Input.hpp>

namespace Aero::Presentation {

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

Base::Result<bool> RoutedCommand::CanExecute(
    CommandManager& manager,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    return manager.CanExecute(*this, parameter, target);
}

Base::Result<void> RoutedCommand::Execute(
    CommandManager& manager,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    Base::Result<bool> executed =
        manager.Execute(*this, parameter, target);
    if (!executed) return executed.GetStatus();
    return {};
}

CommandManager::CommandManager(ObjectTree& tree) noexcept
    : tree_(&tree),
      bindings_(&Base::GetDefaultAllocator()) {}

Base::Result<void> CommandManager::VerifyTarget(
    UIElement& target) const noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Command routing requires an ObjectTree root");
    }
    Base::Result<void> access = root->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!target.IsLoaded() || target.OwningTree() != tree_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Command target must be loaded in the command tree");
    }
    return {};
}

void CommandManager::PruneStaleBindings() noexcept {
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

Base::Result<CommandBindingHandle> CommandManager::TryAddBinding(
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

Base::Result<bool> CommandManager::RemoveBinding(
    CommandBindingHandle handle) noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Command binding removal requires an ObjectTree root");
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

Base::Result<void> CommandManager::SnapshotRoute(
    UIElement& target,
    RoutedCommand* command,
    Base::Vector<RouteBinding>& route) noexcept {
    Base::Result<void> verified = VerifyTarget(target);
    if (!verified) return verified.GetStatus();
    PruneStaleBindings();

    Visual* current = &target;
    while (current != nullptr) {
        const VisualHandle owner = current->Handle();
        for (const BindingRecord& record : bindings_) {
            if (record.owner.index != owner.index ||
                record.owner.generation != owner.generation ||
                (command != nullptr &&
                 record.binding.Command() != command)) {
                continue;
            }
            Base::Result<void> appended =
                route.TryPushBack({record.owner, record.binding});
            if (!appended) return appended.GetStatus();
        }
        current = current->LogicalParent() != nullptr
            ? current->LogicalParent()
            : current->VisualParent();
    }
    return {};
}

Base::Result<bool> CommandManager::CanExecute(
    RoutedCommand& command,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    Base::Vector<RouteBinding> route(&Base::GetDefaultAllocator());
    Base::Result<void> snapshot =
        SnapshotRoute(target, &command, route);
    if (!snapshot) return snapshot.GetStatus();

    CanExecuteRoutedEventArgs args;
    args.command = &command;
    args.parameter = parameter;
    args.target = &target;
    for (const RouteBinding& item : route) {
        Visual* owner = tree_->ResolveHandle(item.owner);
        if (owner == nullptr) continue;
        const CanExecuteRoutedEventHandler& handler =
            item.binding.CanExecuteHandler();
        if (!handler.Empty()) {
            handler.Invoke(owner, args);
        } else if (!item.binding.ExecutedHandler().Empty()) {
            args.canExecute = true;
        }
        if (args.handled || !args.continueRouting) break;
    }
    return args.canExecute;
}

Base::Result<bool> CommandManager::Execute(
    RoutedCommand& command,
    const Core::Value& parameter,
    UIElement& target) noexcept {
    if (!target.IsEnabled()) return false;
    Base::Result<bool> allowed =
        CanExecute(command, parameter, target);
    if (!allowed || !allowed.Value()) {
        return allowed ? Base::Result<bool>(false)
                       : Base::Result<bool>(allowed.GetStatus());
    }

    Base::Vector<RouteBinding> route(&Base::GetDefaultAllocator());
    Base::Result<void> snapshot =
        SnapshotRoute(target, &command, route);
    if (!snapshot) return snapshot.GetStatus();
    ExecutedRoutedEventArgs args;
    args.command = &command;
    args.parameter = parameter;
    args.target = &target;
    bool invoked = false;
    for (const RouteBinding& item : route) {
        Visual* owner = tree_->ResolveHandle(item.owner);
        if (owner == nullptr) continue;
        const ExecutedRoutedEventHandler& handler =
            item.binding.ExecutedHandler();
        if (!handler.Empty()) {
            handler.Invoke(owner, args);
            invoked = true;
        }
        if (args.handled || !args.continueRouting) break;
    }
    return invoked;
}

Base::Result<bool> CommandManager::ProcessInput(
    UIElement& target,
    const KeyboardInput& input) noexcept {
    if (input.action != KeyboardAction::Down ||
        !target.IsEnabled()) return false;
    Base::Vector<RouteBinding> route(&Base::GetDefaultAllocator());
    Base::Result<void> snapshot =
        SnapshotRoute(target, nullptr, route);
    if (!snapshot) return snapshot.GetStatus();

    for (const RouteBinding& item : route) {
        RoutedCommand* command = item.binding.Command();
        if (command == nullptr || !command->MatchesInput(input)) continue;
        Base::Result<bool> allowed =
            CanExecute(*command, Core::Value::Unset(), target);
        if (!allowed) return allowed.GetStatus();
        if (!allowed.Value()) continue;
        return Execute(*command, Core::Value::Unset(), target);
    }
    return false;
}

Base::Result<void> CommandManager::TryAddRequerySuggested(
    const RequerySuggestedHandler& handler) noexcept {
    return requerySuggested_.TryAdd(handler);
}

bool CommandManager::RemoveRequerySuggested(
    const RequerySuggestedHandler& handler) noexcept {
    return requerySuggested_.Remove(handler);
}

void CommandManager::InvalidateRequerySuggested() const noexcept {
    if (!requerySuggested_.Empty()) requerySuggested_.Invoke();
}

} // namespace Aero::Presentation
