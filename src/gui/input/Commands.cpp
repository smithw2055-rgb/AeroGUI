#include "gui/core/State.hpp" 
#include "gui/input/InputState.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/KeyGesture.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/KeyBinding.hpp>
#include <Aero/MouseBinding.hpp>
#include <Aero/RoutedUICommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/FocusManager.hpp>
#include <Aero/ApplicationCommands.hpp>


#include <cctype>
#include <mutex>

namespace Aero::Input {

void ICommand::AddCanExecuteChanged(
    const CanExecuteChangedHandler& handler) noexcept {
    canExecuteChanged_.Add(handler);
}

bool ICommand::RemoveCanExecuteChanged(
    const CanExecuteChangedHandler& handler) noexcept {
    return canExecuteChanged_.Remove(handler);
}

void ICommand::RaiseCanExecuteChanged() const noexcept {
    if (!canExecuteChanged_.Empty()) canExecuteChanged_.Invoke();
}


namespace {

struct StaticRoutedCommandEntry {
    Meta::TypeId ownerType = Meta::InvalidTypeId;
    Base::String memberName;
    Base::Ref<RoutedCommand> command;
};

Base::Vector<StaticRoutedCommandEntry>& StaticRoutedCommands() noexcept {
    static Base::Vector<StaticRoutedCommandEntry> commands;
    return commands;
}

std::mutex& StaticRoutedCommandMutex() noexcept {
    static std::mutex mutex;
    return mutex;
}

struct InternedCommandEntry {
    Base::String name;
    Base::Ref<RoutedCommand> command;
};

Base::Vector<InternedCommandEntry>& InternedCommands() noexcept {
    static Base::Vector<InternedCommandEntry> commands;
    return commands;
}

Base::StringView LastDottedSegment(Base::StringView value) noexcept {
    std::uint32_t last = value.SizeBytes();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        if (value[index] == '.') last = index;
    }
    if (last == value.SizeBytes()) return value;
    return value.Substr(last + 1U, value.SizeBytes() - last - 1U);
}

Base::StringView OwnerDottedSegment(Base::StringView value) noexcept {
    std::uint32_t last = value.SizeBytes();
    for (std::uint32_t index = 0U; index < value.SizeBytes(); ++index) {
        if (value[index] == '.') last = index;
    }
    if (last == value.SizeBytes()) return {};
    return value.Substr(0U, last);
}

} // namespace

Base::Result<void> RoutedCommand::RegisterStatic(
    Meta::TypeId ownerType,
    Base::StringView memberName) noexcept {
    if (ownerType == Meta::InvalidTypeId || memberName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Static routed command requires an owner type and member name");
    }
    std::lock_guard<std::mutex> lock(StaticRoutedCommandMutex());
    for (const StaticRoutedCommandEntry& entry : StaticRoutedCommands()) {
        if (entry.ownerType == ownerType &&
            entry.memberName.View() == memberName) {
            return {};
        }
    }
    Base::Result<Base::Ref<RoutedCommand>> created =
        Base::MakeRef<RoutedCommand>(memberName);
    if (!created) return created.GetStatus();
    StaticRoutedCommandEntry entry;
    entry.ownerType = ownerType;
    Base::Result<void> assigned = entry.memberName.Assign(memberName);
    if (!assigned) return assigned.GetStatus();
    entry.command = std::move(created).Value();
    return StaticRoutedCommands().PushBack(std::move(entry));
}

