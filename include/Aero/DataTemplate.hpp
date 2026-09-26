#pragma once

#include <Aero/Base/Ref.hpp>
#include <Aero/FrameworkTemplate.hpp>
#include <Aero/Value.hpp>

namespace Aero {
using Meta::TypeId;

class AERO_GUI_API DataTemplate : public FrameworkTemplate {
    AERO_DECLARE_TYPE(DataTemplate, FrameworkTemplate)
public:
    DataTemplate() noexcept;
    ~DataTemplate() noexcept override;
    DataTemplate(const DataTemplate&) = delete;
    DataTemplate& operator=(const DataTemplate&) = delete;

    TypeId GetDataType() const noexcept;
    void SetDataType(TypeId value) noexcept;
    ResourceKey GetImplicitKey() const noexcept;
    ResourceDictionary& GetResources() noexcept override;
    const ResourceDictionary& GetResources() const noexcept override;
    void SetResources(Ref<ResourceDictionary> value) noexcept override;
    bool GetIsSealed() const noexcept override;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

private:
    friend class HierarchicalDataTemplate;
};

class AERO_GUI_API HierarchicalDataTemplate : public DataTemplate {
    AERO_DECLARE_TYPE(HierarchicalDataTemplate, DataTemplate)
public:
    HierarchicalDataTemplate() noexcept = default;
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    Ref<Base::Object> GetItemsSource() const noexcept;
    void SetItemsSource(Ref<Base::Object> value) noexcept;
    Ref<Base::Object> GetItemTemplate() const noexcept;
    void SetItemTemplate(Ref<Base::Object> value) noexcept;
};

class DependencyObject;

class AERO_GUI_API DataTemplateSelector : public Base::Object {
    AERO_DECLARE_TYPE(DataTemplateSelector, Base::Object)
public:
    DataTemplateSelector() noexcept = default;

    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    virtual Ref<DataTemplate> SelectTemplate(Base::Object* item, DependencyObject* container) noexcept {
        (void)item;
        (void)container;
        return {};
    }
};

} // namespace Aero
