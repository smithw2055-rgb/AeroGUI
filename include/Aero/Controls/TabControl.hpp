#pragma once

#include <Aero/Controls/Primitives/Selector.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/TabItem.hpp>
#include <Aero/DataTemplate.hpp>

namespace Aero::Controls {

class AERO_GUI_API TabControl : public Primitives::Selector {
    AERO_DECLARE_TYPE(TabControl, Primitives::Selector)
public:
    TabControl() noexcept;
    ~TabControl() override;

    std::uint32_t GetTabCount() const noexcept {
        return GetCount();
    }
    TabItem* GetSelectedTab() const noexcept;
    Value GetSelectedContent() const noexcept {
        return GetValue(SelectedContentProperty);
    }
    Ref<DataTemplate> GetContentTemplate() const noexcept {
        return GetValue(ContentTemplateProperty);
    }
    void SetContentTemplate(Ref<DataTemplate> value) noexcept {
        SetValue(ContentTemplateProperty, std::move(value));
    }
    Dock GetTabStripPlacement() const noexcept {
        return GetValue(TabStripPlacementProperty);
    }
    void SetTabStripPlacement(Dock value) noexcept {
        SetValue(TabStripPlacementProperty, value);
    }

    AERO_READONLY_PROPERTY(Value, SelectedContent);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, ContentTemplate);
    AERO_DEPENDENCY_PROPERTY(Dock, TabStripPlacement);

protected:
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;
    void OnSelectionChanged(
        const SelectionChangedEvent& event) override;
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Result<void> SynchronizeSelection() noexcept;
};

} // namespace Aero::Controls
