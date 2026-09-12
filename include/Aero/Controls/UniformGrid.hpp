#pragma once

#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API UniformGrid : public Panel {
    AERO_DECLARE_TYPE(UniformGrid, Panel)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    UniformGrid() noexcept : Panel(StaticTypeId()) {}
    std::uint32_t GetRows() const noexcept;
    std::uint32_t GetColumns() const noexcept;
    std::uint32_t GetFirstColumn() const noexcept;
    void SetRows(std::uint32_t value) noexcept;
    void SetColumns(std::uint32_t value) noexcept;
    void SetFirstColumn(std::uint32_t value) noexcept;
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Rows);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Columns);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, FirstColumn);
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
