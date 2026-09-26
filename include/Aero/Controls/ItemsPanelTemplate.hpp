#pragma once

#include <Aero/FrameworkTemplate.hpp>


namespace Aero::Controls {

class AERO_GUI_API ItemsPanelTemplate : public ::Aero::FrameworkTemplate {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, FrameworkTemplate)
public:
    ItemsPanelTemplate() noexcept;
    ~ItemsPanelTemplate() noexcept override;
    ItemsPanelTemplate(const ItemsPanelTemplate&) = delete;
    ItemsPanelTemplate& operator=(const ItemsPanelTemplate&) = delete;

    ResourceDictionary& GetResources() noexcept override;
    const ResourceDictionary& GetResources() const noexcept override;
    void SetResources(Ref<ResourceDictionary> value) noexcept override;
    bool GetIsSealed() const noexcept override;
};

} // namespace Aero::Controls
