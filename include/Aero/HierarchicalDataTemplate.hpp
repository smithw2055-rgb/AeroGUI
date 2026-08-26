#pragma once

#include <Aero/DataTemplate.hpp>

namespace Aero {

class AERO_GUI_API HierarchicalDataTemplate : public DataTemplate {
    AERO_DECLARE_TYPE(HierarchicalDataTemplate, DataTemplate)
public:
    HierarchicalDataTemplate() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

} // namespace Aero
