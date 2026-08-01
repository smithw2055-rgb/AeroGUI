#pragma once

#include "gui/ElementInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "TemplateInstance.hpp"

// Private runtime declarations extracted from public authoring headers.
// These services are owned by View/runtime composition and are not part
// of the normal WPF control-authoring surface.
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Documents.hpp>
#include <Aero/Controls/Text.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Controls/Standard.hpp>
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/RoutedEventInternal.hpp"

namespace Aero::Detail {

using namespace Aero::Core;

using namespace Aero::Controls;
using namespace Aero::Controls::Primitives;
using Aero::Controls::Detail::TemplateHandle;

class AERO_API ControlRuntimeAccess::ControlInteractionManager final {
public:
    ControlInteractionManager(
        GuiContext& tree,
        EventRouter& events,
        InputService& input,
        VisualStateManager* states = nullptr) noexcept;
    ~ControlInteractionManager() noexcept;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(ButtonBase& button) noexcept;
    Base::Result<bool> Detach(ButtonBase& button) noexcept;
    Base::Result<void> RefreshCanExecute(
        ButtonBase& button) noexcept;
    // Host-driven deterministic clock for RepeatButton. A single call emits
    // at most 1024 repeats and skips excess backlog.
    Base::Result<std::uint32_t> AdvanceTime(
        std::uint32_t elapsedMilliseconds) noexcept;

private:
    friend class Aero::Controls::Primitives::ButtonBase;
    struct ButtonRecord final {
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

    GuiContext* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    InputService* input_ = nullptr;
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
class AERO_API ControlRuntimeAccess::TextBoxInteractionManager final {
public:
    TextBoxInteractionManager(
        GuiContext& tree,
        EventRouter& events,
        InputService& input,
        Integration::IClipboard& clipboard) noexcept;
    ~TextBoxInteractionManager() noexcept;

    Base::Result<void> Attach(
        TextBox& textBox) noexcept;
    Base::Result<void> Attach(
        PasswordBox& passwordBox) noexcept;
    Base::Result<bool> Detach(
        TextBox& textBox) noexcept;
    Base::Result<bool> Detach(
        PasswordBox& passwordBox) noexcept;

private:
    struct Record final {
        VisualHandle handle;
        std::uint32_t pointerId = 0U;
        std::uint32_t anchor = 0U;
        bool dragging = false;
        bool password = false;
    };

    GuiContext* tree_ = nullptr;
    [[maybe_unused]] EventRouter* events_ = nullptr;
    InputService* input_ = nullptr;
    Integration::IClipboard* clipboard_ = nullptr;
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
class AERO_API ControlRuntimeAccess::ScrollInteractionManager final {
public:
    ScrollInteractionManager(
        GuiContext& tree,
        EventRouter& events) noexcept;
    ~ScrollInteractionManager() noexcept;

    Base::Result<void> Attach(
        ScrollViewer& viewer) noexcept;
    Base::Result<bool> Detach(
        ScrollViewer& viewer) noexcept;

private:
    struct ViewerRecord final {
        ScrollViewer* viewer = nullptr;
        VisualHandle handle;
    };

    GuiContext* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    Base::Vector<ViewerRecord> viewers_;
    MouseWheelEventHandler wheelHandler_;

    void OnMouseWheel(
        Base::Object* sender,
        MouseWheelEventArgs& args) noexcept;
    std::uint32_t FindViewer(
        const ScrollViewer& viewer) const noexcept;
};
class AERO_API ControlRuntimeAccess::SliderInteractionManager final {
public:
    SliderInteractionManager(
        GuiContext& tree,
        EventRouter& events,
        InputService& input) noexcept;
    ~SliderInteractionManager() noexcept;

    Base::Result<void> Attach(
        Slider& slider) noexcept;
    Base::Result<bool> Detach(
        Slider& slider) noexcept;

private:
    struct SliderRecord final {
        VisualHandle handle;
        std::uint32_t pointerId = 0U;
        bool dragging = false;
    };

