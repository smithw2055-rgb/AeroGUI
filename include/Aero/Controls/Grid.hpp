#pragma once

#include <Aero/Controls/Panel.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Input.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_API ColumnDefinition : public Base::Object {
    AERO_DECLARE_TYPE(ColumnDefinition, Base::Object)
public:
    ColumnDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength GetWidth() const noexcept { return width_; }
    double GetMaxWidth() const noexcept { return maxWidth_; }
    Base::StringView GetSharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    void SetWidth(GridLength value) noexcept;
    void SetMaxWidth(double value) noexcept;
    void SetSharedSizeGroup(
        Base::StringView value) noexcept;
private:
    GridLength width_ = GridLength::Star();
    double maxWidth_ = 1.0e12;
    Base::String sharedSizeGroup_;
};

class AERO_API RowDefinition : public Base::Object {
    AERO_DECLARE_TYPE(RowDefinition, Base::Object)
public:
    RowDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength GetHeight() const noexcept { return height_; }
    double GetMaxHeight() const noexcept { return maxHeight_; }
    Base::StringView GetSharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    void SetHeight(GridLength value) noexcept;
    void SetMaxHeight(double value) noexcept;
    void SetSharedSizeGroup(
        Base::StringView value) noexcept;
private:
    GridLength height_ = GridLength::Star();
    double maxHeight_ = 1.0e12;
    Base::String sharedSizeGroup_;
};

class AERO_API Grid : public Panel {
    AERO_DECLARE_TYPE(Grid, Panel)
public:
    Grid() noexcept;
    void SetColumnDefinitions(Base::Span<const GridLength> definitions) noexcept;
    void SetRowDefinitions(Base::Span<const GridLength> definitions) noexcept;
    void SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column) noexcept;
    void SetChildCell(
        UIElement& child,
        std::uint32_t row,
        std::uint32_t column,
        std::uint32_t rowSpan,
        std::uint32_t columnSpan) noexcept;
    Base::Result<void> AddColumnDefinition(
        Base::Ref<ColumnDefinition> definition) noexcept;
    Base::Result<void> AddRowDefinition(
        Base::Ref<RowDefinition> definition) noexcept;
    void ClearColumnDefinitionObjects() noexcept;
    void ClearRowDefinitionObjects() noexcept;
    Base::Result<void> AddInputBinding(
        Base::Ref<Aero::Input::KeyBinding> binding) noexcept;
    void ClearInputBindings() noexcept { inputBindings_.Clear(); }
    Base::Span<const Base::Ref<Aero::Input::KeyBinding>>
    GetInputBindings() const noexcept {
        return {inputBindings_.Data(), inputBindings_.Size()};
    }
    Base::StringView GetColumnDefinitionsText() const noexcept;
    Base::StringView GetRowDefinitionsText() const noexcept;
    void SetColumnDefinitionsText(
        Base::StringView value) noexcept;
    void SetRowDefinitionsText(
        Base::StringView value) noexcept;
    Base::Span<const GridLength> GetColumnDefinitions() const noexcept { return {columns_.Data(), columns_.Size()}; }
    Base::Span<const GridLength> GetRowDefinitions() const noexcept { return {rows_.Data(), rows_.Size()}; }
    inline static constexpr AttachedProperty<std::uint32_t> RowProperty{"Row"};
    inline static constexpr AttachedProperty<std::uint32_t> ColumnProperty{"Column"};
    inline static constexpr AttachedProperty<std::uint32_t> RowSpanProperty{"RowSpan"};
    inline static constexpr AttachedProperty<std::uint32_t> ColumnSpanProperty{"ColumnSpan"};
    inline static constexpr AttachedProperty<bool> IsSharedSizeScopeProperty{"IsSharedSizeScope"};
    // Programmatic compact form; WPF XAML uses the structural
    // ColumnDefinitions and RowDefinitions collections.
    inline static constexpr DependencyProperty<Base::String> ColumnDefinitionsTextProperty{"ColumnDefinitionsText"};
    inline static constexpr DependencyProperty<Base::String> RowDefinitionsTextProperty{"RowDefinitionsText"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    Base::Vector<GridLength> columns_;
    Base::Vector<GridLength> rows_;
    Base::Vector<Base::Ref<ColumnDefinition>>
        columnDefinitionObjects_;
    Base::Vector<Base::Ref<RowDefinition>>
        rowDefinitionObjects_;
    Base::Vector<Base::Ref<Aero::Input::KeyBinding>>
        inputBindings_;
    Base::Vector<double> desiredColumns_;
    Base::Vector<double> desiredRows_;
    std::uint32_t GetColumnCount() const noexcept;
    std::uint32_t GetRowCount() const noexcept;
    GridLength ColumnAt(std::uint32_t index) const noexcept;
    GridLength RowAt(std::uint32_t index) const noexcept;
    Base::Result<void> ValidateDefinitions(Base::Span<const GridLength> definitions) const noexcept;
    std::uint32_t GetChildRow(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumn(const UIElement& child) const noexcept;
    std::uint32_t GetChildRowSpan(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumnSpan(const UIElement& child) const noexcept;
    Base::Result<void> ResolveTracks(Base::Span<const GridLength> definitions,
        Base::Span<const double> desired, double available,
        Base::Vector<double>& resolved) const noexcept;
};

} // namespace Aero::Controls