Base::Result<Base::Ref<RoutedCommand>> RoutedCommand::ResolveStatic(
    Meta::TypeId ownerType,
    Base::StringView memberName) noexcept {
    if (ownerType == Meta::InvalidTypeId || memberName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Static routed command requires an owner type and member name");
    }
    std::lock_guard<std::mutex> lock(StaticRoutedCommandMutex());
    for (const StaticRoutedCommandEntry& entry : StaticRoutedCommands()) {
        if (entry.ownerType == ownerType &&
            entry.memberName.View() == memberName) {
            return entry.command;
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Static routed command member is not registered");
}

Base::Result<Base::Ref<RoutedCommand>> RoutedCommand::ResolveAuthored(
    Base::StringView name) noexcept {
    Base::StringView authored = name;
    std::uint32_t begin = 0U;
    std::uint32_t end = authored.SizeBytes();
    while (begin < end &&
           (authored[begin] == ' ' || authored[begin] == '\t' ||
            authored[begin] == '\r' || authored[begin] == '\n')) {
        ++begin;
    }
    while (end > begin &&
           (authored[end - 1U] == ' ' || authored[end - 1U] == '\t' ||
            authored[end - 1U] == '\r' || authored[end - 1U] == '\n')) {
        --end;
    }
    authored = authored.Substr(begin, end - begin);
    if (authored.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Command name cannot be empty");
    }
    const Base::StringView owner = OwnerDottedSegment(authored);
    const Base::StringView member = owner.Empty()
        ? authored
        : LastDottedSegment(authored);
    const Base::StringView ownerTypeName = LastDottedSegment(owner);

    std::lock_guard<std::mutex> lock(StaticRoutedCommandMutex());
    if (!owner.Empty()) {
        for (const StaticRoutedCommandEntry& entry : StaticRoutedCommands()) {
            if (entry.memberName.View() != member) continue;
            if (ownerTypeName == Base::StringView("ApplicationCommands") &&
                entry.ownerType == ApplicationCommands::StaticTypeId()) {
                return entry.command;
            }
        }
        for (const StaticRoutedCommandEntry& entry : StaticRoutedCommands()) {
            if (entry.memberName.View() == member) return entry.command;
        }
    }
    for (const StaticRoutedCommandEntry& entry : StaticRoutedCommands()) {
        if (entry.memberName.View() == member) return entry.command;
    }
    for (const InternedCommandEntry& entry : InternedCommands()) {
        if (entry.name.View() == member || entry.name.View() == authored) {
            return entry.command;
        }
    }
    Base::Result<Base::Ref<RoutedCommand>> created =
        Base::MakeRef<RoutedCommand>(member);
    if (!created) return created.GetStatus();
    InternedCommandEntry interned;
    Base::Result<void> assigned = interned.name.Assign(member);
    if (!assigned) return assigned.GetStatus();
    interned.command = created.Value();
    Base::Result<void> stored = InternedCommands().PushBack(std::move(interned));
    if (!stored) return stored.GetStatus();
    return created;
}

Base::Result<void> ApplicationCommands::RegisterDefaults() noexcept {
    constexpr Base::StringView names[] = {
        "Cut", "Copy", "Paste", "Undo", "Redo", "Delete", "Find", "Replace",
        "SelectAll", "Help", "New", "Open", "Save", "SaveAs", "Print",
        "PrintPreview", "Properties", "Close", "Stop", "ContextMenu",
        "CorrectionList", "NotACommand"};
    for (Base::StringView commandName : names) {
        Base::Result<void> registered =
            RoutedCommand::RegisterStatic(StaticTypeId(), commandName);
        if (!registered) return registered.GetStatus();
    }
    return {};
}

Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Cut() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Cut");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Copy() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Copy");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Paste() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Paste");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Undo() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Undo");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Redo() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Redo");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Delete() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Delete");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Find() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Find");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Replace() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Replace");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::SelectAll() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "SelectAll");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Help() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Help");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::New() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "New");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Open() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Open");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Save() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Save");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::SaveAs() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "SaveAs");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Print() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Print");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::PrintPreview() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "PrintPreview");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Properties() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Properties");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Close() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Close");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::Stop() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "Stop");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::ContextMenu() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "ContextMenu");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::CorrectionList() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "CorrectionList");
}
Base::Result<Base::Ref<RoutedCommand>> ApplicationCommands::NotACommand() noexcept {
    return RoutedCommand::ResolveStatic(StaticTypeId(), "NotACommand");
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
    Base::Result<void> assigned = name_.Assign(name);
    if (!assigned) {
        Base::ReportOutOfMemory(
            name.SizeBytes() + 1U, alignof(char), Base::MemoryTag::String);
    }
}

void RoutedCommand::SetName(Base::StringView name) noexcept {
    static_cast<void>(name_.Assign(name));
}

void RoutedCommand::AddInputGesture(
    Base::Ref<InputGesture> gesture) noexcept {
    if (!gesture) return;
    static_cast<void>(gestures_.PushBack(std::move(gesture)));
}

bool RoutedCommand::MatchesInput(
    const KeyboardInput& input) const noexcept {
    for (const Base::Ref<InputGesture>& gesture : gestures_) {
        if (gesture && gesture->Matches(input)) return true;
    }
    return false;
}

bool RoutedCommand::MatchesPointer(
    const PointerInput& input) const noexcept {
    for (const Base::Ref<InputGesture>& gesture : gestures_) {
        if (gesture && gesture->MatchesPointer(input)) return true;
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

void CommandBinding::SetCommandName(Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(TrimAscii(value))) return;
    commandName_ = std::move(candidate);
    command_.Reset();
}

void CommandBinding::SetExecutedName(Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(TrimAscii(value))) return;
    executedName_ = std::move(candidate);
}

void CommandBinding::SetCanExecuteName(Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(TrimAscii(value))) return;
    canExecuteName_ = std::move(candidate);
}

Base::Result<void> CommandBinding::Finalize() noexcept {
    if (command_) return {};
    if (commandName_.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "CommandBinding requires Command");
    }
    Base::Result<Base::Ref<RoutedCommand>> command =
        RoutedCommand::ResolveAuthored(commandName_.View());
    if (!command) return command.GetStatus();
    command_ = std::move(command).Value();
    return {};
}

