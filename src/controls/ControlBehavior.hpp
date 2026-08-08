#pragma once

#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "TemplateInstance.hpp"

// Private control behavior and template implementation for one View.
// Public controls expose WPF semantics; these classes only retain interaction state.
#include <Aero/Controls.hpp>
#include <Aero/Documents.hpp>
#include <Aero/Gui/Text.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Gui/ControlTemplate.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Input/Platform.hpp>
#include <cstddef>
#include <new>
#include <utility>

namespace Aero::Controls {

// Interaction bookkeeping for ToggleButton lives in the control runtime, not
// in the public WPF-shaped controls headers.
enum class ToggleState : std::uint8_t {
    Unchecked = 0U,
    Checked,
    Indeterminate,
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {

using namespace Aero::Meta;
using namespace Aero::Threading;

using namespace Aero::Controls;
using namespace Aero::Controls::Primitives;
using Aero::Controls::Detail::TemplateHandle;

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Primitives {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct ButtonBase::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events,
        InputRouter& input,
        VisualStateManager* states = nullptr) noexcept;
    ~Impl() noexcept;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(ButtonBase& button) noexcept;
    Base::Result<bool> Detach(ButtonBase& button) noexcept;
    Base::Result<void> RefreshCanExecute(
        ButtonBase& button) noexcept;
    Base::Result<void> RefreshVisualState(
        ButtonBase& button,
        bool useTransitions = true) noexcept {
        return SyncVisualState(button, useTransitions);
    }
    // Host-driven deterministic clock for RepeatButton. A single call emits
    // at most 1024 repeats and skips excess backlog.
    Base::Result<std::uint32_t> AdvanceTime(
        std::uint32_t elapsedMilliseconds) noexcept;

private:
    friend class Aero::Controls::Primitives::ButtonBase;
    struct ButtonRecord {
        VisualHandle handle;
        Base::Ref<ICommand> command;
        std::uint32_t pointerId = 0U;
        bool pointerDown = false;
        bool keyboardDown = false;
        bool wasMouseOver = false;
        std::uint64_t repeatElapsed = 0U;
        std::uint64_t nextRepeat = 0U;
        ToggleState toggleState = ToggleState::Unchecked;
        bool updatingToggle = false;
    };

    ElementTree* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    InputRouter* input_ = nullptr;
    VisualStateManager* states_ = nullptr;
    Base::Vector<ButtonRecord> buttons_;
    MouseButtonEventHandler mouseDownHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    KeyEventHandler keyUpHandler_;
    KeyboardFocusChangedEventHandler focusChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    PointerStateChangedHandler pointerStateChangedHandler_;
    PointerCaptureChangedHandler captureChangedHandler_;
    RequerySuggestedHandler requeryHandler_;
    bool initialized_ = false;

    std::uint32_t FindButton(const ButtonBase& button) const noexcept;
    ButtonBase* ResolveButton(std::uint32_t index) noexcept;
    Base::Result<void> SubscribeCommand(
        ButtonBase& button,
        ButtonRecord& record) noexcept;
    void UnsubscribeCommand(ButtonRecord& record) noexcept;
    void RemoveAt(std::uint32_t index) noexcept;
    Base::Result<void> InvokeClick(ButtonBase& button) noexcept;
    Base::Result<void> ApplyToggleState(
        ToggleButton& button,
        ToggleState state) noexcept;
    void PublishToggleState(
        ToggleButton& button,
        ButtonRecord& record) noexcept;
    void UncheckRadioPeers(RadioButton& button) noexcept;
    Base::Result<void> SyncVisualState(
        ButtonBase& button,
        bool useTransitions = true) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnMouseUp(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
    void OnKeyUp(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
    void OnFocusChanged(
        Base::Object* sender,
        KeyboardFocusChangedEventArgs& args) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnPointerStateChanged(UIElement& element) noexcept;
    void OnCaptureChanged(
        std::uint32_t pointerId,
        UIElement* target,
        bool captured) noexcept;
    void OnRequerySuggested() noexcept;
};

} // namespace Aero::Controls::Primitives

namespace Aero::Controls::Detail {
using ButtonBehavior = ::Aero::Controls::Primitives::ButtonBase::Impl;
}

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct TextBox::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events,
        InputRouter& input,
        Input::IClipboard& clipboard) noexcept;
    ~Impl() noexcept;

