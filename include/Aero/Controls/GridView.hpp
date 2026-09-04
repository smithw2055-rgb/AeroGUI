#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/GridViewColumn.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API GridView
    : public Base::Object {
    AERO_DECLARE_TYPE(GridView, Base::Object)
public:
    GridView() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Span<const Ref<GridViewColumn>>
        GetColumns() const noexcept {
        return {
            columns_.Data(),
            columns_.Size()};
    }
    Result<void> AddColumn(
        Ref<GridViewColumn> column)
        noexcept;
    void ClearColumns() noexcept {
        columns_.Clear();
    }
    bool GetAllowsColumnReorder() const noexcept {
        return allowsColumnReorder_;
    }
    void SetAllowsColumnReorder(bool value) noexcept {
        allowsColumnReorder_ = value;
    }
    Ref<Style> GetColumnHeaderContainerStyle() const noexcept {
        return columnHeaderContainerStyle_;
    }
    void SetColumnHeaderContainerStyle(
        Ref<Style> value) noexcept {
        columnHeaderContainerStyle_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderContextMenu() const noexcept {
        return columnHeaderContextMenu_;
    }
    void SetColumnHeaderContextMenu(Ref<Base::Object> value) noexcept {
        columnHeaderContextMenu_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderTemplate() const noexcept {
        return columnHeaderTemplate_;
    }
    void SetColumnHeaderTemplate(Ref<Base::Object> value) noexcept {
        columnHeaderTemplate_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderTemplateSelector() const noexcept {
        return columnHeaderTemplateSelector_;
    }
    void SetColumnHeaderTemplateSelector(Ref<Base::Object> value) noexcept {
        columnHeaderTemplateSelector_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderToolTip() const noexcept {
        return columnHeaderToolTip_;
    }
    void SetColumnHeaderToolTip(Ref<Base::Object> value) noexcept {
        columnHeaderToolTip_ = std::move(value);
    }
    Ref<Base::Object> GetColumnsObject() const noexcept {
        return Ref<Base::Object>::TryFromBorrowed(
            *const_cast<GridView*>(this));
    }
    void SetColumnsObject(Ref<Base::Object>) noexcept {}

private:
    Base::Vector<Ref<GridViewColumn>>
        columns_;
    bool allowsColumnReorder_ = false;
    Ref<Style> columnHeaderContainerStyle_;
    Ref<Base::Object> columnHeaderContextMenu_;
    Ref<Base::Object> columnHeaderTemplate_;
    Ref<Base::Object> columnHeaderTemplateSelector_;
    Ref<Base::Object> columnHeaderToolTip_;
};
} // namespace Aero::Controls
