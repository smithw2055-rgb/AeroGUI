#pragma once

#include <Aero/Controls/ColumnDefinition.hpp>
#include <Aero/Controls/RowDefinition.hpp>
#include <Aero/Controls/GridLength.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Input.hpp>
#include <Aero/InputBinding.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API Grid : public Panel {
    AERO_DECLARE_TYPE(Grid, Panel)
    #if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
    #endif
public:
    Grid() noexcept;
    void SetColumnDefinitions(Span<const GridLength> definitions) noexcept;
    void SetRowDefinitions(Span<const GridLength> definitions) noexcept;
    void SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column) noexcept;
    void SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column, std::uint32_t rowSpan, std::uint32_t columnSpan) noexcept;
    void AddColumnDefinition(Ref<ColumnDefinition> definition) noexcept;
    void AddRowDefinition(Ref<RowDefinition> definition) noexcept;
    void ClearColumnDefinitionObjects() noexcept;
    void ClearRowDefinitionObjects() noexcept;
    void AddInputBinding(
        Ref<Aero::Input::InputBinding> binding) noexcept {
        UIElement::AddInputBinding(std::move(binding));
    }
    void ClearInputBindings() noexcept {
        UIElement::ClearInputBindings();
    }
    Span<const Ref<Aero::Input::InputBinding>>
    GetInputBindings() const noexcept {
        return UIElement::GetInputBindings();
    }
    StringView GetColumnDefinitionsText() const noexcept;
    StringView GetRowDefinitionsText() const noexcept;
    void SetColumnDefinitionsText(StringView value) noexcept;
    void SetRowDefinitionsText(StringView value) noexcept;
    Span<const GridLength> GetColumnDefinitions() const noexcept { 
        return {columns_.Data(), columns_.Size()}; 
    }
    Span<const GridLength> GetRowDefinitions() const noexcept { 
        return {rows_.Data(), rows_.Size()}; 
    }
    AERO_ATTACHED_PROPERTY(std::uint32_t, Row);
    AERO_ATTACHED_PROPERTY(std::uint32_t, Column);
    AERO_ATTACHED_PROPERTY(std::uint32_t, RowSpan);
    AERO_ATTACHED_PROPERTY(std::uint32_t, ColumnSpan);
    AERO_ATTACHED_PROPERTY(bool, IsSharedSizeScope);
    // Programmatic compact form; WPF XAML uses the structural
    // ColumnDefinitions and RowDefinitions collections.
    AERO_DEPENDENCY_PROPERTY(String, ColumnDefinitionsText);
    AERO_DEPENDENCY_PROPERTY(String, RowDefinitionsText);
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    Base::Vector<GridLength> columns_;
    Base::Vector<GridLength> rows_;
    Base::Vector<Ref<ColumnDefinition>>
        columnDefinitionObjects_;
    Base::Vector<Ref<RowDefinition>>
        rowDefinitionObjects_;
    Base::Vector<double> desiredColumns_;
    Base::Vector<double> desiredRows_;
    std::uint32_t GetColumnCount() const noexcept;
    std::uint32_t GetRowCount() const noexcept;
    GridLength ColumnAt(std::uint32_t index) const noexcept;
    GridLength RowAt(std::uint32_t index) const noexcept;
    Result<void> ValidateDefinitions(Span<const GridLength> definitions) const noexcept;
    std::uint32_t GetChildRow(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumn(const UIElement& child) const noexcept;
    std::uint32_t GetChildRowSpan(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumnSpan(const UIElement& child) const noexcept;
    Result<void> ResolveTracks(Span<const GridLength> definitions,
        Span<const double> desired, double available,
        Base::Vector<double>& resolved) const noexcept;
};

} // namespace Aero::Controls
