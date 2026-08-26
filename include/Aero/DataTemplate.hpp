#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Value.hpp>

namespace Aero {
using Meta::TypeId;
struct DataTemplateRuntime;

class AERO_GUI_API DataTemplate : public Base::Object {
    AERO_DECLARE_TYPE(DataTemplate, Base::Object)
public:

    DataTemplate() noexcept;
    ~DataTemplate() noexcept override;
    DataTemplate(const DataTemplate&) = delete;
    DataTemplate& operator=(const DataTemplate&) = delete;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    TypeId GetDataType() const noexcept;
    void SetDataType(TypeId value) noexcept;
    ResourceKey GetImplicitKey() const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Ref<ResourceDictionary> value) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct DataTemplateRuntime;
    friend class HierarchicalDataTemplate;
    void* state_ = nullptr;
};
} // namespace Aero