    Base::Result<void> Attach(
        TextBox& textBox) noexcept;
    Base::Result<void> Attach(
        PasswordBox& passwordBox) noexcept;
    Base::Result<bool> Detach(
        TextBox& textBox) noexcept;
    Base::Result<bool> Detach(
        PasswordBox& passwordBox) noexcept;

private:
    struct Record {
        VisualHandle handle;
        std::uint32_t pointerId = 0U;
        std::uint32_t anchor = 0U;
        bool dragging = false;
        bool password = false;
    };

    ElementTree* tree_ = nullptr;
    [[maybe_unused]] EventRouter* events_ = nullptr;
    InputRouter* input_ = nullptr;
    Input::IClipboard* clipboard_ = nullptr;
    Base::Vector<Record> records_;
    MouseButtonEventHandler mouseDownHandler_;
    MouseEventHandler mouseMoveHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    TextCompositionEventHandler textInputHandler_;
    KeyboardFocusChangedEventHandler focusChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    PointerCaptureChangedHandler captureChangedHandler_;
    bool captureSubscribed_ = false;

    std::uint32_t Find(
        const UIElement& owner) const noexcept;
    UIElement* ResolveOwner(
        std::uint32_t index) noexcept;
    TextBox* ResolveEditor(
        std::uint32_t index) noexcept;
    void RemoveAt(
        std::uint32_t index) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnMouseMove(
        Base::Object* sender,
        MouseEventArgs& args) noexcept;
    void OnMouseUp(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
    void OnTextInput(
        Base::Object* sender,
        TextCompositionEventArgs& args) noexcept;
    void OnFocusChanged(
        Base::Object* sender,
        KeyboardFocusChangedEventArgs& args) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnCaptureChanged(
        std::uint32_t pointerId,
        UIElement* target,
        bool captured) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using TextEditBehavior = ::Aero::Controls::TextBox::Impl;
}

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct ScrollViewer::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events) noexcept;
    ~Impl() noexcept;

    Base::Result<void> Attach(
        ScrollViewer& viewer) noexcept;
    Base::Result<bool> Detach(
        ScrollViewer& viewer) noexcept;

private:
    struct ViewerRecord {
        ScrollViewer* viewer = nullptr;
        VisualHandle handle;
    };

    ElementTree* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    Base::Vector<ViewerRecord> viewers_;
    MouseWheelEventHandler wheelHandler_;

    void OnMouseWheel(
        Base::Object* sender,
        MouseWheelEventArgs& args) noexcept;
    std::uint32_t FindViewer(
        const ScrollViewer& viewer) const noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using ScrollBehavior = ::Aero::Controls::ScrollViewer::Impl;
}

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct Slider::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events,
        InputRouter& input) noexcept;
    ~Impl() noexcept;

    Base::Result<void> Attach(
        Slider& slider) noexcept;
    Base::Result<bool> Detach(
        Slider& slider) noexcept;

