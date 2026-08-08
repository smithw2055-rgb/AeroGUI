#pragma once

#include <Aero/Gui/Decorator.hpp>
#include <Aero/Gui/Panel.hpp>

namespace Aero::Controls {

class AERO_GUI_API ItemsPresenter : public Decorator {
    AERO_DECLARE_TYPE(ItemsPresenter, Decorator)
public:
    ItemsPresenter() noexcept : Decorator(StaticTypeId()) {}
    ~ItemsPresenter() override = default;
    Panel* GetItemsHost() const noexcept;
    void SetItemsHost(
        const Base::Ref<Base::Object>& owner,
        Panel& panel) noexcept;
};

} // namespace Aero::Controls
