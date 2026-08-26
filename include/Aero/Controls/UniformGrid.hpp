#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API UniformGrid : public Panel {
    AERO_DECLARE_TYPE(UniformGrid, Panel)
    #if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
    #endif
public:
    UniformGrid() noexcept : Panel(StaticTypeId()) {}
    std::uint32_t GetRows() const noexcept;
    std::uint32_t GetColumns() const noexcept;
    std::uint32_t GetFirstColumn() const noexcept;
    void SetRows(std::uint32_t value) noexcept;
    void SetColumns(std::uint32_t value) noexcept;
    void SetFirstColumn(
        std::uint32_t value) noexcept;
    inline static constexpr DependencyProperty<std::uint32_t> RowsProperty{"Rows"};
    inline static constexpr DependencyProperty<std::uint32_t> ColumnsProperty{"Columns"};
    inline static constexpr DependencyProperty<std::uint32_t> FirstColumnProperty{"FirstColumn"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    void ResolveDimensions(
        std::uint32_t childCount,
        std::uint32_t& rows,
        std::uint32_t& columns) const noexcept;
};

} // namespace Aero::Controls