private:
    struct SliderRecord {
        VisualHandle handle;
        Input::CommandBindingHandle decreaseSmallCommand;
        Input::CommandBindingHandle increaseSmallCommand;
        Input::CommandBindingHandle decreaseLargeCommand;
        Input::CommandBindingHandle increaseLargeCommand;
        std::uint32_t pointerId = 0U;
        bool dragging = false;
    };

    ElementTree* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    InputRouter* input_ = nullptr;
    Base::Vector<SliderRecord> sliders_;
    MouseButtonEventHandler mouseDownHandler_;
    MouseEventHandler mouseMoveHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    PointerCaptureChangedHandler captureChangedHandler_;
    ExecutedRoutedEventHandler decreaseSmallHandler_;
    ExecutedRoutedEventHandler increaseSmallHandler_;
    ExecutedRoutedEventHandler decreaseLargeHandler_;
    ExecutedRoutedEventHandler increaseLargeHandler_;

    std::uint32_t Find(
        const Slider& slider) const noexcept;
    Slider* Resolve(
        std::uint32_t index) noexcept;
    void RemoveAt(
        std::uint32_t index) noexcept;
    Base::Result<void> SetFromPoint(
        Slider& slider,
        Point point) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnMouseMove(
        Base::Object* sender,
        MouseEventArgs& args) noexcept;
    void OnMouseUp(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
    void OnCaptureChanged(
        std::uint32_t pointerId,
        UIElement* target,
        bool captured) noexcept;
    void OnDecreaseSmallCommand(
        Base::Object* sender,
        ExecutedRoutedEventArgs& args) noexcept;
    void OnIncreaseSmallCommand(
        Base::Object* sender,
        ExecutedRoutedEventArgs& args) noexcept;
    void OnDecreaseLargeCommand(
        Base::Object* sender,
        ExecutedRoutedEventArgs& args) noexcept;
    void OnIncreaseLargeCommand(
        Base::Object* sender,
        ExecutedRoutedEventArgs& args) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using SliderBehavior = ::Aero::Controls::Slider::Impl;
}

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct TreeView::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events,
        InputRouter& input,
        VisualStateManager* states = nullptr)
        noexcept;
    ~Impl() noexcept;

    Base::Result<void> Attach(
        TreeView& treeView) noexcept;
    Base::Result<bool> Detach(
        TreeView& treeView) noexcept;

private:
    ElementTree* tree_ = nullptr;
    [[maybe_unused]]
    EventRouter* events_ = nullptr;
    InputRouter* input_ = nullptr;
    VisualStateManager* states_ = nullptr;
    Base::Vector<VisualHandle> records_;
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    std::uint32_t FindTreeView(
        const TreeView& treeView) const noexcept;
    TreeView* ResolveTreeView(
        std::uint32_t index) noexcept;
    TreeViewItem* FindItem(
        TreeView& treeView,
        Base::Object* source) const noexcept;
    Base::Result<void> CollectVisibleItems(
        ::Aero::Media::Visual& parent,
        Base::Vector<TreeViewItem*>& items)
        noexcept;
    void OnMouseDown(
        Base::Object* sender,
        MouseButtonEventArgs& args)
        noexcept;
    void OnKeyDown(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using TreeBehavior = ::Aero::Controls::TreeView::Impl;
}

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct ComboBox::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events,
        InputRouter& input) noexcept;
    ~Impl() noexcept;

    Base::Result<void> Attach(
        ComboBox& comboBox) noexcept;
    Base::Result<bool> Detach(
        ComboBox& comboBox) noexcept;

private:
    ElementTree* tree_ = nullptr;
    [[maybe_unused]]
    EventRouter* events_ = nullptr;
    InputRouter* input_ = nullptr;
    Base::Vector<VisualHandle> records_;
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    std::uint32_t FindComboBox(
        const ComboBox& comboBox) const noexcept;
    ComboBox* ResolveComboBox(
        std::uint32_t index) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using ComboBehavior = ::Aero::Controls::ComboBox::Impl;
}

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct ListBox::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events,
        InputRouter& input,
        VisualStateManager* states = nullptr) noexcept;
    ~Impl() noexcept;

    Base::Result<void> Attach(ListBox& listBox) noexcept;
    Base::Result<bool> Detach(ListBox& listBox) noexcept;

private:
    struct Record {
        VisualHandle handle;
        std::uint32_t anchorIndex = UINT32_MAX;
    };

    ElementTree* tree_ = nullptr;
    [[maybe_unused]] EventRouter* events_ = nullptr;
    InputRouter* input_ = nullptr;
    VisualStateManager* states_ = nullptr;
    Base::Vector<Record> records_;
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    std::uint32_t FindListBox(
        const ListBox& listBox) const noexcept;
    ListBox* ResolveListBox(
        std::uint32_t index) noexcept;
    std::uint32_t FindContainerIndex(
        ListBox& listBox,
        Base::Object* source) const noexcept;
    Base::Result<bool> ApplyUserSelection(
        ListBox& listBox,
        Record& record,
        std::uint32_t index,
        std::uint32_t modifiers) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using ListBehavior = ::Aero::Controls::ListBox::Impl;

