#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Gui/ResourceDictionary.hpp>
#include <Aero/Value.hpp>

namespace Aero {
using Meta::TypeId;
class AERO_API DataTemplate : public Base::Object {
    AERO_DECLARE_TYPE(DataTemplate, Base::Object)
public:
    struct Impl;

    DataTemplate() noexcept;
    ~DataTemplate() noexcept override;
    DataTemplate(const DataTemplate&) = delete;
    DataTemplate& operator=(const DataTemplate&) = delete;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    TypeId GetDataType() const noexcept;
    void SetDataType(TypeId value) noexcept;
    Base::Ref<Base::Object> GetHierarchicalItemsSource() const noexcept;
    void SetHierarchicalItemsSource(Base::Ref<Base::Object> value) noexcept;
    Base::Ref<Base::Object> GetHierarchicalItemTemplate() const noexcept;
    void SetHierarchicalItemTemplate(Base::Ref<Base::Object> value) noexcept;
    ResourceKey GetImplicitKey() const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct Impl;
    void* state_ = nullptr;
};
} // namespace Aero
