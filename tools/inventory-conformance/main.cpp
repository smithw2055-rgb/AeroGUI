#include <Aero/Collections.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Gui.hpp>
#include <Aero/Input.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Meta.hpp>
#include <Aero/Module.hpp>
#include <Aero/Triggers/Behavior.hpp>
#include <Aero/View.hpp>
#include <AeroApp/Window.hpp>

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include <AeroApp/App.hpp>

namespace Inventory {

inline constexpr Aero::Base::StringView XamlNamespace(
    "clr-namespace:Inventory");

enum class ItemCategory : std::uint8_t {
    All = 0U,
    Hand,
    Ring,
    Head,
    Chest,
    Arms,
    Legs,
    Feet
};

class TestCommand final : public Aero::Input::ICommand {
    AERO_DECLARE_TYPE_NAMED(
        TestCommand,
        Aero::Input::ICommand,
        "clr-namespace:Inventory",
        "TestCommand")
public:
    Aero::Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Aero::Base::Result<bool> CanExecute(
        const Aero::Value&,
        Aero::UIElement* = nullptr) noexcept override {
        return true;
    }
    void Execute(
        const Aero::Value& parameter,
        Aero::UIElement* = nullptr) noexcept override {
        if (executionCount_ < 16U) {
            parameters_[executionCount_] = parameter;
        }
        ++executionCount_;
        lastParameter_ = parameter;
    }
    std::uint32_t GetExecutionCount() const noexcept {
        return executionCount_;
    }
    const Aero::Value& GetLastParameter() const noexcept {
        return lastParameter_;
    }
    const Aero::Value& GetParameter(
        std::uint32_t index) const noexcept {
        return parameters_[index < 16U ? index : 15U];
    }
private:
    std::uint32_t executionCount_ = 0U;
    Aero::Value lastParameter_;
    Aero::Value parameters_[16];
};

class Item final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        Item,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "Item")
public:
    Item() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Aero::Base::String> NameProperty{"Name"};
    inline static constexpr DependencyProperty<Aero::Base::String> DescriptionProperty{"Description"};
    inline static constexpr DependencyProperty<ItemCategory> CategoryProperty{"Category"};
    inline static constexpr DependencyProperty<std::int32_t> LifeProperty{"Life"};
    inline static constexpr DependencyProperty<std::int32_t> ManaProperty{"Mana"};
    inline static constexpr DependencyProperty<std::int32_t> DpsProperty{"Dps"};
    inline static constexpr DependencyProperty<std::int32_t> ArmorProperty{"Armor"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Media::ImageSource>> IconProperty{"Icon"};
};

class Slot final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        Slot,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "Slot")
public:
    Slot() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Aero::Base::String> NameProperty{"Name"};
    inline static constexpr DependencyProperty<ItemCategory> AllowedCategoryProperty{"AllowedCategory"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Item>> ItemProperty{"Item"};
    inline static constexpr DependencyProperty<bool> IsDragOverProperty{"IsDragOver"};
    inline static constexpr DependencyProperty<bool> IsDropAllowedProperty{"IsDropAllowed"};
    inline static constexpr DependencyProperty<bool> IsSelectedProperty{"IsSelected"};
    inline static constexpr DependencyProperty<bool> MoveFocusProperty{"MoveFocus"};
};

class Player final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        Player,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "Player")
public:
    Player() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Aero::Base::String> NameProperty{"Name"};
    inline static constexpr DependencyProperty<std::int32_t> LifeProperty{"Life"};
    inline static constexpr DependencyProperty<std::int32_t> ManaProperty{"Mana"};
    inline static constexpr DependencyProperty<std::int32_t> DpsProperty{"Dps"};
    inline static constexpr DependencyProperty<std::int32_t> ArmorProperty{"Armor"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Base::Object>> SlotsProperty{"Slots"};
};