class AERO_API TemplateEngine {
public:
    TemplateEngine(
        ElementTree& tree,
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties,
        LayoutEngine* layout = nullptr,
        ::Aero::Render::Detail::RenderTree* renderer = nullptr,
        ::Aero::Meta::Registry* metadata = nullptr,
        Aero::GuiPrivate::Detail::BindingEngine* bindings = nullptr) noexcept
        : tree_(&tree),
          providerSession_(values),
          values_(&providerSession_),
          properties_(&properties),
          layout_(layout),
          renderer_(renderer),
          metadata_(metadata),
          bindings_(bindings),
          propertyChangedHandler_(
              this, &TemplateEngine::OnPropertyChanged) {}
    ~TemplateEngine() noexcept;

    Base::Result<TemplateHandle> Apply(
        Control& control,
        const ControlTemplate& plan) noexcept;
    Base::Result<bool> Clear(
        TemplateHandle handle) noexcept;
    Base::Result<bool> Clear(
        Control& control) noexcept;
    DependencyObject* FindName(
        TemplateHandle handle,
        Base::StringView name) const noexcept;
    DependencyObject* FindPart(
        TemplateHandle handle,
        TypeId type) const noexcept;
    TemplateHandle AppliedHandle(
        const Control& control) const noexcept;
    const ControlTemplate* AppliedTemplate(
        TemplateHandle handle) const noexcept;

private:
    struct Instance {
        TemplateHandle handle;
        Control* parent = nullptr;
        const ControlTemplate* plan = nullptr;
        ::Aero::Media::Visual* rootVisual = nullptr;
        UIElement* rootElement = nullptr;
        Base::Vector<Aero::Controls::Detail::TemplatePart> parts;
        Base::Vector<Aero::Controls::Detail::TemplateContentProjection> projections;
        NameScope names;
        Base::Vector<Data::BindingHandle>
            metadataBindings;
    };

    ElementTree* tree_ = nullptr;
    ::Aero::GuiPrivate::Detail::TemplatedParentProviderSession providerSession_;
    ::Aero::GuiPrivate::Detail::TemplatedParentProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    LayoutEngine* layout_ = nullptr;
    ::Aero::Render::Detail::RenderTree* renderer_ = nullptr;
    ::Aero::Meta::Registry* metadata_ = nullptr;
    Aero::GuiPrivate::Detail::BindingEngine* bindings_ = nullptr;
    Base::Vector<Instance> instances_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    std::uint64_t nextHandle_ = 1U;

    std::uint32_t FindInstance(
        TemplateHandle handle) const noexcept;
    std::uint32_t FindInstance(
        const Control& control) const noexcept;
    DependencyObject* FindTarget(
        const Instance& instance,
        Base::StringView name) const noexcept;
    Base::Result<void> Subscribe(
        Instance& instance) noexcept;
    void Unsubscribe(Instance& instance) noexcept;
    Base::Result<void> ApplyBindings(
        Instance& instance,
        DependencyPropertyHandle changed =
            DependencyPropertyHandle{}) noexcept;
    Base::Result<void> AttachMetadataBindings(
        Instance& instance) noexcept;
    void DetachMetadataBindings(
        Instance& instance) noexcept;
    Base::Result<void> EvaluateTriggers(
        Instance& instance) noexcept;
    Base::Result<void> ClearProviders(
        Instance& instance) noexcept;
    Base::Result<void> ClearAt(
        std::uint32_t index) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

} // namespace Aero::Controls::Detail

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

struct Menu::Impl {
public:
    Impl(
        ElementTree& tree,
        EventRouter& events,
        InputRouter& input) noexcept;
    ~Impl() noexcept;

    Base::Result<void> Attach(
        Menu& menu) noexcept;
    Base::Result<bool> Detach(
        Menu& menu) noexcept;

private:
    ElementTree* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    InputRouter* input_ = nullptr;
    Base::Vector<VisualHandle> records_;
    MouseButtonEventHandler mouseDownHandler_;
    KeyEventHandler keyDownHandler_;

