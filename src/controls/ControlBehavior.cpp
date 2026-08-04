#include "ControlBehavior.hpp"


#include <Aero/Controls/Core.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Primitives.hpp>
#include <Aero/Controls/Common.hpp>
#include <Aero/Controls/Text.hpp>

namespace Aero::Controls::Detail {

ControlBehavior::ControlBehavior(
    Base::IAllocator& allocator,
    ::Aero::Meta::Registry& metadata,
    Aero::ElementTree& tree,
    Aero::GuiPrivate::Detail::EventRouter& events,
    Aero::GuiPrivate::Detail::InputRouter& input,
    VisualStateManager* visualStates,
    Integration::IClipboard* clipboard,
    bool controlsEnabled,
    bool textEditingEnabled) noexcept
    : allocator_(&allocator),
      metadata_(&metadata),
      tree_(&tree),
      events_(&events),
      input_(&input),
      visualStates_(visualStates),
      clipboard_(clipboard),
      controlsEnabled_(controlsEnabled),
      textEditingEnabled_(textEditingEnabled) {}

ControlBehavior::~ControlBehavior() noexcept {
    Shutdown();
}

template<class T, class... TArgs>
Base::Result<T*> ControlBehavior::Construct(TArgs&&... arguments) noexcept {
    static_assert(alignof(T) <= alignof(std::max_align_t),
        "Control behavior requires unsupported over-alignment");
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(storage_);
    const std::uintptr_t current = base + offset_;
    const std::uintptr_t aligned =
        (current + alignof(T) - 1U) & ~(alignof(T) - 1U);
    const std::size_t next =
        static_cast<std::size_t>(aligned - base) + sizeof(T);
    if (next > StorageBytes) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Control behavior storage is exhausted");
    }
    offset_ = next;
    return new (reinterpret_cast<void*>(aligned))
        T(std::forward<TArgs>(arguments)...);
}

template<class T>
void ControlBehavior::Destroy(T*& object) noexcept {
    if (object == nullptr) return;
    object->~T();
    object = nullptr;
}

Base::Result<void> ControlBehavior::Initialize() noexcept {
    if (initialized_) return {};
    if (metadata_ == nullptr || tree_ == nullptr || events_ == nullptr ||
        input_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Control behavior has no View state");
    }

    if (controlsEnabled_) {
        Base::Result<Aero::Controls::Detail::ButtonBehavior*> buttons =
            Construct<Aero::Controls::Detail::ButtonBehavior>(
                *tree_, *events_, *input_, visualStates_);
        if (!buttons) return buttons.GetStatus();
        buttons_ = buttons.Value();
        Base::Result<void> status = buttons_->Initialize();
        if (!status) return status.GetStatus();

        Base::Result<Aero::Controls::Detail::ScrollBehavior*> scrolling =
            Construct<Aero::Controls::Detail::ScrollBehavior>(*tree_, *events_);
        if (!scrolling) return scrolling.GetStatus();
        scrolling_ = scrolling.Value();

        Base::Result<Aero::Controls::Detail::SliderBehavior*> sliders =
            Construct<Aero::Controls::Detail::SliderBehavior>(
                *tree_, *events_, *input_);
        if (!sliders) return sliders.GetStatus();
        sliders_ = sliders.Value();

        Base::Result<Aero::Controls::Detail::ListBehavior*> lists =
            Construct<Aero::Controls::Detail::ListBehavior>(
                *tree_, *events_, *input_, visualStates_);
        if (!lists) return lists.GetStatus();
        lists_ = lists.Value();

        Base::Result<Aero::Controls::Detail::ComboBehavior*> combos =
            Construct<Aero::Controls::Detail::ComboBehavior>(
                *tree_, *events_, *input_);
        if (!combos) return combos.GetStatus();
        combos_ = combos.Value();

        Base::Result<Aero::Controls::Detail::TreeBehavior*> trees =
            Construct<Aero::Controls::Detail::TreeBehavior>(
                *tree_, *events_, *input_, visualStates_);
        if (!trees) return trees.GetStatus();
        trees_ = trees.Value();

        Base::Result<Aero::Controls::Detail::MenuBehavior*> menus =
            Construct<Aero::Controls::Detail::MenuBehavior>(
                *tree_, *events_, *input_);
        if (!menus) return menus.GetStatus();
        menus_ = menus.Value();
    }

    if (textEditingEnabled_ && clipboard_ != nullptr) {
        Base::Result<Aero::Controls::Detail::TextEditBehavior*> text =
            Construct<Aero::Controls::Detail::TextEditBehavior>(
                *tree_, *events_, *input_, *clipboard_);
        if (!text) return text.GetStatus();
        textBoxes_ = text.Value();
    }

    initialized_ = true;
    return {};
}

