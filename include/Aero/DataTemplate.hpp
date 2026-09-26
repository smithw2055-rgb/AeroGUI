#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Value.hpp>

namespace Aero {
using Meta::TypeId;
namespace Controls { struct FrameworkTemplateState; }

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
    friend struct Controls::FrameworkTemplateState;
    friend class HierarchicalDataTemplate;
    void* state_ = nullptr;
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