class ViewModel final : public Aero::DependencyObject {
    AERO_DECLARE_TYPE_NAMED(
        ViewModel,
        Aero::DependencyObject,
        "clr-namespace:Inventory",
        "ViewModel")
public:
    ViewModel() noexcept : DependencyObject(StaticTypeId()) {}
    inline static constexpr DependencyProperty<Aero::Base::String> PlatformProperty{"Platform"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Player>> PlayerProperty{"Player"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Base::Object>> InventoryProperty{"Inventory"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Base::Object>> ItemsProperty{"Items"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Input::ICommand>> StartDragItemProperty{"StartDragItem"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Input::ICommand>> EndDragItemProperty{"EndDragItem"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Input::ICommand>> DropItemProperty{"DropItem"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Input::ICommand>> SelectSlotProperty{"SelectSlot"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Slot>> DragSourceProperty{"DragSource"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Item>> DraggedItemProperty{"DraggedItem"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Slot>> SelectedSlotProperty{"SelectedSlot"};
};

class AnimatedNumber final : public Aero::Controls::UserControl {
    AERO_DECLARE_TYPE_NAMED(
        AnimatedNumber,
        Aero::Controls::UserControl,
        "clr-namespace:Inventory",
        "AnimatedNumber")
public:
    AnimatedNumber() noexcept : UserControl(StaticTypeId()) {}
    inline static constexpr DependencyProperty<std::int32_t> NumberProperty{"Number"};
    inline static constexpr DependencyProperty<std::int32_t> AnimatedNumberProperty{"AnimatedNumber"};
    inline static constexpr DependencyProperty<Aero::Base::String> AnimationDurationProperty{"AnimationDuration"};
};

class DragAdornerBehavior final : public Aero::Interactivity::Behavior {
    AERO_DECLARE_TYPE_NAMED(
        DragAdornerBehavior,
        Aero::Interactivity::Behavior,
        "clr-namespace:Inventory",
        "DragAdornerBehavior")
public:
    DragAdornerBehavior() noexcept
        : Behavior(StaticTypeId()),
          dragOverHandler_(this, &DragAdornerBehavior::OnDragOver),
          dropHandler_(this, &DragAdornerBehavior::OnDrop) {}
    inline static constexpr DependencyProperty<Aero::Base::Point> DragStartOffsetProperty{"DragStartOffset"};
    inline static constexpr DependencyProperty<double> DraggedItemXProperty{"DraggedItemX"};
    inline static constexpr DependencyProperty<double> DraggedItemYProperty{"DraggedItemY"};
    inline static std::uint32_t attachedCount = 0U;
    inline static std::uint32_t detachedCount = 0U;
    static void ResetCounters() noexcept {
        attachedCount = 0U;
        detachedCount = 0U;
    }
protected:
    Aero::Base::Result<void> OnAttached() noexcept override {
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated == nullptr) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InvalidState,
                "DragAdornerBehavior has no associated object");
        }
        associated->SetAllowDrop(true);
        associated->DragOver().Add(dragOverHandler_);
        associated->Drop().Add(dropHandler_);
        ++attachedCount;
        return {};
    }
    void OnDetaching() noexcept override {
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated != nullptr) {
            static_cast<void>(associated->DragOver().Remove(dragOverHandler_));
            static_cast<void>(associated->Drop().Remove(dropHandler_));
            associated->SetAllowDrop(false);
        }
        ++detachedCount;
    }
private:
    Aero::DragEventHandler dragOverHandler_;
    Aero::DragEventHandler dropHandler_;

    void OnDragOver(Aero::Base::Object*, Aero::DragEventArgs& args) noexcept {
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated == nullptr) return;
        const Aero::Base::Point position = args.GetPosition(*associated);
        const Aero::Base::Point offset = GetValueOr(
            DragStartOffsetProperty, Aero::Base::Point{});
        SetValue(DraggedItemXProperty, position.x - offset.x);
        SetValue(DraggedItemYProperty, position.y - offset.y);
    }
    void OnDrop(Aero::Base::Object*, Aero::DragEventArgs& args) noexcept {
        args.SetEffects(Aero::Input::DragDropEffects::None);
    }
};

class DragItemBehavior final : public Aero::Interactivity::Behavior {
    AERO_DECLARE_TYPE_NAMED(
        DragItemBehavior,
        Aero::Interactivity::Behavior,
        "clr-namespace:Inventory",
        "DragItemBehavior")
public:
    DragItemBehavior() noexcept
        : Behavior(StaticTypeId()),
          feedbackHandler_(this, &DragItemBehavior::OnGiveFeedback),
          mouseDownHandler_(this, &DragItemBehavior::OnMouseDown),
          mouseUpHandler_(this, &DragItemBehavior::OnMouseUp),
          mouseMoveHandler_(this, &DragItemBehavior::OnMouseMove),
          completedHandler_(this, &DragItemBehavior::OnDragCompleted) {}
    inline static constexpr DependencyProperty<Aero::Base::Point> DragStartOffsetProperty{"DragStartOffset"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Input::ICommand>> StartDragCommandProperty{"StartDragCommand"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Input::ICommand>> EndDragCommandProperty{"EndDragCommand"};
    inline static std::uint32_t attachedCount = 0U;
    inline static std::uint32_t detachedCount = 0U;
    static void ResetCounters() noexcept {
        attachedCount = 0U;
        detachedCount = 0U;
    }
protected:
    Aero::Base::Result<void> OnAttached() noexcept override {
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated == nullptr) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InvalidState,
                "DragItemBehavior has no associated object");
        }
        associated->GiveFeedback().Add(feedbackHandler_);
        associated->PreviewMouseLeftButtonDown().Add(mouseDownHandler_);
        associated->PreviewMouseLeftButtonUp().Add(mouseUpHandler_);
        associated->PreviewMouseMove().Add(mouseMoveHandler_);
        associated->DragCompleted().Add(completedHandler_);
        ++attachedCount;
        return {};
    }
    void OnDetaching() noexcept override {
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated != nullptr) {
            static_cast<void>(associated->GiveFeedback().Remove(feedbackHandler_));
            static_cast<void>(associated->PreviewMouseLeftButtonDown().Remove(mouseDownHandler_));
            static_cast<void>(associated->PreviewMouseLeftButtonUp().Remove(mouseUpHandler_));
            static_cast<void>(associated->PreviewMouseMove().Remove(mouseMoveHandler_));
            static_cast<void>(associated->DragCompleted().Remove(completedHandler_));
        }
        mouseClicked_ = false;
        dragStarted_ = false;
        ++detachedCount;
    }
private:
    Aero::GiveFeedbackEventHandler feedbackHandler_;
    Aero::MouseButtonEventHandler mouseDownHandler_;
    Aero::MouseButtonEventHandler mouseUpHandler_;
    Aero::MouseEventHandler mouseMoveHandler_;
    Aero::DragCompletedEventHandler completedHandler_;
    bool mouseClicked_ = false;
    bool dragStarted_ = false;

    void OnGiveFeedback(
        Aero::Base::Object*,
        Aero::GiveFeedbackEventArgs& args) noexcept {
        args.SetUseDefaultCursors(false);
        args.SetHandled(true);
    }
    void OnMouseDown(
        Aero::Base::Object*,
        Aero::MouseButtonEventArgs&) noexcept {
        mouseClicked_ = true;
    }
    void OnMouseUp(
        Aero::Base::Object*,
        Aero::MouseButtonEventArgs&) noexcept {
        mouseClicked_ = false;
    }
    void OnMouseMove(
        Aero::Base::Object*,
        Aero::MouseEventArgs& args) noexcept {
        if (!mouseClicked_) return;
        mouseClicked_ = false;
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated == nullptr) return;
        Aero::Value item = associated->GetDataContext();
        if (item.IsUnset() || item.IsNullObject()) return;
        Aero::Base::Ref<Aero::Input::ICommand> command = GetValueOr(
            StartDragCommandProperty,
            Aero::Base::Ref<Aero::Input::ICommand>{});
        if (!command) return;
        Aero::Base::Result<bool> canExecute =
            command->CanExecute(item, associated);
        if (!canExecute || !canExecute.Value()) return;
        SetValue(DragStartOffsetProperty, args.GetPosition());
        command->Execute(item, associated);
        Aero::Base::Result<void> started = associated->BeginDrag(
            args.GetPointerId(), item,
            Aero::Input::DragDropEffects::Move);
        dragStarted_ = started.HasValue();
    }
    void OnDragCompleted(
        Aero::Base::Object*,
        Aero::DragCompletedEventArgs& args) noexcept {
        if (!dragStarted_) return;
        dragStarted_ = false;
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated == nullptr) return;
        Aero::Base::Ref<Aero::Input::ICommand> command = GetValueOr(
            EndDragCommandProperty,
            Aero::Base::Ref<Aero::Input::ICommand>{});
        if (!command) return;
        const bool success = !args.GetCanceled() &&
            args.GetEffects() != Aero::Input::DragDropEffects::None;
        Aero::Value parameter = Aero::Value::FromBoolean(
            Aero::Meta::TypeOf<bool>(), success);
        Aero::Base::Result<bool> canExecute =
            command->CanExecute(parameter, associated);
        if (canExecute && canExecute.Value()) {
            command->Execute(parameter, associated);
        }
    }
};

class DropItemBehavior final : public Aero::Interactivity::Behavior {
    AERO_DECLARE_TYPE_NAMED(
        DropItemBehavior,
        Aero::Interactivity::Behavior,
        "clr-namespace:Inventory",
        "DropItemBehavior")
public:
    DropItemBehavior() noexcept
        : Behavior(StaticTypeId()),
          enterHandler_(this, &DropItemBehavior::OnDragEnter),
          leaveHandler_(this, &DropItemBehavior::OnDragLeave),
          dropHandler_(this, &DropItemBehavior::OnDrop) {}
    inline static constexpr DependencyProperty<bool> IsDragOverProperty{"IsDragOver"};
    inline static constexpr DependencyProperty<Aero::Base::Ref<Aero::Input::ICommand>> DropCommandProperty{"DropCommand"};
    inline static std::uint32_t attachedCount = 0U;
    inline static std::uint32_t detachedCount = 0U;
    inline static std::uint32_t enterCount = 0U;
    inline static std::uint32_t leaveCount = 0U;
    inline static std::uint32_t dropCount = 0U;
    inline static Aero::FrameworkElement* lastEntered = nullptr;
    inline static DropItemBehavior* lastEnteredBehavior = nullptr;
    static void ResetCounters() noexcept {
        attachedCount = 0U;
        detachedCount = 0U;
        enterCount = 0U;
        leaveCount = 0U;
        dropCount = 0U;
        lastEntered = nullptr;
        lastEnteredBehavior = nullptr;
    }
protected:
    Aero::Base::Result<void> OnAttached() noexcept override {
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated == nullptr) {
            return Aero::Base::Status::Failure(
                Aero::Base::ErrorCode::InvalidState,
                "DropItemBehavior has no associated object");
        }
        associated->SetAllowDrop(true);
        associated->PreviewDragEnter().Add(enterHandler_);
        associated->PreviewDragLeave().Add(leaveHandler_);
        associated->PreviewDrop().Add(dropHandler_);
        ++attachedCount;
        return {};
    }
    void OnDetaching() noexcept override {
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated != nullptr) {
            static_cast<void>(associated->PreviewDragEnter().Remove(enterHandler_));
            static_cast<void>(associated->PreviewDragLeave().Remove(leaveHandler_));
            static_cast<void>(associated->PreviewDrop().Remove(dropHandler_));
            associated->SetAllowDrop(false);
        }
        ++detachedCount;
    }
