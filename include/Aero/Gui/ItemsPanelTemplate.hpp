#pragma once

#include <Aero/Gui/ResourceDictionary.hpp>

namespace Aero::Controls {

class AERO_API ItemsPanelTemplate : public Base::Object {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, Base::Object)
public:
    struct Impl;

    ItemsPanelTemplate() noexcept;
    ~ItemsPanelTemplate() noexcept override;
    ItemsPanelTemplate(const ItemsPanelTemplate&) = delete;
    ItemsPanelTemplate& operator=(const ItemsPanelTemplate&) = delete;

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct Impl;
    void* state_ = nullptr;
};

} // namespace Aero::Controls
