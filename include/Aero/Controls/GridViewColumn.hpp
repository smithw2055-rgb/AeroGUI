#pragma once

#include <Aero/Controls/GridViewColumnHeader.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Style.hpp>

namespace Aero::Meta { class Registration; }

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API GridViewColumn
    : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    GridViewColumn() noexcept
        : DependencyObject(StaticTypeId()) {}
    Value GetHeader() const noexcept;
    void SetHeader(Value value) noexcept;
    void SetHeader(StringView value) noexcept;
    double GetWidth() const noexcept;
    void SetWidth(double value) noexcept;
    Ref<DataTemplate>
        GetCellTemplate() const noexcept;
    void SetCellTemplate(Ref<DataTemplate> value) noexcept;
    Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept;
    StringView GetDisplayMemberPath()
        const noexcept;
    void SetDisplayMemberPath(StringView value) noexcept;
    Ref<Aero::Data::Binding>
        GetDisplayMemberBinding() const noexcept;
    void SetDisplayMemberBinding(Ref<Aero::Data::Binding> value) noexcept;
    Ref<Style> GetHeaderContainerStyle() const noexcept {
        return GetValue(HeaderContainerStyleProperty);
    }
    void SetHeaderContainerStyle(Ref<Style> value) noexcept {
        SetValue(HeaderContainerStyleProperty, std::move(value));
    }

    AERO_DEPENDENCY_PROPERTY(Value, Header);
    AERO_DEPENDENCY_PROPERTY(double, Width);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, CellTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, HeaderTemplate);
    AERO_DEPENDENCY_PROPERTY(String, DisplayMemberPath);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Data::Binding>, DisplayMemberBinding);
    AERO_DEPENDENCY_PROPERTY(Ref<Style>, HeaderContainerStyle);
};
} // namespace Aero::Controls
