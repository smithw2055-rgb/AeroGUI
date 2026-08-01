#include "ControlBehaviorService.hpp"

#include <Aero/Controls/Base.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Controls/Standard.hpp>
#include <Aero/Controls/Text.hpp>
#include <Aero/Documents.hpp>
#include "RuntimeManagers.hpp"
#include "ControlInternals.hpp"
#include "gui/InputInternal.hpp"
#include <Aero/Meta/MetadataDomain.hpp>
#include <Aero/Integration/PlatformServices.hpp>

#include <new>
#include <utility>

namespace Aero::Controls::Detail {
namespace {

template<class T, class... Args>
Base::Result<T*> Create(Base::IAllocator& allocator, Args&&... args) noexcept {
    void* memory = allocator.Allocate({sizeof(T), alignof(T), Base::MemoryTag::Ui});
    if (memory == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::OutOfMemory,
            "Control behavior allocation failed");
    }
    return new (memory) T(std::forward<Args>(args)...);
}

template<class T>
void Destroy(Base::IAllocator& allocator, T*& object) noexcept {
    if (object == nullptr) return;
    object->~T();
    allocator.Deallocate(object, sizeof(T), alignof(T), Base::MemoryTag::Ui);
    object = nullptr;
}

} // namespace

struct ControlBehaviorService::Impl final {
    Core::MetadataDomain* metadata = nullptr;
    Aero::GuiContext* tree = nullptr;
    Aero::Detail::EventRouter* events = nullptr;
    Aero::Detail::InputService* input = nullptr;
    VisualStateManager* visualStates = nullptr;
    Integration::IClipboard* clipboard = nullptr;
    bool controlsEnabled = false;
    bool textEditingEnabled = false;

    ControlInteractionManager* buttons = nullptr;
    TextBoxInteractionManager* textBoxes = nullptr;
    ScrollInteractionManager* scrolling = nullptr;
    SliderInteractionManager* sliders = nullptr;
    ListBoxInteractionManager* lists = nullptr;
    ComboBoxInteractionManager* combos = nullptr;
    TreeViewInteractionManager* trees = nullptr;
    MenuInteractionManager* menus = nullptr;
};

ControlBehaviorService::ControlBehaviorService(Base::IAllocator& allocator,
    Core::MetadataDomain& metadata, Aero::GuiContext& tree,
    Aero::Detail::EventRouter& events, Aero::Detail::InputService& input,
    VisualStateManager* visualStates, Integration::IClipboard* clipboard,
    bool controlsEnabled, bool textEditingEnabled) noexcept
    : allocator_(&allocator) {
    void* memory = allocator.Allocate({sizeof(Impl), alignof(Impl), Base::MemoryTag::Ui});
    if (memory == nullptr) return;
    impl_ = new (memory) Impl();
    impl_->metadata = &metadata;
    impl_->tree = &tree;
    impl_->events = &events;
    impl_->input = &input;
    impl_->visualStates = visualStates;
    impl_->clipboard = clipboard;
    impl_->controlsEnabled = controlsEnabled;
    impl_->textEditingEnabled = textEditingEnabled;
}

ControlBehaviorService::~ControlBehaviorService() noexcept {
    Shutdown();
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Ui);
        impl_ = nullptr;
    }
}

Base::Result<void> ControlBehaviorService::Initialize() noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::OutOfMemory,
            "Control behavior state allocation failed");
    }
    Base::Result<void> status;
    if (impl_->controlsEnabled) {
        auto buttons = Create<ControlInteractionManager>(*allocator_, *impl_->tree,
            *impl_->events, *impl_->input, impl_->visualStates);
        if (!buttons) return buttons.GetStatus();
        impl_->buttons = buttons.Value();
        status = impl_->buttons->Initialize();
        if (!status) return status.GetStatus();

        auto scrolling = Create<ScrollInteractionManager>(*allocator_, *impl_->tree, *impl_->events);
        if (!scrolling) return scrolling.GetStatus();
        impl_->scrolling = scrolling.Value();
        auto sliders = Create<SliderInteractionManager>(*allocator_, *impl_->tree, *impl_->events, *impl_->input);
        if (!sliders) return sliders.GetStatus();
        impl_->sliders = sliders.Value();
        auto lists = Create<ListBoxInteractionManager>(*allocator_, *impl_->tree, *impl_->events,
            *impl_->input, impl_->visualStates);
        if (!lists) return lists.GetStatus();
        impl_->lists = lists.Value();
        auto combos = Create<ComboBoxInteractionManager>(*allocator_, *impl_->tree, *impl_->events, *impl_->input);
        if (!combos) return combos.GetStatus();
        impl_->combos = combos.Value();
        auto trees = Create<TreeViewInteractionManager>(*allocator_, *impl_->tree, *impl_->events,
            *impl_->input, impl_->visualStates);
        if (!trees) return trees.GetStatus();
        impl_->trees = trees.Value();
        auto menus = Create<MenuInteractionManager>(*allocator_, *impl_->tree, *impl_->events, *impl_->input);
        if (!menus) return menus.GetStatus();
        impl_->menus = menus.Value();
    }
    if (impl_->textEditingEnabled && impl_->clipboard != nullptr) {
        auto textBoxes = Create<TextBoxInteractionManager>(*allocator_, *impl_->tree,
            *impl_->events, *impl_->input, *impl_->clipboard);
        if (!textBoxes) return textBoxes.GetStatus();
        impl_->textBoxes = textBoxes.Value();
    }
    return {};
}

