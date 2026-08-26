#pragma once

#include <Aero/Controls/GridViewColumnHeader.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Style.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API GridViewColumn
    : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    GridViewColumn() noexcept
        : DependencyObject(StaticTypeId()) {}
    Value GetHeader() const noexcept;
    void SetHeader(Value value) noexcept;
    Result<void> SetHeader(StringView value) noexcept;
    double GetWidth() const noexcept;
    void SetWidth(
        double value) noexcept;
    Ref<DataTemplate>
        GetCellTemplate() const noexcept;
    void SetCellTemplate(
        Ref<DataTemplate> value) noexcept;
    Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Ref<DataTemplate> value) noexcept;
    StringView GetDisplayMemberPath()
        const noexcept;
    void SetDisplayMemberPath(
        StringView value) noexcept;
    Ref<Aero::Data::Binding>
        GetDisplayMemberBinding() const noexcept;
    void SetDisplayMemberBinding(
        Ref<Aero::Data::Binding> value) noexcept;
    Ref<Style> GetHeaderContainerStyle() const noexcept {
        return GetValueOr(
            HeaderContainerStyleProperty,
            Ref<Style>{});
    }
    void SetHeaderContainerStyle(
        Ref<Style> value) noexcept {
        SetValue(HeaderContainerStyleProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<double> WidthProperty{"Width"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> CellTemplateProperty{"CellTemplate"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr DependencyProperty<String> DisplayMemberPathProperty{"DisplayMemberPath"};
    inline static constexpr DependencyProperty<Ref<Aero::Data::Binding>> DisplayMemberBindingProperty{"DisplayMemberBinding"};
    inline static constexpr DependencyProperty<Ref<Style>> HeaderContainerStyleProperty{"HeaderContainerStyle"};
};
} // namespace Aero::Controls
