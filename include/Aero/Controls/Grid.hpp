#pragma once

#include <Aero/Controls/GridLength.hpp>
#include <Aero/Controls/Panels.hpp>
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
    ~Grid() override;

    static Base::Result<GridLength> ConvertLength(Base::StringView text) noexcept;
    static bool ParseDefinitions(
        Base::StringView text, Base::Vector<GridLength>& output) noexcept;
    void AddColumnDefinition(Ref<ColumnDefinition> definition) noexcept;
    void AddRowDefinition(Ref<RowDefinition> definition) noexcept;
    void AddInputBinding(Ref<Aero::Input::InputBinding> binding) noexcept {
        UIElement::AddInputBinding(std::move(binding));
    }
    void ClearInputBindings() noexcept { UIElement::ClearInputBindings(); }

    Span<const GridLength> GetColumnDefinitions() const noexcept;
    void SetColumnDefinitions(Span<const GridLength> definitions) noexcept;
    Span<const GridLength> GetRowDefinitions() const noexcept;
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
    float MaxWidthAt(std::uint32_t index) const noexcept;
    float MaxHeightAt(std::uint32_t index) const noexcept;
    bool ValidateDefinitions(Span<const GridLength> definitions) const noexcept;
    bool ResolveTracks(Span<const GridLength> definitions, Span<const float> desired, float available,
        Base::BasicVector<float, 16U>& resolved, Span<const float> maxLimits = {}) const noexcept;

    std::uint32_t GetColumnCount() const noexcept;
    std::uint32_t GetRowCount() const noexcept;
    std::uint32_t GetChildRow(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumn(const UIElement& child) const noexcept;
    std::uint32_t GetChildRowSpan(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumnSpan(const UIElement& child) const noexcept;

    void SyncDefinitionVectors() noexcept;

    struct SpanningChild {
        UIElement* child = nullptr;
        std::uint32_t column = 0U;
        std::uint32_t row = 0U;
        std::uint32_t columnSpan = 1U;
        std::uint32_t rowSpan = 1U;
    };
    struct NonSpanningChild {
        UIElement* child = nullptr;
        std::uint32_t column = 0U;
        std::uint32_t row = 0U;
        bool hasStarCol = false;
        bool hasStarRow = false;
    };

    struct TrackStorage;
    TrackStorage* EnsureTracks() noexcept;
    void DropTracksIfEmpty() noexcept;

    TrackStorage* tracks_ = nullptr;
};

} // namespace Aero::Controls
