#pragma once

#include <Aero/Resources.hpp>

namespace Aero::Controls {

struct ItemsPanelTemplateRuntime;

class AERO_GUI_API ItemsPanelTemplate : public Base::Object {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, Base::Object)
public:

    ItemsPanelTemplate() noexcept;
    ~ItemsPanelTemplate() noexcept override;
    ItemsPanelTemplate(const ItemsPanelTemplate&) = delete;
    ItemsPanelTemplate& operator=(const ItemsPanelTemplate&) = delete;

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Ref<ResourceDictionary> value) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct ItemsPanelTemplateRuntime;
    void* state_ = nullptr;
};

} // namespace Aero::Controls