private:
    Aero::DragEventHandler enterHandler_;
    Aero::DragEventHandler leaveHandler_;
    Aero::DragEventHandler dropHandler_;

    void OnDragEnter(
        Aero::Base::Object*, Aero::DragEventArgs& args) noexcept {
        ++enterCount;
        lastEntered = GetAssociatedObject();
        lastEnteredBehavior = this;
        SetValue(IsDragOverProperty, true);
        args.SetHandled(true);
    }
    void OnDragLeave(
        Aero::Base::Object*, Aero::DragEventArgs& args) noexcept {
        ++leaveCount;
        SetValue(IsDragOverProperty, false);
        args.SetHandled(true);
    }
    void OnDrop(
        Aero::Base::Object*, Aero::DragEventArgs& args) noexcept {
        ++dropCount;
        SetValue(IsDragOverProperty, false);
        Aero::FrameworkElement* associated = GetAssociatedObject();
        if (associated == nullptr) {
            args.SetEffects(Aero::Input::DragDropEffects::None);
            args.SetHandled(true);
            return;
        }
        Aero::Value item = associated->GetDataContext();
        Aero::Base::Ref<Aero::Input::ICommand> command = GetValueOr(
            DropCommandProperty,
            Aero::Base::Ref<Aero::Input::ICommand>{});
        Aero::Base::Result<bool> canExecute = command
            ? command->CanExecute(item, associated)
            : Aero::Base::Result<bool>(false);
        if (command && canExecute && canExecute.Value() &&
            !item.IsUnset() && !item.IsNullObject()) {
            command->Execute(item, associated);
        } else {
            args.SetEffects(Aero::Input::DragDropEffects::None);
        }
        args.SetHandled(true);
    }
};

} // namespace Inventory

AERO_DECLARE_TYPE_ENUM(Inventory::ItemCategory)

