#pragma once

#include <Aero/Controls/GridLength.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Input.hpp>
#include <Aero/InputBinding.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API ColumnDefinition : public DependencyObject {
    AERO_DECLARE_TYPE(ColumnDefinition, DependencyObject)

public:
    ColumnDefinition() noexcept : DependencyObject(StaticTypeId()) {}

    GridLength GetWidth() const noexcept;
    void SetWidth(GridLength value) noexcept;
    double GetMaxWidth() const noexcept;
    void SetMaxWidth(double value) noexcept;
    StringView GetSharedSizeGroup() const noexcept;
    void SetSharedSizeGroup(StringView value) noexcept;

    AERO_DEPENDENCY_PROPERTY(GridLength, Width);
    AERO_DEPENDENCY_PROPERTY(double, MaxWidth);
    AERO_DEPENDENCY_PROPERTY(String, SharedSizeGroup);
};

class AERO_GUI_API RowDefinition : public DependencyObject {
    AERO_DECLARE_TYPE(RowDefinition, DependencyObject)

public:
    RowDefinition() noexcept : DependencyObject(StaticTypeId()) {}

    GridLength GetHeight() const noexcept;
    void SetHeight(GridLength value) noexcept;
    double GetMaxHeight() const noexcept;
    void SetMaxHeight(double value) noexcept;
    StringView GetSharedSizeGroup() const noexcept;
    void SetSharedSizeGroup(StringView value) noexcept;

    AERO_DEPENDENCY_PROPERTY(GridLength, Height);
    AERO_DEPENDENCY_PROPERTY(double, MaxHeight);
    AERO_DEPENDENCY_PROPERTY(String, SharedSizeGroup);
};

class AERO_GUI_API Grid : public Panel {
    AERO_DECLARE_TYPE(Grid, Panel)

public:
    Grid() noexcept;

    static Base::Result<GridLength> ConvertLength(Base::StringView text) noexcept;
    static Base::Result<void> ParseDefinitions(
        Base::StringView text, Base::Vector<GridLength>& output) noexcept;
    void AddColumnDefinition(Ref<ColumnDefinition> definition) noexcept;
    void AddRowDefinition(Ref<RowDefinition> definition) noexcept;
    void AddInputBinding(Ref<Aero::Input::InputBinding> binding) noexcept {
        UIElement::AddInputBinding(std::move(binding));
    }
    void ClearInputBindings() noexcept { UIElement::ClearInputBindings(); }

    Span<const GridLength> GetColumnDefinitions() const noexcept { return {columns_.Data(), columns_.Size()}; }
    void SetColumnDefinitions(Span<const GridLength> definitions) noexcept;
    Span<const GridLength> GetRowDefinitions() const noexcept { return {rows_.Data(), rows_.Size()}; }
    void SetRowDefinitions(Span<const GridLength> definitions) noexcept;
    void SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column) noexcept;
    void SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column, std::uint32_t rowSpan, std::uint32_t columnSpan) noexcept;
    void ClearColumnDefinitionObjects() noexcept;
    void ClearRowDefinitionObjects() noexcept;
    Span<const Ref<Aero::Input::InputBinding>> GetInputBindings() const noexcept {
        return UIElement::GetInputBindings();
    }
    StringView GetColumnDefinitionsText() const noexcept;
    void SetColumnDefinitionsText(StringView value) noexcept;
    StringView GetRowDefinitionsText() const noexcept;
    void SetRowDefinitionsText(StringView value) noexcept;

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
    bool ValidateValueCore(Meta::DependencyPropertyHandle property, const PropertyValue& value) const noexcept override;
    void OnPropertyChanged(const Meta::DependencyPropertyChangedEventArgs& args) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    GridLength ColumnAt(std::uint32_t index) const noexcept;
    GridLength RowAt(std::uint32_t index) const noexcept;
    Result<void> ValidateDefinitions(Span<const GridLength> definitions) const noexcept;
    Result<void> ResolveTracks(Span<const GridLength> definitions, Span<const double> desired, double available,
        Base::Vector<double>& resolved) const noexcept;

    std::uint32_t GetColumnCount() const noexcept;
    std::uint32_t GetRowCount() const noexcept;
    std::uint32_t GetChildRow(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumn(const UIElement& child) const noexcept;
    std::uint32_t GetChildRowSpan(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumnSpan(const UIElement& child) const noexcept;

    Base::Vector<GridLength> columns_;
    Base::Vector<GridLength> rows_;
    Base::Vector<Ref<ColumnDefinition>> columnDefinitionObjects_;
    Base::Vector<Ref<RowDefinition>> rowDefinitionObjects_;
    Base::Vector<double> desiredColumns_;
    Base::Vector<double> desiredRows_;
};

} // namespace Aero::Controls