void KeyBinding::SetCommandName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(TrimAscii(value))) return;
    commandName_ = std::move(candidate);
    command_.Reset();
    finalized_ = false;
}

void KeyBinding::SetKeyName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(TrimAscii(value))) return;
    keyName_ = std::move(candidate);
    command_.Reset();
    finalized_ = false;
}

void KeyBinding::SetModifiersName(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(TrimAscii(value))) return;
    modifiersName_ = std::move(candidate);
    command_.Reset();
    finalized_ = false;
}

Base::Result<void> KeyBinding::Finalize() noexcept {
    if (finalized_) return {};
    if (commandName_.Empty() && !command_) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "KeyBinding requires Command and Key");
    }
    if (keyName_.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "KeyBinding requires Command and Key");
    }
    if (!command_) {
        Base::Result<Base::Ref<RoutedCommand>> command =
            RoutedCommand::ResolveAuthored(commandName_.View());
        if (!command) return command.GetStatus();
        command_ = std::move(command).Value();
    }
    Base::Result<std::uint32_t> key = ParseKeyName(keyName_.View());
    if (!key) return key.GetStatus();
    Base::Result<std::uint32_t> modifiers = ParseModifiersName(
        modifiersName_.View());
    if (!modifiers) return modifiers.GetStatus();
    Base::Result<Base::Ref<KeyGesture>> gesture =
        Base::MakeRef<KeyGesture>(key.Value(), modifiers.Value());
    if (!gesture) return gesture.GetStatus();
    command_->AddInputGesture(
        Base::Ref<InputGesture>(std::move(gesture).Value()));
    finalized_ = true;
    return {};
}

void MouseBinding::SetCommandName(Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(TrimAscii(value))) return;
    commandName_ = std::move(candidate);
    command_.Reset();
    finalized_ = false;
}

bool MouseBinding::Matches(const PointerInput& input) const noexcept {
    return input.changedButton == button_ && input.action == action_;
}

Base::Result<void> MouseBinding::Finalize() noexcept {
    if (finalized_) return {};
    if (command_) {
        finalized_ = true;
        return {};
    }
    if (commandName_.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
            "MouseBinding requires Command");
    }
    Base::Result<Base::Ref<RoutedCommand>> command =
        RoutedCommand::ResolveAuthored(commandName_.View());
    if (!command) return command.GetStatus();
    command_ = std::move(command).Value();
    finalized_ = true;
    return {};
}

void RoutedUICommand::SetText(Base::StringView value) noexcept {
    static_cast<void>(text_.Assign(value));
}

Base::Result<bool> RoutedCommand::CanExecute(
    const Meta::Value& parameter,
    UIElement* target) noexcept {
    Aero::InputRouter* input = target != nullptr
        ? AeroGuiInternal::InputRouterOf(*target)
        : nullptr;
    if (input == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "RoutedCommand requires a loaded command target");
    }
    return input->CanExecute(*this, parameter, *target);
}

void RoutedCommand::Execute(
    const Meta::Value& parameter,
    UIElement* target) noexcept {
    Aero::InputRouter* input = target != nullptr
        ? AeroGuiInternal::InputRouterOf(*target)
        : nullptr;
    if (input == nullptr) {
        return;
    }
    static_cast<void>(input->Execute(*this, parameter, *target));
}

} // namespace Aero::Input

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Input;

CommandState::CommandState(ElementTree& tree, EventRouter& events) noexcept
    : tree_(&tree),
      events_(&events),
      bindings_(&Base::GetDefaultAllocator()) {}

