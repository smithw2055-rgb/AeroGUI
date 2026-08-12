#pragma once

#include <Aero/Controls.hpp>

namespace Aero::Controls {

// Narrow property-change bridge for the cached views used by ItemsControl.
// DependencyProperty ownership remains canonical; these methods only mirror
// committed effective values into the hot runtime pointers.
struct ItemsControl::Access {
    static bool HasAttachedGenerator(
        const ItemsControl& control) noexcept {
        return control.generator_ != nullptr;
    }
    static void SetItemsSource(
        ItemsControl& control,
        Collections::IItemsSource* source) noexcept {
        control.SetItemsSourceCore(source);
    }
    static void SetItemsSourceBorrowed(
        ItemsControl& control,
        Collections::IItemsSource* source) noexcept {
        control.SetValue(
            ItemsControl::ItemsSourceProperty,
            Base::Ref<Base::Object>{});
        control.SetItemsSourceCore(source);
    }
    static void SetItemTemplate(
        ItemsControl& control,
        const DataTemplate* value) noexcept {
        control.SetItemTemplateCore(value);
    }
    static void SetItemsPanel(
        ItemsControl& control,
        const ItemsPanelTemplate* value) noexcept {
        control.SetItemsPanelCore(value);
    }
    static void SetItemContainerStyle(
        ItemsControl& control,
        const Style* value) noexcept {
        control.SetItemContainerStyleCore(value);
    }
    static void RefreshDisplayMemberPath(
        ItemsControl& control) noexcept {
        control.PublishReset();
    }
};

} // namespace Aero::Controls

namespace Aero::Controls {

// Internal adapter for scalar ItemsSource values. It is deliberately kept out
// of the public controls surface; callers use AddBoxedItem helpers instead.
class BoxedItemValue : public Base::Object {
    AERO_DECLARE_TYPE(BoxedItemValue, Base::Object)
public:
    explicit BoxedItemValue(Meta::Value value) noexcept
        : value_(std::move(value)) {}

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    const Meta::Value& Value() const noexcept {
        return value_;
    }

private:
    Meta::Value value_;
};

using ItemContainerGeneratorImpl =
    ::Aero::Controls::ItemContainerGenerator::Access;

} // namespace Aero::Controls
