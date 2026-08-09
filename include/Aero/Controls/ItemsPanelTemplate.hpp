#pragma once

#include <Aero/Resources.hpp>

namespace Aero::Controls {

class AERO_GUI_API ItemsPanelTemplate : public Base::Object {
    AERO_DECLARE_TYPE(ItemsPanelTemplate, Base::Object)
#if defined(AERO_GUI_IMPLEMENTATION)
public:
#else
private:
#endif
    struct Access;

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
    friend struct Access;
    void* state_ = nullptr;
};

} // namespace Aero::Controls