Base::Result<void> ControlBehavior::Attach(
    Visual& visual,
    Integration::ITextInputMethodHost* inputMethodHost) noexcept {
    if (!initialized_ || metadata_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Control behavior is not initialized");
    }
    const Meta::TypeId type = visual.RuntimeType();
    auto& types = metadata_->Types();
    if (types.IsDerivedFrom(type, Control::StaticTypeId())) {
        SetVisualStateManager(
            *static_cast<Control*>(&visual), visualStates_);
    }
    if (buttons_ != nullptr &&
        types.IsDerivedFrom(type, Primitives::ButtonBase::StaticTypeId())) {
        Base::Result<void> result = buttons_->Attach(
            *static_cast<Primitives::ButtonBase*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (types.IsDerivedFrom(type, TextBox::StaticTypeId())) {
        auto& textBox = *static_cast<TextBox*>(&visual);
        if (inputMethodHost != nullptr) {
            textBox.SetInputMethodHost(inputMethodHost);
        }
        if (textBoxes_ != nullptr) {
            Base::Result<void> result = textBoxes_->Attach(textBox);
            if (!result) return result.GetStatus();
        }
    }
    if (types.IsDerivedFrom(type, PasswordBox::StaticTypeId())) {
        auto& passwordBox = *static_cast<PasswordBox*>(&visual);
        if (inputMethodHost != nullptr) {
            passwordBox.SetInputMethodHost(inputMethodHost);
        }
        if (textBoxes_ != nullptr) {
            Base::Result<void> result = textBoxes_->Attach(passwordBox);
            if (!result) return result.GetStatus();
        }
    }
    if (scrolling_ != nullptr &&
        types.IsDerivedFrom(type, ScrollViewer::StaticTypeId())) {
        Base::Result<void> result = scrolling_->Attach(
            *static_cast<ScrollViewer*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (sliders_ != nullptr &&
        types.IsDerivedFrom(type, Slider::StaticTypeId())) {
        Base::Result<void> result = sliders_->Attach(
            *static_cast<Slider*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (lists_ != nullptr &&
        types.IsDerivedFrom(type, ListBox::StaticTypeId())) {
        Base::Result<void> result = lists_->Attach(
            *static_cast<ListBox*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (combos_ != nullptr &&
        types.IsDerivedFrom(type, ComboBox::StaticTypeId())) {
        Base::Result<void> result = combos_->Attach(
            *static_cast<ComboBox*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (trees_ != nullptr &&
        types.IsDerivedFrom(type, TreeView::StaticTypeId())) {
        Base::Result<void> result = trees_->Attach(
            *static_cast<TreeView*>(&visual));
        if (!result) return result.GetStatus();
    }
    if (menus_ != nullptr &&
        types.IsDerivedFrom(type, Menu::StaticTypeId())) {
        Base::Result<void> result = menus_->Attach(
            *static_cast<Menu*>(&visual));
        if (!result) return result.GetStatus();
    }
    return {};
}

Base::Result<bool> ControlBehavior::Detach(
    Visual& visual) noexcept {
    if (metadata_ == nullptr) return false;
    const Meta::TypeId type = visual.RuntimeType();
    auto& types = metadata_->Types();
    Base::Result<bool> detached = false;
    if (buttons_ != nullptr &&
        types.IsDerivedFrom(
            type, Primitives::ButtonBase::StaticTypeId())) {
        detached = buttons_->Detach(
            *static_cast<Primitives::ButtonBase*>(&visual));
    } else if (textBoxes_ != nullptr &&
               types.IsDerivedFrom(type, TextBox::StaticTypeId())) {
        detached = textBoxes_->Detach(
            *static_cast<TextBox*>(&visual));
    } else if (textBoxes_ != nullptr &&
               types.IsDerivedFrom(type, PasswordBox::StaticTypeId())) {
        detached = textBoxes_->Detach(
            *static_cast<PasswordBox*>(&visual));
    } else if (scrolling_ != nullptr &&
               types.IsDerivedFrom(type, ScrollViewer::StaticTypeId())) {
        detached = scrolling_->Detach(
            *static_cast<ScrollViewer*>(&visual));
    } else if (sliders_ != nullptr &&
               types.IsDerivedFrom(type, Slider::StaticTypeId())) {
        detached = sliders_->Detach(
            *static_cast<Slider*>(&visual));
    } else if (lists_ != nullptr &&
               types.IsDerivedFrom(type, ListBox::StaticTypeId())) {
        detached = lists_->Detach(
            *static_cast<ListBox*>(&visual));
    } else if (combos_ != nullptr &&
               types.IsDerivedFrom(type, ComboBox::StaticTypeId())) {
        detached = combos_->Detach(
            *static_cast<ComboBox*>(&visual));
    } else if (trees_ != nullptr &&
               types.IsDerivedFrom(type, TreeView::StaticTypeId())) {
        detached = trees_->Detach(
            *static_cast<TreeView*>(&visual));
    } else if (menus_ != nullptr &&
               types.IsDerivedFrom(type, Menu::StaticTypeId())) {
        detached = menus_->Detach(
            *static_cast<Menu*>(&visual));
    }
    if (!detached) return detached.GetStatus();
    if (types.IsDerivedFrom(type, Control::StaticTypeId())) {
        SetVisualStateManager(
            *static_cast<Control*>(&visual), nullptr);
    }
    return detached.Value();
}

Base::Result<void> ControlBehavior::RefreshButtonVisualState(
    Primitives::ButtonBase& button,
    bool useTransitions) noexcept {
    if (buttons_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Button behavior is unavailable");
    }
    return buttons_->RefreshVisualState(
        button, useTransitions);
}

Base::Result<std::uint32_t> ControlBehavior::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    return buttons_ != nullptr
        ? buttons_->AdvanceTime(elapsedMilliseconds)
        : Base::Result<std::uint32_t>(std::uint32_t{0U});
}

void ControlBehavior::Shutdown() noexcept {
    if (!initialized_ && offset_ == 0U) return;
    Destroy(menus_);
    Destroy(trees_);
    Destroy(combos_);
    Destroy(lists_);
    Destroy(sliders_);
    Destroy(scrolling_);
    Destroy(textBoxes_);
    Destroy(buttons_);
    offset_ = 0U;
    initialized_ = false;
}

} // namespace Aero::Controls::Detail