namespace {

Aero::Base::Result<void> RegisterInventoryModule(
    Aero::Meta::Registration& context) noexcept {
    using namespace Aero;
    using namespace Aero::Meta;

    auto category = Register<Inventory::ItemCategory>(
        context, Inventory::XamlNamespace, "ItemCategory");
    category
        .Value("All", Inventory::ItemCategory::All)
        .Value("Hand", Inventory::ItemCategory::Hand)
        .Value("Ring", Inventory::ItemCategory::Ring)
        .Value("Head", Inventory::ItemCategory::Head)
        .Value("Chest", Inventory::ItemCategory::Chest)
        .Value("Arms", Inventory::ItemCategory::Arms)
        .Value("Legs", Inventory::ItemCategory::Legs)
        .Value("Feet", Inventory::ItemCategory::Feet);
    Base::Result<void> status = category.Result();
    if (!status) return status.GetStatus();

    status = Register<Inventory::TestCommand>(context).Result();
    if (!status) return status.GetStatus();

    auto item = Register<Inventory::Item>(context);
    item
        .Property(Inventory::Item::NameProperty, FrameworkPropertyMetadata(Base::String{}))
        .Property(Inventory::Item::DescriptionProperty, FrameworkPropertyMetadata(Base::String{}))
        .Property(Inventory::Item::CategoryProperty, FrameworkPropertyMetadata(Inventory::ItemCategory::All))
        .Property(Inventory::Item::LifeProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Item::ManaProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Item::DpsProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Item::ArmorProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Item::IconProperty, FrameworkPropertyMetadata(Base::Ref<Media::ImageSource>{}))
        .Factory();
    status = item.Result();
    if (!status) return status.GetStatus();

    auto slot = Register<Inventory::Slot>(context);
    slot
        .Property(Inventory::Slot::NameProperty, FrameworkPropertyMetadata(Base::String{}))
        .Property(Inventory::Slot::AllowedCategoryProperty, FrameworkPropertyMetadata(Inventory::ItemCategory::All))
        .Property(Inventory::Slot::ItemProperty, FrameworkPropertyMetadata(Base::Ref<Inventory::Item>{}))
        .Property(Inventory::Slot::IsDragOverProperty, FrameworkPropertyMetadata(false).BindsTwoWayByDefault())
        .Property(Inventory::Slot::IsDropAllowedProperty, FrameworkPropertyMetadata(false))
        .Property(Inventory::Slot::IsSelectedProperty, FrameworkPropertyMetadata(false))
        .Property(Inventory::Slot::MoveFocusProperty, FrameworkPropertyMetadata(false))
        .Factory();
    status = slot.Result();
    if (!status) return status.GetStatus();

    auto player = Register<Inventory::Player>(context);
    player
        .Property(Inventory::Player::NameProperty, FrameworkPropertyMetadata(Base::String{}))
        .Property(Inventory::Player::LifeProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Player::ManaProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Player::DpsProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Player::ArmorProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::Player::SlotsProperty, FrameworkPropertyMetadata(Base::Ref<Base::Object>{}))
        .Factory();
    status = player.Result();
    if (!status) return status.GetStatus();

    auto viewModel = Register<Inventory::ViewModel>(context);
    viewModel
        .Property(Inventory::ViewModel::PlatformProperty, FrameworkPropertyMetadata(Base::String{}))
        .Property(Inventory::ViewModel::PlayerProperty, FrameworkPropertyMetadata(Base::Ref<Inventory::Player>{}))
        .Property(Inventory::ViewModel::InventoryProperty, FrameworkPropertyMetadata(Base::Ref<Base::Object>{}))
        .Property(Inventory::ViewModel::ItemsProperty, FrameworkPropertyMetadata(Base::Ref<Base::Object>{}))
        .Property(Inventory::ViewModel::StartDragItemProperty, FrameworkPropertyMetadata(Base::Ref<Input::ICommand>{}))
        .Property(Inventory::ViewModel::EndDragItemProperty, FrameworkPropertyMetadata(Base::Ref<Input::ICommand>{}))
        .Property(Inventory::ViewModel::DropItemProperty, FrameworkPropertyMetadata(Base::Ref<Input::ICommand>{}))
        .Property(Inventory::ViewModel::SelectSlotProperty, FrameworkPropertyMetadata(Base::Ref<Input::ICommand>{}))
        .Property(Inventory::ViewModel::DragSourceProperty, FrameworkPropertyMetadata(Base::Ref<Inventory::Slot>{}))
        .Property(Inventory::ViewModel::DraggedItemProperty, FrameworkPropertyMetadata(Base::Ref<Inventory::Item>{}))
        .Property(Inventory::ViewModel::SelectedSlotProperty, FrameworkPropertyMetadata(Base::Ref<Inventory::Slot>{}))
        .Factory();
    status = viewModel.Result();
    if (!status) return status.GetStatus();

    auto animated = Register<Inventory::AnimatedNumber>(context);
    animated
        .Property(Inventory::AnimatedNumber::NumberProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::AnimatedNumber::AnimatedNumberProperty, FrameworkPropertyMetadata(std::int32_t{0}))
        .Property(Inventory::AnimatedNumber::AnimationDurationProperty, FrameworkPropertyMetadata(Base::String{}))
        .Factory();
    status = animated.Result();
    if (!status) return status.GetStatus();

    auto adorner = Register<Inventory::DragAdornerBehavior>(context);
    adorner
        .Property(Inventory::DragAdornerBehavior::DragStartOffsetProperty, FrameworkPropertyMetadata(Base::Point{}))
        .Property(Inventory::DragAdornerBehavior::DraggedItemXProperty, FrameworkPropertyMetadata(0.0))
        .Property(Inventory::DragAdornerBehavior::DraggedItemYProperty, FrameworkPropertyMetadata(0.0))
        .Factory();
    status = adorner.Result();
    if (!status) return status.GetStatus();

    auto drag = Register<Inventory::DragItemBehavior>(context);
    drag
        .Property(Inventory::DragItemBehavior::DragStartOffsetProperty, FrameworkPropertyMetadata(Base::Point{}))
        .Property(Inventory::DragItemBehavior::StartDragCommandProperty, FrameworkPropertyMetadata(Base::Ref<Input::ICommand>{}))
        .Property(Inventory::DragItemBehavior::EndDragCommandProperty, FrameworkPropertyMetadata(Base::Ref<Input::ICommand>{}))
        .Factory();
    status = drag.Result();
    if (!status) return status.GetStatus();

    auto drop = Register<Inventory::DropItemBehavior>(context);
    drop
        .Property(Inventory::DropItemBehavior::IsDragOverProperty, FrameworkPropertyMetadata(false).BindsTwoWayByDefault())
        .Property(Inventory::DropItemBehavior::DropCommandProperty, FrameworkPropertyMetadata(Base::Ref<Input::ICommand>{}))
        .Factory();
    return drop.Result();
}

constexpr Aero::ModuleRegistration InventoryModule =
    Aero::DefineModule("Inventory", &RegisterInventoryModule);

class FileStream final : public Aero::Base::Stream {
public:
    FileStream(std::FILE* file, std::uint64_t length) noexcept
        : file_(file), length_(length) {}
    ~FileStream() override { if (file_ != nullptr) std::fclose(file_); }
    bool CanRead() const noexcept override { return file_ != nullptr; }
    bool CanSeek() const noexcept override { return file_ != nullptr; }
    Aero::Base::Result<std::uint32_t> Read(Aero::Base::Span<std::uint8_t> destination) noexcept override {
        if (file_ == nullptr) return Aero::Base::Status::Failure(Aero::Base::ErrorCode::InvalidState, "Inventory file stream is closed");
        const std::size_t read = std::fread(destination.Data(), 1U, destination.Size(), file_);
        if (read == 0U && std::ferror(file_) != 0) return Aero::Base::Status::Failure(Aero::Base::ErrorCode::InternalError, "Inventory file read failed");
        return static_cast<std::uint32_t>(read);
    }
    Aero::Base::Result<std::uint64_t> Position() const noexcept override {
        const long value = file_ != nullptr ? std::ftell(file_) : -1L;
        return value >= 0 ? Aero::Base::Result<std::uint64_t>(static_cast<std::uint64_t>(value))
                          : Aero::Base::Result<std::uint64_t>(Aero::Base::Status::Failure(Aero::Base::ErrorCode::InternalError, "Inventory file position failed"));
    }
    Aero::Base::Result<std::uint64_t> Length() const noexcept override { return length_; }
    Aero::Base::Result<std::uint64_t> Seek(std::int64_t offset, Aero::Base::SeekOrigin origin) noexcept override {
        int nativeOrigin = origin == Aero::Base::SeekOrigin::Begin ? SEEK_SET : origin == Aero::Base::SeekOrigin::Current ? SEEK_CUR : SEEK_END;
        if (file_ == nullptr || offset < LONG_MIN || offset > LONG_MAX || std::fseek(file_, static_cast<long>(offset), nativeOrigin) != 0) {
            return Aero::Base::Status::Failure(Aero::Base::ErrorCode::InternalError, "Inventory file seek failed");
        }
        return Position();
    }
private:
    std::FILE* file_ = nullptr;
    std::uint64_t length_ = 0U;
};

std::string ToString(Aero::Base::StringView value) {
    return value.Data() == nullptr ? std::string{} : std::string(value.Data(), value.SizeBytes());
}

std::string ComponentPath(Aero::Base::StringView path) {
    std::string value = ToString(path);
    constexpr const char* marker = ";component/";
    const std::size_t found = value.find(marker);
    if (found != std::string::npos) value.erase(0U, found + std::strlen(marker));
    while (!value.empty() && value.front() == '/') value.erase(value.begin());
    return value;
}

class InventoryXamlProvider final : public ::Aero::Markup::XamlProvider {
public:
    explicit InventoryXamlProvider(std::string root) noexcept : root_(std::move(root)) {}
    Aero::Base::Result<::Aero::Markup::StreamResourceInfo> Open(const Aero::Base::ResourceUri& uri) const noexcept override {
        if (!uri.Assembly().Empty() && uri.Assembly() != Aero::Base::StringView("Inventory")) {
            return Aero::Base::Status::Failure(Aero::Base::ErrorCode::NotFound, "Inventory assembly route was not found");
        }
        std::string path = root_;
        if (!path.empty() && path.back() != '/') path.push_back('/');
        path += ComponentPath(uri.Path());
        std::FILE* file = std::fopen(path.c_str(), "rb");
        if (file == nullptr) return Aero::Base::Status::Failure(Aero::Base::ErrorCode::NotFound, "Inventory resource was not found");
        if (std::fseek(file, 0L, SEEK_END) != 0) { std::fclose(file); return Aero::Base::Status::Failure(Aero::Base::ErrorCode::InternalError, "Inventory file length failed"); }
        const long length = std::ftell(file);
        if (length < 0 || std::fseek(file, 0L, SEEK_SET) != 0) { std::fclose(file); return Aero::Base::Status::Failure(Aero::Base::ErrorCode::InternalError, "Inventory file rewind failed"); }
        Aero::Base::Result<Aero::Base::Ref<FileStream>> stream = Aero::Base::MakeRef<FileStream>(file, static_cast<std::uint64_t>(length));
        if (!stream) { std::fclose(file); return stream.GetStatus(); }
        ::Aero::Markup::StreamResourceInfo info;
        info.uri = uri;
        info.stream = Aero::Base::Ref<Aero::Base::Stream>(std::move(stream).Value());
        info.revision = 1U;
        return info;
    }
private:
    std::string root_;
};

void PrintDiagnostics(const Aero::Diagnostics::DiagnosticBag& diagnostics) {
    for (const Aero::Diagnostics::Diagnostic& item : diagnostics.Items()) {
        std::fprintf(stderr, "  0x%08x %u:%u %.*s\n", item.Code().value,
            item.Source().begin.line, item.Source().begin.column,
            static_cast<int>(item.Message().SizeBytes()), item.Message().Data());
    }
}

struct ViewModelFixture {
    Aero::Base::Ref<Inventory::ViewModel> viewModel;
    Aero::Base::Ref<Inventory::TestCommand> command;
    Aero::Base::Ref<Aero::Collections::ObservableCollection> slots;
};

Aero::Base::Result<ViewModelFixture> CreateViewModel() noexcept {
    using namespace Aero;
    Base::Result<Base::Ref<Inventory::ViewModel>> vm = Base::MakeRef<Inventory::ViewModel>();
    if (!vm) return vm.GetStatus();
    Base::Result<Base::Ref<Inventory::Player>> player = Base::MakeRef<Inventory::Player>();
    Base::Result<Base::Ref<Collections::ObservableCollection>> inventory = Base::MakeRef<Collections::ObservableCollection>();
    Base::Result<Base::Ref<Collections::ObservableCollection>> items = Base::MakeRef<Collections::ObservableCollection>();
    Base::Result<Base::Ref<Collections::ObservableCollection>> slots = Base::MakeRef<Collections::ObservableCollection>();
    Base::Result<Base::Ref<Inventory::TestCommand>> command = Base::MakeRef<Inventory::TestCommand>();
    if (!player || !inventory || !items || !slots || !command) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Inventory view model allocation failed");

    player.Value()->SetValue(Inventory::Player::NameProperty, Base::StringView("Morgan Hearson"));
    player.Value()->SetValue(Inventory::Player::LifeProperty, std::int32_t{1423});
    player.Value()->SetValue(Inventory::Player::ManaProperty, std::int32_t{345});
    player.Value()->SetValue(Inventory::Player::DpsProperty, std::int32_t{2164});
    player.Value()->SetValue(Inventory::Player::ArmorProperty, std::int32_t{218});

    for (std::uint32_t index = 0U; index < 12U; ++index) {
        Base::Result<Base::Ref<Inventory::Item>> item = Base::MakeRef<Inventory::Item>();
        Base::Result<Base::Ref<Inventory::Slot>> slot = Base::MakeRef<Inventory::Slot>();
        if (!item || !slot) return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Inventory item allocation failed");
        item.Value()->SetValue(Inventory::Item::NameProperty, Base::StringView("Item"));
        item.Value()->SetValue(Inventory::Item::DescriptionProperty, Base::StringView("Item description"));
        item.Value()->SetValue(Inventory::Item::LifeProperty, static_cast<std::int32_t>(index * 2U));
        item.Value()->SetValue(Inventory::Item::ManaProperty, static_cast<std::int32_t>(index));
        item.Value()->SetValue(Inventory::Item::DpsProperty, static_cast<std::int32_t>(index * 9U));
        item.Value()->SetValue(Inventory::Item::ArmorProperty, static_cast<std::int32_t>(index * 3U));
        slot.Value()->SetValue(Inventory::Slot::NameProperty, Base::StringView("Slot"));
        slot.Value()->SetValue(Inventory::Slot::AllowedCategoryProperty, Inventory::ItemCategory::All);
        slot.Value()->SetValue(Inventory::Slot::ItemProperty, item.Value());
        Base::Result<void> added = items.Value()->Add(Base::Ref<Base::Object>(item.Value()));
        if (added) added = inventory.Value()->Add(Base::Ref<Base::Object>(slot.Value()));
        if (!added) return added.GetStatus();
        if (index < 8U) {
            added = slots.Value()->Add(Base::Ref<Base::Object>(slot.Value()));
            if (!added) return added.GetStatus();
        }
    }
    player.Value()->SetValue(Inventory::Player::SlotsProperty, Base::Ref<Base::Object>(slots.Value()));
    vm.Value()->SetValue(Inventory::ViewModel::PlatformProperty, Base::StringView("PC"));
    vm.Value()->SetValue(Inventory::ViewModel::PlayerProperty, player.Value());
    vm.Value()->SetValue(Inventory::ViewModel::InventoryProperty, Base::Ref<Base::Object>(inventory.Value()));
    vm.Value()->SetValue(Inventory::ViewModel::ItemsProperty, Base::Ref<Base::Object>(items.Value()));
    Base::Ref<Input::ICommand> interfaceCommand(command.Value());
    vm.Value()->SetValue(Inventory::ViewModel::StartDragItemProperty, interfaceCommand);
    vm.Value()->SetValue(Inventory::ViewModel::EndDragItemProperty, interfaceCommand);
    vm.Value()->SetValue(Inventory::ViewModel::DropItemProperty, interfaceCommand);
    vm.Value()->SetValue(Inventory::ViewModel::SelectSlotProperty, std::move(interfaceCommand));
    ViewModelFixture result;
    result.viewModel = std::move(vm).Value();
    result.command = std::move(command).Value();
    result.slots = std::move(slots).Value();
    return result;
}


Aero::Base::Point RootCenter(
    const Aero::UIElement& element,
    const Aero::Media::Visual& root) noexcept {
    Aero::Base::Point point{
        element.GetRenderSize().width * 0.5,
        element.GetRenderSize().height * 0.5};
    const Aero::Media::Visual* current = &element;
    while (current != nullptr) {
        const Aero::FrameworkElement* framework =
            current->AsFrameworkElement();
        if (framework != nullptr) {
            point = Aero::Media::TransformPoint(
                framework->GetLocalVisualTransform(), point);
        }
        const Aero::UIElement* currentElement =
            current->AsUIElement();
        if (currentElement != nullptr) {
            const Aero::Rect slot =
                currentElement->GetLayoutSlot();
            point.x += slot.x;
            point.y += slot.y;
        }
        if (current == &root) break;
        current = current->GetVisualParent();
    }
    return point;
}

bool IsObjectValue(
    const Aero::Value& value,
    const Aero::Base::Object* expected) noexcept {
    return value.Kind() == Aero::Meta::ValueKind::Object &&
        !value.IsNullObject() && value.AsObject() &&
        value.AsObject().Get() == expected;
}

bool SendHostKey(
    Aero::View& view,
    const Aero::Input::KeyboardInput& input) noexcept {
    const auto key = static_cast<Aero::Input::Key>(input.key);
    return input.action == Aero::Input::KeyboardAction::Down
        ? view.KeyDown(key)
        : view.KeyUp(key);
}

bool SendHostPointer(
    Aero::View& view,
    const Aero::Input::PointerInput& input) noexcept {
    const int x = static_cast<int>(input.position.x);
    const int y = static_cast<int>(input.position.y);
    switch (input.action) {
    case Aero::Input::PointerAction::Move:
        return view.MouseMove(x, y);
    case Aero::Input::PointerAction::Down:
        return view.MouseButtonDown(x, y, input.changedButton);
    case Aero::Input::PointerAction::Up:
        return view.MouseButtonUp(x, y, input.changedButton);
    case Aero::Input::PointerAction::Wheel:
        return view.MouseWheel(
            x, y, static_cast<int>(input.wheelDeltaY));
    }
    return false;
}

void AdvanceView(
    Aero::View& view,
    double& timeInSeconds,
    std::uint32_t elapsedMilliseconds) noexcept {
    timeInSeconds += static_cast<double>(elapsedMilliseconds) / 1000.0;
    view.Update(timeInSeconds);
}

bool VerifyMainWindowInteractions(
    Aero::View& view,
    double& timeInSeconds,
    Aero::FrameworkElement& root,
    ViewModelFixture& fixture) {
    using namespace Aero;
    auto* slotHead = root.FindName<Controls::ContentControl>("SlotHead");
    auto* slotChest = root.FindName<Controls::ContentControl>("SlotChest");
    auto* selectPc = root.FindName<FrameworkElement>("SelectPC");
    auto* selectXbox = root.FindName<FrameworkElement>("SelectXBOX");
    if (slotHead == nullptr || slotChest == nullptr ||
        selectPc == nullptr || selectXbox == nullptr) {
        std::fprintf(stderr, "INTERACTION FAIL: required named elements are missing\n");
        return false;
    }
    if (selectPc->GetVisibility() != Visibility::Visible ||
        selectXbox->GetVisibility() != Visibility::Collapsed) {
        std::fprintf(stderr, "INTERACTION FAIL: initial DataTrigger state is incorrect\n");
        return false;
    }

    Base::Result<bool> focused = slotHead->Focus();
    if (!focused || !slotHead->GetIsKeyboardFocused()) {
        std::fprintf(stderr,
            "INTERACTION FAIL: SlotHead focus failed result=%d loaded=%d enabled=%d visible=%d focusable=%d focused=%d status=%s\n",
            focused ? (focused.Value() ? 1 : 0) : -1,
            slotHead->GetIsLoaded() ? 1 : 0,
            slotHead->GetIsEnabled() ? 1 : 0,
            slotHead->GetIsVisible() ? 1 : 0,
            slotHead->GetFocusable() ? 1 : 0,
            slotHead->GetIsKeyboardFocused() ? 1 : 0,
            focused ? "ok" : (focused.GetStatus().message != nullptr
                ? focused.GetStatus().message : "unknown"));
        return false;
    }
    Input::KeyboardInput key;
    key.action = Input::KeyboardAction::Down;
    key.key = Input::KeyboardKeyEnter;
    bool dispatched = SendHostKey(view, key);
    if (!dispatched || fixture.command->GetExecutionCount() != 1U) {
        std::fprintf(stderr, "INTERACTION FAIL: Enter KeyTrigger did not execute command\n");
        return false;
    }
    Base::Ref<Base::Object> firstSlot = fixture.slots->GetItem(0U);
    const Value& enterParameter = fixture.command->GetLastParameter();
    if (enterParameter.Kind() != Meta::ValueKind::Object ||
        enterParameter.IsNullObject() ||
        !enterParameter.AsObject() ||
        enterParameter.AsObject().Get() != firstSlot.Get()) {
        std::fprintf(stderr, "INTERACTION FAIL: Enter command parameter is not SlotHead DataContext\n");
        return false;
    }

    focused = slotChest->Focus();
    if (!focused || !slotChest->GetIsKeyboardFocused()) {
        std::fprintf(stderr, "INTERACTION FAIL: SlotChest focus failed\n");
        return false;
    }
    auto* slot = static_cast<Inventory::Slot*>(firstSlot.Get());
    slot->SetValue(Inventory::Slot::MoveFocusProperty, true);
    if (!slotHead->GetIsKeyboardFocused()) {
        std::fprintf(stderr, "INTERACTION FAIL: PropertyChangedTrigger did not focus SlotHead\n");
        return false;
    }

    key.key = Input::KeyboardKeyEscape;
    dispatched = SendHostKey(view, key);
    if (!dispatched || fixture.command->GetExecutionCount() != 2U ||
        !fixture.command->GetLastParameter().IsNullObject()) {
        std::fprintf(stderr, "INTERACTION FAIL: Escape KeyTrigger parameter is not null\n");
        return false;
    }

    const Value sourceData = slotHead->GetDataContext();
    const Value targetData = slotChest->GetDataContext();
    if (sourceData.Kind() != Meta::ValueKind::Object ||
        sourceData.IsNullObject() || !sourceData.AsObject() ||
        targetData.Kind() != Meta::ValueKind::Object ||
        targetData.IsNullObject() || !targetData.AsObject()) {
        std::fprintf(stderr, "DRAG FAIL: slot DataContext is unavailable\n");
        return false;
    }
    auto* targetSlot = static_cast<Inventory::Slot*>(
        targetData.AsObject().Get());
    auto* dragAdorner =
        root.FindName<Inventory::DragAdornerBehavior>("DragAdorner");
    if (dragAdorner == nullptr) {
        std::fprintf(stderr, "DRAG FAIL: DragAdorner behavior is missing\n");
        return false;
    }

    const Base::Point sourcePoint = RootCenter(*slotHead, root);
    const Base::Point targetPoint = RootCenter(*slotChest, root);
    Input::PointerInput pointer;
    pointer.pointerId = 41U;
    pointer.changedButton = Input::MouseButton::Left;
    pointer.position = sourcePoint;
    pointer.action = Input::PointerAction::Down;
    bool pointerResult = SendHostPointer(view, pointer);
    if (!pointerResult) {
        std::fprintf(stderr, "DRAG FAIL: source pointer down was not routed\n");
        return false;
    }
    pointer.action = Input::PointerAction::Move;
    pointer.position = {sourcePoint.x + 2.0, sourcePoint.y + 2.0};
    pointerResult = SendHostPointer(view, pointer);
    if (!pointerResult || !slotHead->GetIsDragging() ||
        fixture.command->GetExecutionCount() != 3U ||
        !IsObjectValue(
            fixture.command->GetParameter(2U),
            sourceData.AsObject().Get())) {
        const UIElement* hitTarget = nullptr;
        std::fprintf(stderr,
            "DRAG FAIL: drag did not start from SlotHead count=%u dragging=%d source=(%.2f,%.2f) hit=%p slot=%p attached=%u\n",
            fixture.command->GetExecutionCount(),
            slotHead->GetIsDragging() ? 1 : 0,
            sourcePoint.x, sourcePoint.y,
            static_cast<const void*>(hitTarget),
            static_cast<const void*>(slotHead),
            Inventory::DragItemBehavior::attachedCount);
        const ::Aero::Media::Visual* trace = hitTarget;
        while (trace != nullptr) {
            std::fprintf(stderr, "  hit route node=%p type=%llu\n",
                static_cast<const void*>(trace),
                static_cast<unsigned long long>(trace->RuntimeType()));
            if (trace == slotHead || trace == &root) break;
            trace = trace->GetVisualParent();
        }
        std::fprintf(stderr,
            "  slot render=(%.2f,%.2f) layout=(%.2f,%.2f,%.2f,%.2f)\n",
            slotHead->GetRenderSize().width,
            slotHead->GetRenderSize().height,
            slotHead->GetLayoutSlot().x,
            slotHead->GetLayoutSlot().y,
            slotHead->GetLayoutSlot().width,
            slotHead->GetLayoutSlot().height);
        trace = slotHead;
        while (trace != nullptr) {
            const UIElement* e = trace->AsUIElement();
            const Rect r = e != nullptr ? e->GetLayoutSlot() : Rect{};
            std::fprintf(stderr,
                "  slot route node=%p type=%llu layout=(%.2f,%.2f,%.2f,%.2f)\n",
                static_cast<const void*>(trace),
                static_cast<unsigned long long>(trace->RuntimeType()),
                r.x, r.y, r.width, r.height);
            if (trace == &root) break;
            trace = trace->GetVisualParent();
        }
        return false;
    }

    pointer.position = targetPoint;
    pointerResult = SendHostPointer(view, pointer);
    if (!pointerResult) {
        std::fprintf(stderr, "DRAG FAIL: drag over target was not routed\n");
        return false;
    }
    AdvanceView(view, timeInSeconds, 16U);
    if (!targetSlot->GetValueOr(
            Inventory::Slot::IsDragOverProperty, false)) {
        std::fprintf(stderr,
            "DRAG FAIL: DragEnter did not update target state enter=%u leave=%u allow=%d last=%p chest=%p hit=%p\n",
            Inventory::DropItemBehavior::enterCount,
            Inventory::DropItemBehavior::leaveCount,
            slotChest->GetAllowDrop() ? 1 : 0,
            static_cast<void*>(Inventory::DropItemBehavior::lastEntered),
            static_cast<void*>(slotChest),
            nullptr);
        if (Inventory::DropItemBehavior::lastEnteredBehavior != nullptr) {
            const auto bindings = Inventory::DropItemBehavior::lastEnteredBehavior->GetAuthoredBindings();
            std::fprintf(stderr,
                "  behavior value=%d authored=%u mode=%u path=%.*s\n",
                Inventory::DropItemBehavior::lastEnteredBehavior->GetValueOr(
                    Inventory::DropItemBehavior::IsDragOverProperty, false) ? 1 : 0,
                bindings.Size(),
                bindings.Empty() || !bindings[0].binding
                    ? 255U
                    : static_cast<unsigned>(bindings[0].binding->GetMode()),
                bindings.Empty() || !bindings[0].binding
                    ? 0
                    : static_cast<int>(bindings[0].binding->GetPath().GetPath().SizeBytes()),
                bindings.Empty() || !bindings[0].binding
                    ? ""
                    : bindings[0].binding->GetPath().GetPath().Data());
        }
        return false;
    }
    const double draggedX = dragAdorner->GetValueOr(
        Inventory::DragAdornerBehavior::DraggedItemXProperty, 0.0);
    const double draggedY = dragAdorner->GetValueOr(
        Inventory::DragAdornerBehavior::DraggedItemYProperty, 0.0);
    if (draggedX == 0.0 && draggedY == 0.0) {
        std::fprintf(stderr,
            "DRAG FAIL: DragAdorner coordinates were not updated (%.2f, %.2f)\n",
            draggedX, draggedY);
        return false;
    }

    pointer.action = Input::PointerAction::Up;
    pointerResult = SendHostPointer(view, pointer);
    AdvanceView(view, timeInSeconds, 16U);
    if (!pointerResult || slotHead->GetIsDragging() ||
        fixture.command->GetExecutionCount() != 5U ||
        !IsObjectValue(
            fixture.command->GetParameter(3U),
            targetData.AsObject().Get()) ||
        fixture.command->GetParameter(4U).Kind() !=
            Meta::ValueKind::Boolean ||
        !fixture.command->GetParameter(4U).AsBoolean() ||
        targetSlot->GetValueOr(
            Inventory::Slot::IsDragOverProperty, false)) {
        std::fprintf(stderr, "DRAG FAIL: drop/completion semantics are incorrect\n");
        return false;
    }

    pointer.pointerId = 42U;
    pointer.position = sourcePoint;
    pointer.action = Input::PointerAction::Down;
    pointerResult = SendHostPointer(view, pointer);
    pointer.action = Input::PointerAction::Move;
    pointer.position = {sourcePoint.x + 2.0, sourcePoint.y + 2.0};
    if (pointerResult) pointerResult = SendHostPointer(view, pointer);
    if (!pointerResult || !slotHead->GetIsDragging() ||
        fixture.command->GetExecutionCount() != 6U) {
        std::fprintf(stderr, "DRAG FAIL: cancel test did not start a drag\n");
        return false;
    }
    key.action = Input::KeyboardAction::Down;
    key.key = Input::KeyboardKeyEscape;
    dispatched = SendHostKey(view, key);
    if (!dispatched || slotHead->GetIsDragging() ||
        fixture.command->GetExecutionCount() != 7U ||
        fixture.command->GetParameter(6U).Kind() !=
            Meta::ValueKind::Boolean ||
        fixture.command->GetParameter(6U).AsBoolean()) {
        std::fprintf(stderr,
            "DRAG FAIL: Escape did not cancel the drag dispatch=%d dragging=%d count=%u kind=%u value=%d\n",
            dispatched ? 1 : 0,
            slotHead->GetIsDragging() ? 1 : 0,
            fixture.command->GetExecutionCount(),
            fixture.command->GetExecutionCount() > 6U
                ? static_cast<unsigned>(fixture.command->GetParameter(6U).Kind()) : 255U,
            fixture.command->GetExecutionCount() > 6U &&
                fixture.command->GetParameter(6U).Kind() == Meta::ValueKind::Boolean
                ? (fixture.command->GetParameter(6U).AsBoolean() ? 1 : 0) : -1);
        return false;
    }
    return true;
}

bool LoadOne(Aero::Gui& gui, const char* relative) {
    Inventory::DragAdornerBehavior::ResetCounters();
    Inventory::DragItemBehavior::ResetCounters();
    Inventory::DropItemBehavior::ResetCounters();
    Aero::Markup::XamlReader reader(gui);
    Aero::Diagnostics::DiagnosticBag diagnostics;
    Aero::Base::Result<Aero::Markup::XamlDocument> resourceDocument =
        reader.Load(
            "pack://application:,,,/Inventory;component/Resources.xaml",
            {},
            &diagnostics);
    if (!resourceDocument) {
        std::fprintf(stderr, "RESOURCES FAIL: %s\n",
            resourceDocument.GetStatus().message);
        PrintDiagnostics(diagnostics);
        return false;
    }
    Aero::ResourceDictionary* resources =
        resourceDocument.Value().Root<Aero::ResourceDictionary>();
    if (resources == nullptr) {
        std::fprintf(stderr,
            "RESOURCES FAIL: resource root is not ResourceDictionary\n");
        return false;
    }
    ::Aero::ViewOptions viewOptions;
    viewOptions.automaticAnimationClock = false;
    viewOptions.applicationResources = resources;
    viewOptions.diagnostics = &diagnostics;
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> made =
        gui.CreateView(viewOptions);
    if (!made) { std::fprintf(stderr, "view failed: %s\n", made.GetStatus().message); return false; }
    Aero::Base::Ref<Aero::View> view = std::move(made).Value();
    std::string uri = "pack://application:,,,/Inventory;component/";
    uri += relative;
    diagnostics.Clear();
    Aero::Base::Result<Aero::Markup::XamlDocument> loaded =
        reader.Load(
            Aero::Base::StringView(
                uri.data(),
                static_cast<std::uint32_t>(uri.size())),
            *resources,
            {},
            &diagnostics);
    if (!loaded) {
        std::fprintf(stderr, "LOAD FAIL %s: %s\n", relative, loaded.GetStatus().message);
        PrintDiagnostics(diagnostics);
        return false;
    }
    if (std::strcmp(relative, "App.xaml") == 0 || std::strcmp(relative, "InventoryAtlas.xaml") == 0 || std::strcmp(relative, "Resources.xaml") == 0) {
        std::printf("LOAD OK %s\n", relative);
        return true;
    }
    Aero::Base::Result<ViewModelFixture> vm = CreateViewModel();
    if (!vm) { std::fprintf(stderr, "view model failed: %s\n", vm.GetStatus().message); return false; }
    Aero::FrameworkElement* root =
        loaded.Value().Root<Aero::FrameworkElement>();
    if (root != nullptr) {
        root->SetDataContext(Aero::Base::Ref<Aero::Base::Object>(
            vm.Value().viewModel));
    }
    Aero::Base::Result<void> mounted = view->SetContent(std::move(loaded).Value(), {1000.0, 600.0});
    if (!mounted) { std::fprintf(stderr, "MOUNT FAIL %s: %s\n", relative, mounted.GetStatus().message); return false; }
    double timeInSeconds = 0.0;
    view->Update(timeInSeconds);
    AdvanceView(*view, timeInSeconds, 16U);
    if (std::strcmp(relative, "MainWindow.xaml") == 0) {
        AdvanceView(*view, timeInSeconds, 3200U);
        if (root == nullptr ||
            !VerifyMainWindowInteractions(
                *view, timeInSeconds, *root, vm.Value())) {
            return false;
        }
        const std::uint32_t adornerAttached =
            Inventory::DragAdornerBehavior::attachedCount;
        const std::uint32_t dragAttached =
            Inventory::DragItemBehavior::attachedCount;
        const std::uint32_t dropAttached =
            Inventory::DropItemBehavior::attachedCount;
        if (adornerAttached != 1U || dragAttached == 0U ||
            dropAttached == 0U || dragAttached != dropAttached) {
            std::fprintf(stderr, "INTERACTION FAIL: Behavior instances were not attached per target\n");
            return false;
        }
        view.Reset();
        if (Inventory::DragAdornerBehavior::detachedCount !=
                adornerAttached ||
            Inventory::DragItemBehavior::detachedCount != dragAttached ||
            Inventory::DropItemBehavior::detachedCount != dropAttached) {
            std::fprintf(stderr, "INTERACTION FAIL: Behavior detach lifecycle is not symmetric\n");
            return false;
        }
    }
    std::printf("LOAD OK %s\n", relative);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: aero-inventory-conformance <inventory-root> [xaml]\n");
        return 2;
    }
    Aero::Base::Result<Aero::Base::Ref<InventoryXamlProvider>> provider =
        Aero::Base::MakeRef<InventoryXamlProvider>(std::string(argv[1]));
    Aero::Gui gui;
    Aero::Base::Result<void> result =
        gui.AddModule(Aero::App::AppMetadataModule());
    if (result) result = gui.AddModule(InventoryModule);
    if (result && !provider) result = provider.GetStatus();
    if (result) result = gui.SetXamlProvider(
        std::move(provider).Value(), "pack", "Inventory");
    if (result) result = gui.Initialize();
    if (!result) { std::fprintf(stderr, "initialization failed: %s\n", result.GetStatus().message); return 1; }
    if (argc >= 3) return LoadOne(gui, argv[2]) ? 0 : 1;
    const char* files[] = {"AnimatedNumber.xaml", "App.xaml", "InventoryAtlas.xaml", "MainWindow.xaml"};
    bool success = true;
    for (const char* file : files) success = LoadOne(gui, file) && success;
    return success ? 0 : 1;
}