Base::Result<void> CommandState::VerifyTarget(
    UIElement& target) const noexcept {
    ::Aero::Media::Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Command routing requires an ElementTree root");
    }
    Base::Result<void> access = root->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!target.GetIsLoaded() || target.GetTree() != tree_) {
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

Base::Result<CommandBindingHandle> CommandState::AddBinding(
    UIElement& owner,
    const CommandBinding& binding) noexcept {
    Base::Result<void> verified = VerifyTarget(owner);
    if (!verified) return verified.GetStatus();
    Base::Result<void> finalized = const_cast<CommandBinding&>(binding).Finalize();
    if (!finalized) return finalized.GetStatus();
    if (!binding.IsValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Command binding requires a command");
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
    if (RoutedCommand* command = binding.GetCommand()) {
        record.command = Base::Ref<RoutedCommand>::TryFromBorrowed(*command);
    }
    record.canExecute = binding.GetCanExecute();
    record.executed = binding.GetExecuted();
    Base::Result<void> appended =
        bindings_.PushBack(std::move(record));
    if (!appended) return appended.GetStatus();
    return bindings_[bindings_.Size() - 1U].handle;
}

Base::Result<bool> CommandState::RemoveBinding(
    CommandBindingHandle handle) noexcept {
    ::Aero::Media::Visual* root = tree_->Root();
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

Base::Result<InputBindingHandle> CommandState::AddInputBinding(
    UIElement& owner,
    Base::Ref<InputBinding> binding) noexcept {
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
    Base::Result<void> appended = inputBindings_.PushBack(std::move(record));
    if (!appended) return appended.GetStatus();
    return inputBindings_.Back().handle;
}

Base::Result<bool> CommandState::CanExecute(
    ICommand& command,
    const Meta::Value& parameter,
    UIElement& target) noexcept {
    if (command.RuntimeType() == RoutedCommand::StaticTypeId()) {
        return CanExecute(
            static_cast<RoutedCommand&>(command), parameter, target);
    }
    return command.CanExecute(parameter, &target);
}

Base::Result<bool> CommandState::Execute(
    ICommand& command,
    const Meta::Value& parameter,
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
    command.Execute(parameter, &target);
    return true;
}

Base::Result<bool> CommandState::CanExecute(
    RoutedCommand& command,
    const Meta::Value& parameter,
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
            const VisualHandle ownerHandle = AeroGuiInternal::Handle(element);
            for (const BindingRecord& record : bindings_) {
                if (record.owner.index != ownerHandle.index ||
                    record.owner.generation != ownerHandle.generation ||
                    (record.command.Get() != &command &&
                     (record.command.Get() == nullptr ||
                      record.command->GetName() != command.GetName()))) {
                    continue;
                }
                const CanExecuteRoutedEventHandler& handler = record.canExecute;
                if (!handler.Empty()) {
                    handler.Invoke(&element, args);
                } else if (!record.executed.Empty()) {
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
    const Meta::Value& parameter,
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
            const VisualHandle ownerHandle = AeroGuiInternal::Handle(element);
            for (const BindingRecord& record : bindings_) {
                if (record.owner.index != ownerHandle.index ||
                    record.owner.generation != ownerHandle.generation ||
                    (record.command.Get() != &command &&
                     (record.command.Get() == nullptr ||
                      record.command->GetName() != command.GetName()))) {
                    continue;
                }
                const ExecutedRoutedEventHandler& handler = record.executed;
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
            const VisualHandle owner = AeroGuiInternal::Handle(element);
            for (const InputBindingRecord& record : inputBindings_) {
                if (record.owner.index != owner.index ||
                    record.owner.generation != owner.generation ||
                    !record.binding || !record.binding->GetCommand()) {
                    continue;
                }
                RoutedCommand& command = *record.binding->GetCommand();
                if (command.MatchesInput(input)) {
                    result = Execute(command, Meta::Value::Unset(), target);
                    return false;
                }
            }
            for (const BindingRecord& record : bindings_) {
                if (record.owner.index != owner.index ||
                    record.owner.generation != owner.generation) {
                    continue;
                }
                RoutedCommand* command = record.command.Get();
                if (command == nullptr || !command->MatchesInput(input)) continue;
                Base::Result<bool> allowed = CanExecute(*command, Meta::Value::Unset(), target);
                if (!allowed) {
                    result = allowed.GetStatus();
                    return false;
                }
                if (allowed.Value()) {
                    result = Execute(*command, Meta::Value::Unset(), target);
                    return false;
                }
            }
            return true;
        });
    if (!routed) return routed.GetStatus();
    return result;
}

Base::Result<bool> CommandState::ProcessInput(
    UIElement& target,
    const PointerInput& input) noexcept {
    if (!target.GetIsEnabled()) return false;
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
            const VisualHandle owner = AeroGuiInternal::Handle(element);
            for (const InputBindingRecord& record : inputBindings_) {
                if (record.owner.index != owner.index ||
                    record.owner.generation != owner.generation ||
                    !record.binding || !record.binding->GetCommand()) {
                    continue;
                }
                bool matches = false;
                if (MouseBinding* mouse =
                        ::Aero::TryCast<MouseBinding>(record.binding.Get())) {
                    matches = mouse->Matches(input);
                } else {
                    matches = record.binding->GetCommand()->MatchesPointer(input);
                }
                if (matches) {
                    result = Execute(
                        *record.binding->GetCommand(),
                        Meta::Value::Unset(),
                        target);
                    return false;
                }
            }
            return true;
        });
    if (!routed) return routed.GetStatus();
    return result;
}

void CommandState::AddRequerySuggested(
    const RequerySuggestedHandler& handler) noexcept {
    requerySuggested_.Add(handler);
}

bool CommandState::RemoveRequerySuggested(
    const RequerySuggestedHandler& handler) noexcept {
    return requerySuggested_.Remove(handler);
}

void CommandState::InvalidateRequerySuggested() const noexcept {
    if (!requerySuggested_.Empty()) requerySuggested_.Invoke();
}

} // namespace Aero
