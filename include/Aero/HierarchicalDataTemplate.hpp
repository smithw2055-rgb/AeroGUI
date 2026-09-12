#pragma once

#include <Aero/DataTemplate.hpp>

namespace Aero {

class AERO_GUI_API HierarchicalDataTemplate : public DataTemplate {
    AERO_DECLARE_TYPE(HierarchicalDataTemplate, DataTemplate)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    HierarchicalDataTemplate() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Ref<Base::Object> GetItemsSource() const noexcept;
    void SetItemsSource(Ref<Base::Object> value) noexcept;
    Ref<Base::Object> GetItemTemplate() const noexcept;
    void SetItemTemplate(Ref<Base::Object> value) noexcept;
};

} // namespace Aero
