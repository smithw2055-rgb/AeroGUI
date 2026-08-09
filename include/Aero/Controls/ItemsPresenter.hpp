#pragma once

#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {

class AERO_GUI_API ItemsPresenter : public Decorator {
    AERO_DECLARE_TYPE(ItemsPresenter, Decorator)
public:
    ItemsPresenter() noexcept : Decorator(StaticTypeId()) {}
    ~ItemsPresenter() override = default;
    Panel* GetItemsHost() const noexcept;
    void SetItemsHost(
        const Ref<Base::Object>& owner,
        Panel& panel) noexcept;
};

} // namespace Aero::Controls