    GuiContext* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    InputService* input_ = nullptr;
    Base::Vector<SliderRecord> sliders_;
    MouseButtonEventHandler mouseDownHandler_;
    MouseEventHandler mouseMoveHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    PointerCaptureChangedHandler captureChangedHandler_;

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
};
class AERO_API ControlRuntimeAccess::TreeViewInteractionManager final {
public:
    TreeViewInteractionManager(
        GuiContext& tree,
        EventRouter& events,
        InputService& input,
        VisualStateManager* states = nullptr)
        noexcept;
    ~TreeViewInteractionManager() noexcept;

    Base::Result<void> Attach(
        TreeView& treeView) noexcept;
    Base::Result<bool> Detach(
        TreeView& treeView) noexcept;

private:
    GuiContext* tree_ = nullptr;
    [[maybe_unused]]
    EventRouter* events_ = nullptr;
    InputService* input_ = nullptr;
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
        Visual& parent,
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
class AERO_API ControlRuntimeAccess::ComboBoxInteractionManager final {
public:
    ComboBoxInteractionManager(
        GuiContext& tree,
        EventRouter& events,
        InputService& input) noexcept;
    ~ComboBoxInteractionManager() noexcept;

    Base::Result<void> Attach(
        ComboBox& comboBox) noexcept;
    Base::Result<bool> Detach(
        ComboBox& comboBox) noexcept;

private:
    GuiContext* tree_ = nullptr;
    [[maybe_unused]]
    EventRouter* events_ = nullptr;
    InputService* input_ = nullptr;
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
class AERO_API ControlRuntimeAccess::ListBoxInteractionManager final {
public:
    ListBoxInteractionManager(
        GuiContext& tree,
        EventRouter& events,
        InputService& input,
        VisualStateManager* states = nullptr) noexcept;
    ~ListBoxInteractionManager() noexcept;

    Base::Result<void> Attach(ListBox& listBox) noexcept;
    Base::Result<bool> Detach(ListBox& listBox) noexcept;

private:
    struct Record final {
        VisualHandle handle;
        std::uint32_t anchorIndex = UINT32_MAX;
    };

    GuiContext* tree_ = nullptr;
    [[maybe_unused]] EventRouter* events_ = nullptr;
    InputService* input_ = nullptr;
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
class AERO_API ControlRuntimeAccess::TemplateManager final {
public:
    TemplateManager(
        GuiContext& tree,
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties,
        LayoutManager* layout = nullptr,
        RenderTree* renderer = nullptr,
        Core::MetadataRuntime* metadata = nullptr,
        Aero::Detail::BindingManager* bindings = nullptr) noexcept
        : tree_(&tree),
          providerSession_(values),
          values_(&providerSession_),
          properties_(&properties),
          layout_(layout),
          renderer_(renderer),
          metadata_(metadata),
          bindings_(bindings),
          propertyChangedHandler_(
              this, &TemplateManager::OnPropertyChanged) {}
    ~TemplateManager() noexcept;

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
    struct Instance final {
        TemplateHandle handle;
        Control* parent = nullptr;
        const ControlTemplate* plan = nullptr;
        Visual* rootVisual = nullptr;
        UIElement* rootElement = nullptr;
        Base::Vector<Aero::Controls::Detail::TemplatePart> parts;
        Base::Vector<Aero::Controls::Detail::TemplateContentProjection> projections;
        NameScope names;
        Base::Vector<Data::BindingHandle>
            metadataBindings;
    };

    GuiContext* tree_ = nullptr;
    Core::Detail::TemplatedParentProviderSession providerSession_;
    Core::Detail::TemplatedParentProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    LayoutManager* layout_ = nullptr;
    RenderTree* renderer_ = nullptr;
    Core::MetadataRuntime* metadata_ = nullptr;
    Aero::Detail::BindingManager* bindings_ = nullptr;
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
class AERO_API ControlRuntimeAccess::MenuInteractionManager final {
public:
    MenuInteractionManager(
        GuiContext& tree,
        EventRouter& events,
        InputService& input) noexcept;
    ~MenuInteractionManager() noexcept;

    Base::Result<void> Attach(
        Menu& menu) noexcept;
    Base::Result<bool> Detach(
        Menu& menu) noexcept;

private:
    GuiContext* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    InputService* input_ = nullptr;
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

} // namespace Aero::Detail