    std::uint32_t FindMenu(
        const Menu& menu) const noexcept;
    Menu* ResolveMenu(
        std::uint32_t index) noexcept;
    MenuItem* FindItem(
        Menu& menu,
        Base::Object* source) const noexcept;
    Base::Result<void> Invoke(
        Menu& menu,
        MenuItem& item) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        MouseButtonEventArgs& args)
        noexcept;
    void OnKeyDown(
        Base::Object* sender,
        KeyEventArgs& args) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {
using MenuBehavior = ::Aero::Controls::Menu::Impl;
}

namespace Aero::Controls::Detail {

// One compact owner for all built-in control interaction state. The behavior
// objects are placement-constructed in inline storage, avoiding per-behavior
// heap allocations and eliminating the old Service/Access routing chain.
class ControlBehavior {
public:
    ControlBehavior(
        Base::IAllocator& allocator,
        ::Aero::Meta::Registry& metadata,
        Aero::ElementTree& tree,
        Aero::GuiPrivate::Detail::EventRouter& events,
        Aero::GuiPrivate::Detail::InputRouter& input,
        VisualStateManager* visualStates,
        ::Aero::Input::IClipboard* clipboard,
        bool controlsEnabled,
        bool textEditingEnabled) noexcept;
    ~ControlBehavior() noexcept;

    ControlBehavior(const ControlBehavior&) = delete;
    ControlBehavior& operator=(const ControlBehavior&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(
        ::Aero::Media::Visual& visual,
        ::Aero::Input::ITextInputMethodHost* inputMethodHost) noexcept;
    Base::Result<bool> Detach(::Aero::Media::Visual& visual) noexcept;
    Base::Result<void> RefreshButtonVisualState(
        Primitives::ButtonBase& button,
        bool useTransitions = true) noexcept;
    Base::Result<std::uint32_t> AdvanceTime(
        std::uint32_t elapsedMilliseconds) noexcept;
    void Shutdown() noexcept;

    static void SetVisualStateManager(
        Control& control,
        VisualStateManager* visualStates) noexcept;

private:
    template<class T, class... TArgs>
    Base::Result<T*> Construct(TArgs&&... arguments) noexcept;
    template<class T>
    void Destroy(T*& object) noexcept;

    static constexpr std::size_t StorageBytes =
        sizeof(Aero::Controls::Detail::ButtonBehavior) +
        sizeof(Aero::Controls::Detail::TextEditBehavior) +
        sizeof(Aero::Controls::Detail::ScrollBehavior) +
        sizeof(Aero::Controls::Detail::SliderBehavior) +
        sizeof(Aero::Controls::Detail::ListBehavior) +
        sizeof(Aero::Controls::Detail::ComboBehavior) +
        sizeof(Aero::Controls::Detail::TreeBehavior) +
        sizeof(Aero::Controls::Detail::MenuBehavior) +
        9U * alignof(std::max_align_t);

    Base::IAllocator* allocator_ = nullptr;
    ::Aero::Meta::Registry* metadata_ = nullptr;
    Aero::ElementTree* tree_ = nullptr;
    Aero::GuiPrivate::Detail::EventRouter* events_ = nullptr;
    Aero::GuiPrivate::Detail::InputRouter* input_ = nullptr;
    VisualStateManager* visualStates_ = nullptr;
    ::Aero::Input::IClipboard* clipboard_ = nullptr;
    bool controlsEnabled_ = false;
    bool textEditingEnabled_ = false;
    bool initialized_ = false;
    std::size_t offset_ = 0U;
    alignas(std::max_align_t) std::byte storage_[StorageBytes]{};

    Aero::Controls::Detail::ButtonBehavior* buttons_ = nullptr;
    Aero::Controls::Detail::TextEditBehavior* textBoxes_ = nullptr;
    Aero::Controls::Detail::ScrollBehavior* scrolling_ = nullptr;
    Aero::Controls::Detail::SliderBehavior* sliders_ = nullptr;
    Aero::Controls::Detail::ListBehavior* lists_ = nullptr;
    Aero::Controls::Detail::ComboBehavior* combos_ = nullptr;
    Aero::Controls::Detail::TreeBehavior* trees_ = nullptr;
    Aero::Controls::Detail::MenuBehavior* menus_ = nullptr;
};

} // namespace Aero::Controls::Detail