Base::Result<void> ControlBehaviorService::Attach(Visual& visual,
    Integration::ITextInputMethodHost* inputMethodHost) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "Control behavior service is not initialized");
    }
    const Core::TypeId type = visual.RuntimeType();
    auto& types = impl_->metadata->Types();
    if (types.IsDerivedFrom(type, Control::StaticTypeId())) {
        Aero::Detail::ControlRuntimeAccess::SetVisualStateManager(
            *static_cast<Control*>(&visual), impl_->visualStates);
    }
    if (impl_->buttons != nullptr && types.IsDerivedFrom(type, Primitives::ButtonBase::StaticTypeId())) {
        Base::Result<void> result = impl_->buttons->Attach(*static_cast<Primitives::ButtonBase*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (types.IsDerivedFrom(type, TextBox::StaticTypeId())) {
        auto& textBox = *static_cast<TextBox*>(&visual);
        if (inputMethodHost != nullptr) {
            Base::Result<void> result = textBox.SetInputMethodHost(inputMethodHost);
            if (!result) return result.GetStatus();
        }
        if (impl_->textBoxes != nullptr) {
            Base::Result<void> result = impl_->textBoxes->Attach(textBox);
            if (!result) return result.GetStatus();
        }
    }
    if (types.IsDerivedFrom(type, PasswordBox::StaticTypeId())) {
        auto& passwordBox = *static_cast<PasswordBox*>(&visual);
        if (inputMethodHost != nullptr) {
            Base::Result<void> result = passwordBox.SetInputMethodHost(inputMethodHost);
            if (!result) return result.GetStatus();
        }
        if (impl_->textBoxes != nullptr) {
            Base::Result<void> result = impl_->textBoxes->Attach(passwordBox);
            if (!result) return result.GetStatus();
        }
    }
    if (impl_->scrolling != nullptr && types.IsDerivedFrom(type, ScrollViewer::StaticTypeId())) {
        Base::Result<void> result = impl_->scrolling->Attach(*static_cast<ScrollViewer*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (impl_->sliders != nullptr && types.IsDerivedFrom(type, Slider::StaticTypeId())) {
        Base::Result<void> result = impl_->sliders->Attach(*static_cast<Slider*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (impl_->lists != nullptr && types.IsDerivedFrom(type, ListBox::StaticTypeId())) {
        Base::Result<void> result = impl_->lists->Attach(*static_cast<ListBox*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (impl_->combos != nullptr && types.IsDerivedFrom(type, ComboBox::StaticTypeId())) {
        Base::Result<void> result = impl_->combos->Attach(*static_cast<ComboBox*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (impl_->trees != nullptr && types.IsDerivedFrom(type, TreeView::StaticTypeId())) {
        Base::Result<void> result = impl_->trees->Attach(*static_cast<TreeView*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (impl_->menus != nullptr && types.IsDerivedFrom(type, Menu::StaticTypeId())) {
        Base::Result<void> result = impl_->menus->Attach(*static_cast<Menu*>(&visual));
        if (!result) return result.GetStatus();
    }
    return {};
}

Base::Result<std::uint32_t> ControlBehaviorService::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    return impl_ != nullptr && impl_->buttons != nullptr
        ? impl_->buttons->AdvanceTime(elapsedMilliseconds)
        : Base::Result<std::uint32_t>(std::uint32_t{0U});
}

void ControlBehaviorService::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    Destroy(*allocator_, impl_->menus);
    Destroy(*allocator_, impl_->trees);
    Destroy(*allocator_, impl_->combos);
    Destroy(*allocator_, impl_->lists);
    Destroy(*allocator_, impl_->sliders);
    Destroy(*allocator_, impl_->scrolling);
    Destroy(*allocator_, impl_->textBoxes);
    Destroy(*allocator_, impl_->buttons);
}

} // namespace Aero::Controls::Detail
